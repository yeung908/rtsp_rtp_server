/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "server.h"
#include "servers_comm.h"
#include "rtp_server.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashfunction.h"
#include "socketlib/socketlib.h"
#include "server_client.h"
#include "parse_rtp.h"
#include "rtcp.h"

#include <gst/gst.h>
#include <glib.h>

#define MSG_IDENTIFIER 99324
/* Burst size in us */
#define RTP_BURST_TIME 100000000

typedef enum {VIDEO, AUDIO} MEDIA_TYPE;
MEDIA_TYPE media_type;

static GstElement * videodec = NULL, * audiodec = NULL;
/* Gstreamer pipeline */
GstElement *pipeline;
GMainLoop * loop;

unsigned int client_ip;
unsigned short client_port;

int sleeper_pid = -1;
int sleeper_pipe[2];

int play_state = 0;
pthread_mutex_t play_state_mutex;

/* Pipe where gstreamer will write the data */
int media_pipe[2];

int rtp_sockfd = -1;
int rtcp_sockfd = -1;
int rtsp_sockfd = -1;

/* Time when a media sent data for the last time */
struct timeval last_time_sent;
/* Time when the pipeline started sending data */
struct timeval time_play;
/* Time when the pipeline paused sending data */
struct timeval time_pause;

unsigned short comm_port;
struct msgbuf {
    long mtype;
    pid_t pid;
};

/* TODO: Send sockfd, RTSP_TO_RTP structure and rtsp_socket to the message queue if the ssrc exists */
struct msg_to_worker {
    long mtype;
    int sockfd;
    RTSP_TO_RTP message;
    struct sockaddr_storage rtsp_socket;
};

void (*signal(int sig, void(*func)(int)))(int);
void *worker_comm_fun(void *arg);
int rtp_worker_create(int sockfd, struct sockaddr_storage *rtsp_socket);
int check_file_exists(char *path);
int rtp_worker_fun();
gboolean on_pipeline_msg(GstBus * bus, GstMessage * msg, gpointer loop);
void on_pad_added(GstElement * element, GstPad * pad);
char *get_absolute_path(char *path);
void *gstreamer_comm_thread_fun(void *ssrc);
void *gstreamer_loop_thread_fun(void *ssrc);
void rtp_worker_stop_eos(int sig);
void free_worker_process();
void *gstreamer_limitrate_thread_fun(void *arg);
void sleeper_fun();

/* RTP workers */
RTP_WORKER_USE workers[MAX_RTP_WORKERS][1];
int n_workers;
/* Hashtable where the workers will be stored */
hashtable *workers_hash;
pthread_mutex_t workers_mutex;

/* Socket where the RTP server will be receiving data from the RTSP server */
int sockfd;

/* Pthread that will receive messages from workers*/
pthread_t worker_comm;

pthread_t gstreamer_loop_thread; int gstreamer_loop_created = 0;
pthread_t gstreamer_comm_thread; int gstreamer_comm_created = 0;
pthread_t gstreamer_limitrate_thread; int gstreamer_limitrate_created = 0;
pthread_t rtcp_thread; int rtcp_created = 0;
unsigned int my_addr;

int msg_queue;

pid_t main_pid;

void rtp_server_stop(int sig) {
    RTP_WORKER *worker;
    int i;

    /* Close socket */
    if (sockfd != -1) {
        fprintf(stderr, "\n\nRTP - Closing main socket ");
        close(sockfd);
        fprintf(stderr, "- closed\n");
    }
    /* Kill all workers */
    fprintf(stderr, "RTP - Starting killing workers ");
    pthread_mutex_lock(&workers_mutex);
    for (i = 0; i < MAX_RTP_WORKERS; ++i) {
        if (workers[i]->used) {
            workers[i]->used = 0;
            /* kill worker */
            kill(workers[i]->pid, SIGINT);
            waitpid(workers[i]->pid, 0, 0);
            worker = gethashtable(&workers_hash, &workers[i]->ssrc);
            if (worker) {
                delhashtable(&workers_hash, &workers[i]->ssrc);
            }
        }
        fprintf(stderr, ".");
    }
    pthread_mutex_unlock(&workers_mutex);
    fprintf(stderr, "- killed\n");

    /* Kill thread that checks idle workers */
    fprintf(stderr, "RTP - Killing threads ");
    pthread_cancel(worker_comm);
    pthread_join(worker_comm, 0);

    /* Free workers hash. */
    if (workers_hash) {
        fprintf(stderr, "RTP - Deleting workers hash ");
        clearhashtable(&workers_hash);
        freehashtable(&workers_hash);
        fprintf(stderr, "- deleted\n");
    }

    /* Destroy workers mutex */
    fprintf(stderr, "RTP - Destroying mutex ");
    pthread_mutex_destroy(&workers_mutex);
    pthread_mutex_destroy(&play_state_mutex);
    fprintf(stderr, "- destroyed\n");

    /* Free message queue */
    fprintf(stderr, "RTP - Destroying message queue ");
    msgctl(msg_queue, IPC_RMID, 0);
    fprintf(stderr, "destroyed\n");

    /* Die */
    fprintf(stderr, "RTP - Finished\n");
    exit(0);
}

int initialize_rtp_globals() {
    int i;
    int st;
    srand(time(0));
    /* Initialize globals */
    n_workers = 0;
    workers_hash = 0;
    sockfd = -1;

    main_pid = getpid();

    /* Intialize workers array */
    for (i = 0; i < MAX_RTP_WORKERS; ++i)
        workers[i]->used = 0;

    signal(SIGINT, rtp_server_stop);
    signal(SIGUSR1, rtp_worker_stop_eos);

    /* Initialize message queue */
    msg_queue = msgget(MSG_IDENTIFIER/*TODO: Don't hardcode this */, IPC_CREAT /*| IPC_EXCL */| 0700);
    if (msg_queue == -1)
        return(0);
    /* Initialize hash table */
    workers_hash = newhashtable(longhash, longequal, MAX_RTP_WORKERS * 2, 1);
    if (!workers_hash) {
        msgctl(msg_queue, IPC_RMID, 0);
        return(0);
    }

    /* Initialize hash table mutex */
    if (pthread_mutex_init(&workers_mutex, 0)) {
        msgctl(msg_queue, IPC_RMID, 0);
        clearhashtable(&workers_hash);
        freehashtable(&workers_hash);
        return(0);
    }
    if (pthread_mutex_init(&play_state_mutex, 0)) {
        pthread_mutex_destroy(&workers_mutex);
        msgctl(msg_queue, IPC_RMID, 0);
        clearhashtable(&workers_hash);
        freehashtable(&workers_hash);
        return(0);
    }


    /* Create thread that checks idle workers */
    st = pthread_create(&worker_comm, 0, worker_comm_fun, 0);
    if (st)
        kill(getpid(), SIGINT);


    return(1);
}
/* Signal handler for killing workers with ctrl-c*/
void rtp_worker_stop(int sig) {
    free_worker_process();
    kill(getppid(), SIGINT);
    exit(0);
}

/* Signal handler for killing workers on end of stream */
void rtp_worker_stop_eos(int sig) {
    struct msgbuf die_message;
    free_worker_process();
    /* Send die message so this process is waited for */
    die_message.mtype = getppid();
    die_message.pid = getpid();
    msgsnd(msg_queue, &die_message, sizeof(pid_t), 0);
    exit(0);
}

int rtp_server(PORT port) {
    int st;

    comm_port = port;
    st = initialize_rtp_globals();
    if (!st)
        return(0);


    accept_tcp_requests(port, &sockfd, &my_addr, rtp_worker_create);
    /* If we reach this point, there has been a severe error. Terminate */
    kill(getpid(), SIGINT);
    return(0);
}

int main(int argc, char **argv) {
    unsigned short rtp_port = 2001;
    int ret;
    if (argc == 2) {
        ret = atoi(argv[1]);
        if (ret > 1024 && ret < 60000)
            rtp_port = ret;
    } 
    rtp_server(rtp_port);
    return(0);
}

void *worker_comm_fun(void *arg) {
    pid_t pid = getpid();
    pid_t worker_pid;
    struct msgbuf buf;
    int st;
    int i;
    RTP_WORKER *worker;
    /* Reserve the buffer where the child processes will send their pid to be waited for */

    for (;;) {
    /* Wait for messages sent to this pid */
        st = msgrcv(msg_queue, &buf, sizeof(pid_t), pid, 0);

        if (st != -1)
            /* Get worker pid */
            worker_pid = buf.pid;

            pthread_mutex_lock(&workers_mutex);
            for (i = 0; i < MAX_RTP_WORKERS; ++i) {
                if (workers[i]->pid == worker_pid) {
                    /* Delete worker in workers array */
                    workers[i]->used = 0;
                    /* kill worker */
                    kill(workers[i]->pid, SIGUSR1);
                    waitpid(workers[i]->pid, 0, 0);
                    worker = gethashtable(&workers_hash, &workers[i]->ssrc);
                    if (worker) {
                        /* Delete worker in hash table */
                        delhashtable(&workers_hash, &workers[i]->ssrc);
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&workers_mutex);
    }
}

int rtp_worker_create(int sockfd, struct sockaddr_storage *rtsp_socket) {
    int ret;
    RTSP_TO_RTP message;
    RTP_TO_RTSP response;
    RTP_WORKER *worker;
    struct msg_to_worker msg;
    char *host, *path;
    int st;
    pid_t child;
    int i;
    unsigned int *ssrc;
    /* TODO */
    ret = recv(sockfd, &message, sizeof(RTSP_TO_RTP), MSG_WAITALL);
    if (ret != sizeof(RTSP_TO_RTP))
        return -1;
    switch (message.order) {
        case CHECK_EXISTS_RTP:
            /* Extract path from uri */
            st = extract_uri(message.uri, &host, &path);
            if (st &&  host && path) {
                /* Check if the path exists in the computer */
                /* Ignore last /audio or /video */
                path[strlen(path)-6] = 0;
                st = check_file_exists(path);
                if (st)
                    response.order = OK_RTP;
                else
                    response.order = ERR_RTP;
            } else {
                response.order = ERR_RTP;
            }
            free(host);
            free(path);
            ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
            break;
        case SETUP_RTP_UNICAST:
            /* Extract the path */
            st = extract_uri(message.uri, &host, &path);
            if (!st || !host || !path) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                return(0);
            }
            /* Check if the file exists */
            /* Ignore last /audio or /video */
            path[strlen(path)-6] = 0;
            st = check_file_exists(path);
            if (!st) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                free(host);
                free(path);
                return(0);
            }
            free(host);
            free(path);

            /* Create worker process */
            child = fork();
            if (child == 0) {
                rtp_worker_fun();
            } else if (child < 0) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                return(0);
            }

            pthread_mutex_lock(&workers_mutex);
            /* Check if we can have more workers */
            if (n_workers == MAX_RTP_WORKERS) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                kill(child, SIGKILL);
                waitpid(child, 0, 0);
                return(0);
            }
            /* Search for a free worker */
            for (i = 0; i < MAX_RTP_WORKERS; ++i)
                if (workers[i]->used == 0)
                    break;
            /* Intialize RTP_WORKER structure */
            workers[i]->used = 1;
            workers[i]->pid = child;
            /* Create new ssrc */
            do {
                workers[i]->ssrc = rand();
            } while (gethashtable(&workers_hash, &(workers[i]->ssrc)));

            /* Reserve memory for worker */
            worker = malloc(sizeof(RTP_WORKER));
            if (!worker) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                kill(child, SIGKILL);
                waitpid(child, 0, 0);
                return(0);
            }
            /* Initialize worker */
            worker->pid = child;
            /* Insert worker in workers hash */
            ssrc = malloc(sizeof(unsigned int));
            if (!ssrc) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                kill(child, SIGKILL);
                waitpid(child, 0, 0);
                return(0);
            }
            *ssrc = workers[i]->ssrc;
            st = puthashtable(&workers_hash, ssrc, worker);
            if (st) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                kill(child, SIGKILL);
                waitpid(child, 0, 0);
                return(0);
            }
            /* Insert the ssrc in the message */
            message.ssrc = workers[i]->ssrc;

            pthread_mutex_unlock(&workers_mutex);

            /* Fall to default */
        default:
            pthread_mutex_lock(&workers_mutex);
            /* Get the worker pid from the ssrc we got in the message */
            worker = gethashtable(&workers_hash, &(message.ssrc));
            if (!worker) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                return(0);
            }

            /* Create message for worker */
            msg.mtype = worker->pid;
            msg.sockfd = sockfd;
            memcpy(&(msg.message), &message, sizeof(RTSP_TO_RTP));
            memcpy(&(msg.rtsp_socket), rtsp_socket, sizeof(struct sockaddr_storage));

            /* Send to the message to the worker */
            st = msgsnd(msg_queue, &msg, sizeof(struct msg_to_worker) - sizeof(long), 0);
            if (st == -1) {
                response.order = ERR_RTP;
                ret = send(sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                return(0);
            }

            pthread_mutex_unlock(&workers_mutex);
            /* Now the worker must get the message and communicate with the RTSP server. */
            break;
    }
    return(1);
}

char *get_absolute_path(char *path) {
    char *base_dir = 0;
    char *full_dir;
    if (strstr(path, ".."))
        return(0);
    base_dir = getcwd(base_dir, 0);
    if (!base_dir)
        return(0);

    full_dir = malloc(strlen(base_dir) + strlen(path) + 1);
    if (!full_dir) {
        free(base_dir);
        return(0);
    }
    strcpy(full_dir, base_dir);
    strcpy(full_dir + strlen(base_dir), path);
    full_dir[strlen(base_dir) + strlen(path)] = 0;
    free(base_dir);

    return(full_dir);
}
/* 1 if exists, 0 if doesn't exist */
int check_file_exists(char *path) {
    /* Don't accept paths that go up in the directory tree */
    int ret;
    struct stat buf;
    char *full_dir;

    full_dir = get_absolute_path(path);
    if (!full_dir)
        return(0);

    ret = stat(full_dir, &buf);
    free(full_dir);
    if (!ret)
        return(1);
    else
        return(0);
}

int rtp_worker_fun() {
    unsigned short rtp_port;
    unsigned short rtcp_port;
    struct msg_to_worker message;
    struct msgbuf die_message;
    int st;
    RTP_TO_RTSP response;
    char *abs_path = 0, *host = 0, *path = 0, *end_filename;

    /* Signal handler para el worker */
    signal(SIGINT, rtp_worker_stop);

    /* Set paused time */
    gettimeofday(&time_pause, 0);

    /* Abrir cola de mensajes */
    msg_queue = msgget(MSG_IDENTIFIER/*TODO: Don't hardcode this */, IPC_CREAT /*| IPC_EXCL */| 0700);
    if (msg_queue == -1) goto terminate_error;

    message.mtype = getpid();
    /* Wait for SETUP message */
    st = msgrcv(msg_queue, &message, sizeof(struct msg_to_worker) - sizeof(long), getpid(), 0);
    if (st == -1) goto terminate_error;


    /* Change the response port to the one indicated in the message */
    ((struct sockaddr_in*)&(message.rtsp_socket))->sin_port = message.message.response_port;
    /* Open response socket to RTSP server */
    rtsp_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (rtsp_sockfd == -1) goto terminate_error;

    st = connect(rtsp_sockfd, (struct sockaddr *)&(message.rtsp_socket), sizeof(struct sockaddr));
    if (st == -1) goto terminate_error;

    /* Bind two consecutive UDP ports */
    rtp_port =  bind_UDP_ports(&rtp_sockfd, &rtcp_sockfd);
    if (!rtp_port) goto terminate_error;
    rtcp_port = rtp_port + 1;

    /* NOTE: This isn't really necesary for the server to work */
    /* Create socket for rtsp */
/*    rtsp_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (!rtsp_sockfd) goto terminate_error;
    ((struct sockaddr_in *)&message.rtsp_socket)->sin_port = comm_port;
    st = connect(rtsp_sockfd, (struct sockaddr *)&message.rtsp_socket, sizeof(message.rtsp_socket));
    if (st == -1) goto terminate_error; */

    /* TODO: Set media type */
    if (strstr(message.message.uri, "audio"))
        media_type = AUDIO;
    else
        media_type = VIDEO;
    client_ip = message.message.client_ip;
    client_port = message.message.client_port;

    /* Get the absolute path to the file */
    st = extract_uri(message.message.uri, &host, &path);
    free(host);
    host = 0;
    if (!st) goto terminate_error;
    abs_path = get_absolute_path(path);
    free(path);
    path = 0;
    if (!abs_path) goto terminate_error;
    end_filename = strstr(abs_path, "/audio");
    if (!end_filename)
      end_filename = strstr(abs_path, "/video");
    *end_filename = 0;

    /* Initialize gstreamer */
    st = gstreamer_fun(abs_path);
    free(abs_path);
    abs_path = 0;
    if (!st) goto terminate_error;
    /* Set as paused */
    pthread_mutex_lock(&play_state_mutex);

    /* Initialize the pipe for communication with the sleeper process */
    st = pipe(sleeper_pipe);
    if (st == -1) goto terminate_error;
    /* Initialize the process that will make this process sleep to limit the rate */
    sleeper_pid = fork();
    if (sleeper_pid < 0) goto terminate_error;
    else if (sleeper_pid == 0) sleeper_fun();

    /* Initialize gstreamer communication threads, that will send data to the client */
    st = pthread_create(&gstreamer_comm_thread, 0, gstreamer_comm_thread_fun, &message.message.ssrc);
    if (st) goto terminate_error;
    gstreamer_comm_created = 1;

    /* Initialize the thread responsible for limiting the speed of gstreamer */
    st = pthread_create(&gstreamer_limitrate_thread, 0, gstreamer_limitrate_thread_fun, 0);
    if (st) goto terminate_error;
    gstreamer_limitrate_created = 1;

    /* Initialize the thread that will run gstreamer */
    st = pthread_create(&gstreamer_loop_thread, 0, gstreamer_loop_thread_fun, 0);
    if (st) goto terminate_error;
    gstreamer_loop_created = 1;
    /* TODO: Create rtcp thread */

    /* Prepare response */
    response.Session = message.message.Session;
    strcpy(response.uri, message.message.uri); 
    response.ssrc = message.message.ssrc;
    response.order = OK_RTP;
    response.server_port = rtp_port;
    st = send(rtsp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);

    close(rtsp_sockfd);
    rtsp_sockfd = -1;

    for (;;) {
        /* Wait for message */
        st = msgrcv(msg_queue, &message, sizeof(struct msg_to_worker) - sizeof(long), getpid(), 0);
        if (st == -1) goto terminate_error;

        /* Change the response port to the one indicated in the message */
        ((struct sockaddr_in*)&(message.rtsp_socket))->sin_port = message.message.response_port;
        /* Open response socket to RTSP server */
        rtsp_sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (rtsp_sockfd == -1) goto terminate_error;

        st = connect(rtsp_sockfd, (struct sockaddr *)&message.rtsp_socket, sizeof(struct sockaddr));
        if (st == -1) goto terminate_error;

        switch (message.message.order) {
            case PLAY_RTP: {
	        GstStateChangeReturn st_ret;
                fprintf(stderr, "Recibido play en proceso %d\n", getpid());
                /* TODO: Send play command to gstreamer thread */
                /* TODO: goto error if error */
		do {

		  GstState state;
		  GstState pending;

		  fprintf(stderr, "Trying play in process %d\n", getpid());
		  gst_element_set_state(pipeline, GST_STATE_PLAYING);

		  st_ret = gst_element_get_state(pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
		  if (st_ret == GST_STATE_CHANGE_SUCCESS)
		    fprintf(stderr, "Successful play\n");
		  else if (st_ret == GST_STATE_CHANGE_FAILURE)
		    fprintf(stderr, "Error in play\n");
		} while (st_ret == GST_STATE_CHANGE_ASYNC || st_ret == GST_STATE_CHANGE_FAILURE);
		fprintf(stderr, "Play done\n");
		/* Set as playing */
		pthread_mutex_unlock(&play_state_mutex);
                /* Set play time */
                gettimeofday(&time_play, 0);
                response.order = OK_RTP;
                response.server_port = rtp_port;
                st = send(rtsp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                break;
			   }
            case PAUSE_RTP: {
	        GstStateChangeReturn st_ret;
                fprintf(stderr, "Recibido pause en proceso %d\n", getpid());
                /* TODO: Send pause command to gstreamer thread */
                /* TODO: goto error if error */
		/* Set as paused */
                pthread_mutex_lock(&play_state_mutex);
		do {

		  GstState state;
		  GstState pending;

		  fprintf(stderr, "Trying pause in process %d\n", getpid());
		  gst_element_set_state(pipeline, GST_STATE_PAUSED);

		  st_ret = gst_element_get_state(pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
		  if (st_ret == GST_STATE_CHANGE_SUCCESS)
		    fprintf(stderr, "Successful pause\n");
		  else if (st_ret == GST_STATE_CHANGE_FAILURE)
		    fprintf(stderr, "Error in pause\n");
		} while (st_ret == GST_STATE_CHANGE_ASYNC || st_ret == GST_STATE_CHANGE_FAILURE);
                /* Set pause time */
                gettimeofday(&time_pause, 0);
		response.order = OK_RTP;
                response.server_port = rtp_port;
                send(rtsp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                break;
			    }
	    case TEARDOWN_RTP:
                /* NOTE: This isn't really necesary for the server to work */
                /* Send FINISHED_RTP to RTSP server */
                /*response.order = FINISHED_RTP;
                response.Session = message.message.Session;
                strcpy(response.uri, message.message.uri); 
                response.ssrc = message.message.ssrc;
                send(rtsp_sockfd, &response, sizeof(response), 0); */
                /* TODO: goto error if error */
                fprintf(stderr, "Recibido teardown en proceso %d\n", getpid());
                response.order = OK_RTP;
                response.server_port = rtp_port;
                send(rtsp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
                goto terminate;
                break;
            default:
                break;
        }
        close(rtsp_sockfd);
        rtsp_sockfd = -1;
    }

    goto terminate;
terminate_error:
    /* Send ERR_RTP to rtsp socket */
    response.order = ERR_RTP;
    send(rtsp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
terminate:
    free_worker_process();
    /* Send die message so this process is waited for */
    die_message.mtype = getppid();
    die_message.pid = getpid();
    msgsnd(msg_queue, &die_message, sizeof(pid_t), 0);
    kill(getpid(), SIGKILL);
}

void free_worker_process() {
    if (sleeper_pid != -1) {
	kill(sleeper_pid, SIGINT);
	waitpid(sleeper_pid, 0, 0);
    }
    fprintf(stderr, "Killed sleeper process\n");
    close(media_pipe[0]);
    close(media_pipe[1]);
    close(sleeper_pipe[0]);
    close(sleeper_pipe[1]);
    fprintf(stderr, "Closed pipes\n");
    pthread_mutex_unlock(&play_state_mutex);
    pthread_mutex_destroy(&play_state_mutex);
  /* Close sockets */
  if (rtp_sockfd != -1)
    close(rtp_sockfd);
  if (rtcp_sockfd != -1)
    close(rtcp_sockfd);
  if (rtsp_sockfd != -1)
    close(rtsp_sockfd);
  fprintf(stderr, "Closed sockets\n");
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(pipeline));
  fprintf(stderr, "RTP WORKER - Terminated\n");
}

int gstreamer_fun(char *path) {
  GstElement * filesrc, * demuxer, * videoqueue, * audioqueue, * videosink, * audiosink;
  GstElement * audioenc, * audiomuxer, * videomuxer, *videoenc;
  GstBus * bus;
  int st;

  // Inicialización de gstreamer y de gtk
  gst_init(0, 0);
  loop = g_main_loop_new(NULL, FALSE);

  // Inicializacion de todos los elementos de gstreamer que intervendran
  // en la reproduccion
  filesrc = gst_element_factory_make("filesrc", "file-source");
  demuxer = gst_element_factory_make("oggdemux", "ogg-demuxer");
  audioqueue = gst_element_factory_make("queue", "audio-queue");
  audiodec = gst_element_factory_make("vorbisdec", "audio-decoder");
  audioenc = gst_element_factory_make("vorbisenc",  "audio-encoder");
  audiomuxer = gst_element_factory_make("oggmux",  "audio-muxer");
  if (media_type == AUDIO)
    audiosink = gst_element_factory_make("fdsink", "audio-sink");
  else
    audiosink = gst_element_factory_make("fakesink", "audio-sink");
  videoqueue = gst_element_factory_make("queue", "video-queue");
  videodec = gst_element_factory_make("theoradec", "video-decoder");
  videoenc = gst_element_factory_make("theoraenc", "video-encoder");
  videomuxer = gst_element_factory_make("oggmux", "video-muxer");
  if (media_type == VIDEO)
    videosink = gst_element_factory_make ("fdsink", "video-sink");
  else
    videosink = gst_element_factory_make ("fakesink", "video-sink");

  pipeline = gst_pipeline_new("media-player");


  if(! filesrc || ! demuxer || ! audioqueue || ! audiodec || ! audioenc || ! audiomuxer ||
     ! audiosink || ! videoqueue || ! videodec || ! videoenc  || !videomuxer || ! videosink || ! pipeline)
  {
    g_printerr("Error creando elementos gstreamer\n");
    return(0);
  }

  // Se enlazan todos los pipes gstreamer de la misma forma que se
  // haría en la línea de comandos
  g_object_set(G_OBJECT(filesrc), "location", path, NULL);
  st = pipe(media_pipe);
  if (st == -1)
    return(0);
  if (media_type == AUDIO)
    g_object_set(G_OBJECT(audiosink), "fd", media_pipe[1], NULL);
  else
    g_object_set(G_OBJECT(videosink), "fd", media_pipe[1], NULL);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_watch(bus, on_pipeline_msg, loop);
  gst_object_unref(bus);

  gst_bin_add_many(GST_BIN(pipeline), filesrc, demuxer, videodec, audiodec, audioqueue,
		   videoqueue, audioenc, audiomuxer, videoenc, videomuxer, videosink, audiosink, NULL);
  gst_element_link(filesrc, demuxer);
  gst_element_link(videodec, videoqueue);
  gst_element_link(audiodec, audioqueue);

  gst_element_link_many(videoqueue, videoenc, videomuxer, videosink, NULL);
  gst_element_link_many(audioqueue, audioenc, audiomuxer, audiosink, NULL);

  g_signal_connect(demuxer, "pad-added", G_CALLBACK(on_pad_added), NULL);

  gst_element_set_state(pipeline, GST_STATE_PAUSED);

  return(1);
} 

void on_pad_added(GstElement * element, GstPad * pad)
{
  GstCaps * caps;
  GstStructure * str;
  GstPad * targetsink = NULL;

  caps = gst_pad_get_caps(pad);
  g_assert(caps != NULL);
  str = gst_caps_get_structure(caps, 0);
  g_assert(str != NULL);

  /* if the file has video and the media type is video connect it to the pipewriter */
  if(g_strrstr(gst_structure_get_name(str), "video"))
  {
    targetsink = gst_element_get_pad(videodec, "sink");
  }
  /* if the file has audio and the media type is audio connect it to the pipewriter */
  else if(g_strrstr(gst_structure_get_name (str), "audio"))
  {
    targetsink = gst_element_get_pad(audiodec, "sink");
  }

  if (targetsink != 0) {
    gst_pad_link(pad, targetsink);
    gst_object_unref(targetsink);
  }
    gst_caps_unref(caps);
}

gboolean on_pipeline_msg(GstBus * bus, GstMessage * msg, gpointer loop)
{
  gchar  * debug;
  GError * error;;

  switch(GST_MESSAGE_TYPE(msg))
  {
    // Mensaje de finalización del stream
    case GST_MESSAGE_EOS:
      fprintf(stderr, "End of stream\n");
      kill(getpid(), SIGUSR1);
      break;

    // Mensaje que indica que se ha producido un error
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, & error, & debug);
      g_free(debug);

      g_printerr("Error: %s\n", error->message);
      g_error_free(error);

      kill(getpid(), SIGUSR1);
      break;

    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
      g_print("Element %s changed state from %s to %s.\n",
          GST_OBJECT_NAME(msg->src),
          gst_element_state_get_name(old_state),
          gst_element_state_get_name(new_state));
      break;
                                    }

    default:
      break;
  }

  return TRUE;
}

void *gstreamer_loop_thread_fun(void *arg) {
    close(media_pipe[0]);
    g_main_loop_run(loop);
    kill(getpid(), SIGUSR1);
}

void *gstreamer_comm_thread_fun(void *ssrc) {
    char rtp_buffer[RTP_BUFFER_SIZE];
    RTP_PKG rtp_package;
    char rtp_packet[RTP_BUFFER_SIZE + 100];
    struct sockaddr_in dest;
    struct sockaddr_in dest_rtcp;
    int readed;
    int ret;
    int packet_size;
    struct timeval current_time;
    unsigned int elapsed_ms;
    unsigned int packet_count = 1;
    unsigned int octet_count = 0;
    char *rtcp_packet;
    unsigned int last_rtcp_packet = 0;

    rtp_package.d_size = RTP_BUFFER_SIZE;
    rtp_package.data = rtp_buffer;
    rtp_package.header->seq = 1;
    rtp_package.header->ssrc = *((unsigned int *)ssrc);
    close(media_pipe[1]);

    /* Information of the client */
    dest.sin_family = AF_INET;
    dest.sin_port = client_port;
    dest.sin_addr.s_addr = client_ip;
    bzero(dest.sin_zero, 8);

    dest_rtcp.sin_family = AF_INET;
    dest_rtcp.sin_port = htons(ntohs(client_port) + 1);
    dest_rtcp.sin_addr.s_addr = client_ip;
    bzero(dest_rtcp.sin_zero, 8);

    /* Initialize last time sent */
    gettimeofday(&last_time_sent, 0);
    elapsed_ms = 0;
    for (;;) {
        readed = 0;
        do {
	  pthread_mutex_lock(&play_state_mutex);
	    ret = read(media_pipe[0], rtp_buffer + readed, RTP_BUFFER_SIZE - readed);
	    if (ret > 0)
	      readed += ret;
	  pthread_mutex_unlock(&play_state_mutex);
        } while (readed != RTP_BUFFER_SIZE);
        ++rtp_package.header->seq;
	++packet_count;
	octet_count += readed;
        /* Insert timestamp */

        gettimeofday(&current_time, 0);
        /* Check if the media was paused */
        if (last_time_sent.tv_sec < time_pause.tv_sec ||
            last_time_sent.tv_sec == time_pause.tv_sec && last_time_sent.tv_usec < time_pause.tv_usec)
            /* Get current time */
            /* Only measure time when the pipeline was generating content */
            elapsed_ms +=
              ((time_pause.tv_sec*1000 + time_pause.tv_usec/1000) -
               (last_time_sent.tv_sec*1000 + last_time_sent.tv_usec/1000)) + /* time from last sent to paused */
              ((current_time.tv_sec * 1000 + current_time.tv_usec/1000) -
               (time_play.tv_sec*1000 + time_play.tv_usec/1000)); /* time from play to current time */
        else 
            elapsed_ms +=
              (current_time.tv_sec * 1000 + current_time.tv_usec/1000) -
              (last_time_sent.tv_sec * 1000 + last_time_sent.tv_usec/1000);

        rtp_package.header->timestamp = elapsed_ms;
        ++rtp_package.header->seq;

        packet_size = pack_rtp(&rtp_package, rtp_packet, RTP_BUFFER_SIZE + 100);
        sendto(rtp_sockfd, rtp_packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));

	/* Send rtcp SR packet as 2% of the connection (every 10976 bytes of rtp) */
	if (octet_count % 10976 > last_rtcp_packet) {
	    ++last_rtcp_packet;
	    rtcp_packet = pack_rtcp_sr(rtp_package.header->ssrc, current_time,
		elapsed_ms, packet_count, octet_count);
	    sendto(rtcp_sockfd, rtcp_packet, 32*7, 0, (struct sockaddr *)&dest_rtcp, sizeof(struct sockaddr_in));
	    free(rtcp_packet);

	}

    }
}

void *gstreamer_limitrate_thread_fun(void *arg) {
    GstFormat fmt = GST_FORMAT_TIME;
    gint64 pos;
    gint64 len;
    gint64 last_played = 0;
    gint64 sleep_time;
    struct timespec sleep_nano;
    close(sleeper_pipe[0]);
    pid_t mypid = getpid();
    for (;;) {
	/* Sleep for the burst time */
        sleep_nano.tv_sec = 0;
	sleep_nano.tv_nsec = RTP_BURST_TIME;
	nanosleep(&sleep_nano, 0);
        pthread_mutex_lock(&play_state_mutex);
	/* Get current position in the stream in nanoseconds */
	gst_element_query_position(pipeline, &fmt, &pos);
	gst_element_query_duration(pipeline, &fmt, &len);
	//gst_element_set_state(pipeline, GST_STATE_PAUSED);
	if (media_type == AUDIO)
	  fprintf(stderr, "audio position %Lu : %Lu\n", pos, len);
	else
	  fprintf(stderr, "video position %Lu : %Lu\n", pos, len);
	/* Get the sleep time in nanoseconds */
	sleep_time = pos - last_played - RTP_BURST_TIME;
	if (sleep_time < 0) sleep_time = 0;
	sleep_nano.tv_sec = sleep_time / 1000000000;
	sleep_nano.tv_nsec = sleep_time % 1000000000;

	/* Send a message to the sleeper process so he can wake up this process */
	if (sleep_time) {
	  write(sleeper_pipe[1], &sleep_nano, sizeof(struct timespec));
	  /* Stop self */
	  kill(mypid, SIGSTOP);
	}

	last_played = pos;

          //gst_element_set_state(pipeline, GST_STATE_PLAYING);
        pthread_mutex_unlock(&play_state_mutex);
  }
}

void sleeper_stop(int signal) {
    close(sleeper_pipe[0]);
    exit(0);
}

void sleeper_fun() {
    pid_t sleeper = getppid();
    struct timespec sleep_nano;
    signal(SIGINT, sleeper_stop);
    close(sleeper_pipe[1]);
    for (;;) {
	read(sleeper_pipe[0], &sleep_nano, sizeof(struct timespec));
	nanosleep(&sleep_nano, 0);
	kill(sleeper, SIGCONT);
    }
}
