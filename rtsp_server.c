/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "common.h"
#include "server.h"
#include "server_client.h"
#include "rtsp_server.h"
#include "internal_rtsp.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashfunction.h"
#include "rtsp.h"
#include "parse_rtsp.h"
#include "servers_comm.h"
#include "strnstr.h"

#define REQ_BUFFER 4096

void *rtsp_worker_fun(void *arg);
void *rtsp_worker_terminator_fun(void *arg);
void *rtp_messenger_fun(void *arg);
RTSP_RESPONSE *rtsp_server_options(WORKER *self, RTSP_REQUEST *req);
RTSP_RESPONSE *rtsp_server_describe(WORKER *self, RTSP_REQUEST *req);
RTSP_RESPONSE *rtsp_server_setup(WORKER *self, RTSP_REQUEST *req, INTERNAL_RTSP *rtsp_info);
RTSP_RESPONSE *rtsp_server_play(WORKER *self, RTSP_REQUEST *req);
RTSP_RESPONSE *rtsp_server_pause(WORKER *self, RTSP_REQUEST *req);
RTSP_RESPONSE *rtsp_server_teardown(WORKER *self, RTSP_REQUEST *req);
int rtp_send_create_unicast_connection(RTP_TO_RTSP *data_from_rtp, char *uri, int Session, struct sockaddr_storage *client_addr);
int get_session(int *ext_session, INTERNAL_RTSP **rtsp_info);
int rtsp_worker_create(int tmp_sockfd, struct sockaddr_storage *client_addr);

/* Port for communication with rtp servers */
unsigned short rtp_comm_port;
/* My ip */
unsigned int my_addr;

void (*signal(int sig, void (*func)(int)))(int);
/* Workers array where the information of each worker will be saved */
WORKER workers[MAX_RTSP_WORKERS][1];
int n_workers;
pthread_mutex_t workers_mutex;

/* Hashtable where the sessions will be stored */
hashtable *session_hash;
pthread_mutex_t hash_mutex;

/* Server socket descriptor */
int sockfd;

/* Pid of RTP process */
pid_t rtp_proc;

/* Pthread that checks for idle worker */
pthread_t worker_terminator;

/* Pthread that gets petitions from RTP */
pthread_t rtp_messenger;

/* Free all resources from the server */
void rtsp_server_stop(int sig) {
    int i;

    /* Close socket */
    if (sockfd != -1) {
        fprintf(stderr, "\n\nClosing main socket ");
        close(sockfd);
        fprintf(stderr, "- closed\n");
    }

    /* Kill all workers */
    fprintf(stderr, "Starting killing workers ");
    pthread_mutex_lock(&workers_mutex);
    for (i = 0; i < MAX_RTSP_WORKERS; ++i) {
        if (workers[i]->used) {
            workers[i]->used = 0;
            pthread_cancel(workers[i]->thread_id);
            pthread_join(workers[i]->thread_id, 0);
            /* Close worker socket */
            close(workers[i]->sockfd);
        }
        fprintf(stderr, ".");
    }
    pthread_mutex_unlock(&workers_mutex);
    fprintf(stderr, "- killed\n");

    /* Send SIGUSR1 to RTP process and wait process */
    if (rtp_proc != -1) {
        fprintf(stderr, "Killing RTP ");
        kill(rtp_proc, SIGUSR1);
        waitpid(rtp_proc, 0, 0);
        fprintf(stderr, "- killed\n");
    }

    /* Kill thread that checks idle workers */
    fprintf(stderr, "Killing threads ");
    pthread_cancel(worker_terminator);
    pthread_join(worker_terminator, 0);

    /* Kill thread that gets petitions from RTP */
    pthread_cancel(rtp_messenger);
    pthread_join(rtp_messenger, 0);
    fprintf(stderr, "- killed\n");

    /* Free session hash. Don't use mutexes because at this moment all the
     * other threads that could be accessing it have been killed */
    if (session_hash) {
        fprintf(stderr, "Deleting session hash ");
        clearhashtable(&session_hash);
        freehashtable(&session_hash);
        fprintf(stderr, "- deleted\n");
    }

    /* Destroy hash mutex */
    fprintf(stderr, "Destroying mutex ");
    pthread_mutex_destroy(&hash_mutex);
    pthread_mutex_destroy(&workers_mutex);
    fprintf(stderr, "- destroyed\n");

    /* Die */
    fprintf(stderr, "Finished\n");
    exit(0);
}

int initialize_rtsp_globals() {
    int i;
    int st;
    srand(time(0));
    /* Initialize globals */
    n_workers = 0;
    session_hash = 0;
    sockfd = -1;
    rtp_proc = -1;

    /* Intialize workers array */
    for (i = 0; i < MAX_RTSP_WORKERS; ++i)
        workers[i]->used = 0;

    signal(SIGINT, rtsp_server_stop);

    /* Initialize hash table */
    session_hash = newhashtable(longhash, longequal, MAX_RTSP_WORKERS * 2, 1);
    if (!session_hash)
        return(0);

    /* Initialize hash table mutex */
    if (pthread_mutex_init(&hash_mutex, 0)) {
        clearhashtable(&session_hash);
        freehashtable(&session_hash);
        return(0);
    }

    /* Initialize workers mutex */
    if (pthread_mutex_init(&workers_mutex, 0)) {
        pthread_mutex_destroy(&hash_mutex);
        clearhashtable(&session_hash);
        freehashtable(&session_hash);
        return(0);
    }

    /* Create thread that checks idle workers */
    st = pthread_create(&worker_terminator, 0, rtsp_worker_terminator_fun, 0);
    if (st)
        kill(getpid(), SIGINT);

    /* Create thread that gets petitions from RTP */
    st = pthread_create(&rtp_messenger, 0, rtp_messenger_fun, 0);
    if (st)
        kill(getpid(), SIGINT);

    return(1);
}
int rtsp_server(PORT port, PORT rtp_port) {
    int st;

    rtp_comm_port = rtp_port;
    st = initialize_rtsp_globals();
    if (!st)
        return(0);

    accept_tcp_requests(port, &sockfd, &my_addr, rtsp_worker_create);
    /* If we reach this point, there has been a severe error. Terminate */
    kill(getpid(), SIGINT);
    return(0);
}

int main(int argc, char **argv) {
    unsigned short rtsp_port = 2000;
    unsigned short rtp_port = 2001;
    int ret;
    if (argc >= 2) {
        ret = atoi(argv[1]);
        if (ret > 1024 && ret < 60000)
            rtsp_port = ret;
    } 
    if (argc == 3) {
        ret = atoi(argv[2]);
        if (ret > 1024 && ret < 60000)
            rtp_port = ret;
    }
    if (rtsp_port == rtp_port) {
        fprintf(stderr, "Ports must be different\n");
        return 0;
    }
    rtsp_server(rtsp_port, rtp_port);
    return(0);
}

void *rtsp_worker_terminator_fun(void *arg) {
    time_t now;
    int i;
    for (;;) {
        /* Each second check if any worker has been idle too much time */
        sleep(1);
        now = time(0);

        pthread_mutex_lock(&workers_mutex);
        for (i = 0; i < MAX_RTSP_WORKERS; ++i) {
            if (workers[i]->used) {
                if (now - workers[i]->time_contacted > MAX_IDLE_TIME) {
                    workers[i]->used = 0;
                    pthread_cancel(workers[i]->thread_id);
                    pthread_join(workers[i]->thread_id, 0);
                    /* Close worker socket */
                    close(workers[i]->sockfd);
                    --n_workers;
                }
            }
        }
        pthread_mutex_unlock(&workers_mutex);
    }
}

void *rtp_messenger_fun(void *arg) {
    /* TODO: Messaging with RTP process */
    /* NOTE: This isn't really necesary for the server to work.
     * It's use would be to get a notification when a RTP stream ends, but that
     * can be handled by the client */
    wait(0);
    return(0);
}

/* Create a new rtsp worker
 * tmp_sockfd: Socket number
 * client_addr: Structure with the information of the client
 * returns 0 on error
 */
int rtsp_worker_create(int tmp_sockfd, struct sockaddr_storage *client_addr) {
    int i;
    int st;
    pthread_mutex_lock(&workers_mutex);
    if (n_workers < MAX_RTSP_WORKERS) {
        /* Find an unused worker */
        for (i = 0; i < MAX_RTSP_WORKERS; ++i)
                if (!workers[i]->used)
                    break;

            /* Create worker */
            st = pthread_create(&workers[i]->thread_id, 0, rtsp_worker_fun, &workers[i]);
            if (st) 
                return(0);

            workers[i]->used = 1;
            /* Save socket info */
            workers[i]->sockfd = tmp_sockfd;
            memcpy(&(workers[i]->client_addr), client_addr, sizeof(struct sockaddr_storage));
            /* Set as accesed now */
            workers[i]->time_contacted = time(0);
            ++n_workers;
        } else {
            /* If the limit of workers has been reached, return error */
            return(0);
        }
        pthread_mutex_unlock(&workers_mutex);
        return(1);
        }
void *rtsp_worker_fun(void *arg) {
    WORKER *self = arg;
    int sockfd;
    char buf[REQ_BUFFER];
    int ret;
    int st;
    RTSP_REQUEST req[1];
    RTSP_RESPONSE *res;
    int server_error;
    int Session;
    INTERNAL_RTSP *rtsp_info = 0;
    int CSeq = 0;

    pthread_mutex_lock(&workers_mutex);
    sockfd = self->sockfd;
    pthread_mutex_unlock(&workers_mutex);

    for(;;) {
        rtsp_info = 0;
        Session = -1;
        /* Wait to get a correct request */
        do {
            server_error = 0;

            st = receive_message(sockfd, buf, REQ_BUFFER);
            if (st == -1)
                return(0);
            else if (!st)
                server_error = 1;
            st = unpack_rtsp_req(req, buf, st);
            /* If there was an error return err*/
            if (server_error && st) {
                fprintf(stderr, "caca1\n");
                res = rtsp_servererror(req);
                if (res) {
                    st = pack_rtsp_res(res, buf, REQ_BUFFER);
                    if (st) {
                        buf[st] = 0;
                        send(sockfd, buf, st, 0);
                    }
                    free_rtsp_res(&res);
                }
            } else if (server_error || !st) {
                memcpy(buf, "RTSP/1.0 500 Internal server error\r\n\r\n", 38);
                buf[38] = 0;
                send(sockfd, buf, strlen(buf), 0);
            }

        } while (server_error || !st);

        /* Correct request */
        /* Process request */

        /* Get or create session */
        st = get_session(&(req->Session), &rtsp_info);

        /* Check that CSeq is incrementing */
        if (req->CSeq <= CSeq) {
            res = rtsp_servererror(req);
        } else {
            CSeq = req->CSeq;
            switch (req->method) {
                case OPTIONS:
                    req->Session = 0;
                    res = rtsp_server_options(self, req);
                    pthread_mutex_lock(&workers_mutex);
                    self->time_contacted = time(0);
                    pthread_mutex_unlock(&workers_mutex);
                    break;
                case DESCRIBE:
                    req->Session = 0;
                    res = rtsp_server_describe(self, req);
                    pthread_mutex_lock(&workers_mutex);
                    self->time_contacted = time(0);
                    pthread_mutex_unlock(&workers_mutex);
                    break;
                case SETUP:
                    res = rtsp_server_setup(self, req, rtsp_info);
                    pthread_mutex_lock(&workers_mutex);
                    self->time_contacted = time(0);
                    pthread_mutex_unlock(&workers_mutex);
                    break;
                case PLAY:
                    fprintf(stderr, "Recibido play\n");
                    if (!rtsp_info) {
                        res = rtsp_servererror(req);
                    } else {
                        res = rtsp_server_play(self, req);
                        pthread_mutex_lock(&workers_mutex);
                        self->time_contacted = time(0);
                        pthread_mutex_unlock(&workers_mutex);
                    }
                    break;
                case PAUSE:
                    if (!rtsp_info) {
                        res = rtsp_servererror(req);
                    } else {
                        res = rtsp_server_pause(self, req);
                        pthread_mutex_lock(&workers_mutex);
                        self->time_contacted = time(0);
                        pthread_mutex_unlock(&workers_mutex);
                    }
                    break;
                case TEARDOWN:
                    if (!rtsp_info) {
                        res = rtsp_servererror(req);
                    } else {
                        res = rtsp_server_teardown(self, req);
                        pthread_mutex_lock(&workers_mutex);
                        self->time_contacted = time(0);
                        pthread_mutex_unlock(&workers_mutex);
                    }
                    break;
                default:
                    fprintf(stderr, "caca2\n");
                    res = rtsp_servererror(req);
                    break;
            }
        }
        if (res) {
            st = pack_rtsp_res(res, buf, REQ_BUFFER);
            if (st) {
                buf[st] = 0;
                write(2, buf, st);
                send(sockfd, buf, st, 0);
            }
            free_rtsp_res(&res);
        }

        if (req->uri)
            free(req->uri);
    }
}

RTSP_RESPONSE *rtsp_server_options(WORKER *self, RTSP_REQUEST *req) {
    return(rtsp_options_res(req));
}


RTSP_RESPONSE *rtsp_server_describe(WORKER *self, RTSP_REQUEST *req) {
    if (1/* TODO: Check if file exists */) {
        return(rtsp_describe_res(req));
    } else {
        return(rtsp_notfound(req));
    }
}


RTSP_RESPONSE *rtsp_server_setup(WORKER *self, RTSP_REQUEST *req, INTERNAL_RTSP *rtsp_info) {
    int i;
    int j;
    int st;
    int global_uri_len;
    char * end_global_uri;
    RTP_TO_RTSP data_from_rtp;
    RTSP_RESPONSE *res;
    int *Session;

    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri)
        end_global_uri = strstr(req->uri, "/video");
    if (!end_global_uri)
        return(rtsp_notfound(req));
    global_uri_len = end_global_uri - req->uri;

    /* Unicast */
    if (req->cast == UNICAST) {
        if (1/* TODO: Check if file exists */) {
            /* Create new rtsp_info */
            if (!rtsp_info) {
                fprintf(stderr, "Creating new session\n");
                rtsp_info = malloc(sizeof(INTERNAL_RTSP));
                if (!rtsp_info)
                    kill(getpid(), SIGINT);

                rtsp_info->n_sources = 0;
                rtsp_info->sources = 0;

                rtsp_info->Session = req->Session;

                pthread_mutex_lock(&workers_mutex);
                memcpy(&(rtsp_info->client_addr), &(self->client_addr), sizeof(struct sockaddr_storage));
                pthread_mutex_unlock(&workers_mutex);

                pthread_mutex_lock(&hash_mutex);
                Session = malloc(sizeof(unsigned int));
                *Session = req->Session;
                st = puthashtable(&session_hash, Session, rtsp_info);
                pthread_mutex_unlock(&hash_mutex);
                if (st)
                    kill(getpid(), SIGINT);
            }

            pthread_mutex_lock(&hash_mutex);
            fprintf(stderr, "Getting session: %d\n", req->Session);
            rtsp_info = gethashtable(&session_hash, &(req->Session));
            /* Check if the session has disappeared for some reason */
            if (!rtsp_info) {
                pthread_mutex_unlock(&hash_mutex);
                fprintf(stderr, "caca3\n");
                return(rtsp_servererror(req));
            }

            /* Check if the global uri already exists */
            for (i = 0; i < rtsp_info->n_sources; ++i)
                if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
                    break;

            /* If it doesn't exist create it */
            if (i == rtsp_info->n_sources) {
                rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->n_sources));
                if (!rtsp_info->sources) {
                    pthread_mutex_unlock(&hash_mutex);
                    fprintf(stderr, "caca4\n");
                    return(rtsp_servererror(req));
                }
                /* Copy global uri */
                rtsp_info->sources[i]->global_uri = malloc (global_uri_len + 1);
                if (!rtsp_info->sources[i]->global_uri) { 
                    pthread_mutex_unlock(&hash_mutex);
                    fprintf(stderr, "caca5\n");
                    return(rtsp_servererror(req));
                }
                memcpy(rtsp_info->sources[i]->global_uri, req->uri, global_uri_len);
                rtsp_info->sources[i]->global_uri[global_uri_len] = 0;
                rtsp_info->sources[i]->medias = 0;
                rtsp_info->sources[i]->n_medias = 0;
            }

            /* Check if the media uri already exists */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                    break;
            /* If it doesn't exist create it */
            if (j == rtsp_info->sources[i]->n_medias) {
                rtsp_info->sources[i]->medias = realloc(rtsp_info->sources[i]->medias, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->sources[i]->n_medias));
                if (!rtsp_info->sources[i]->n_medias) {
                    pthread_mutex_unlock(&hash_mutex);
                    fprintf(stderr, "caca6\n");
                    return(rtsp_servererror(req));
                }
                /* Copy global uri */
                rtsp_info->sources[i]->medias[j]->media_uri = malloc (strlen(req->uri) + 1);
                if (!rtsp_info->sources[i]->medias[j]->media_uri) { 
                    pthread_mutex_unlock(&hash_mutex);
                    fprintf(stderr, "caca7\n");
                    return(rtsp_servererror(req));
                }
                memcpy(rtsp_info->sources[i]->medias[j]->media_uri, req->uri, strlen(req->uri));
                rtsp_info->sources[i]->medias[j]->media_uri[strlen(req->uri)] = 0;

                /* Put the client udp port in the structure */
                ((struct sockaddr_in*)&rtsp_info->client_addr)->sin_port = htons(req->client_port);
                pthread_mutex_unlock(&hash_mutex);
                st = rtp_send_create_unicast_connection(&data_from_rtp, req->uri, rtsp_info->Session, &(rtsp_info->client_addr));
                if (!st) {
                    return(rtsp_servererror(req));
                }
                pthread_mutex_lock(&hash_mutex);
                rtsp_info = gethashtable(&session_hash, &(req->Session));
                /* Check if the session has disappeared for some reason */
                if (!rtsp_info) {
                    pthread_mutex_unlock(&hash_mutex);
                    fprintf(stderr, "caca30\n");
                    return(rtsp_servererror(req));
                }
                /* Assign ssrc */
                rtsp_info->sources[i]->medias[j]->ssrc = data_from_rtp.ssrc; 
            }


            /* TODO: this */
            //        return rtsp_setup_res(req, data_to_rtp->server_port, 0, UNICAST, 0);
            res = rtsp_setup_res(req, data_from_rtp.server_port, 0, UNICAST, 0);
            pthread_mutex_unlock(&hash_mutex);
            return res;
        } else {
            return(rtsp_notfound(req));
        }
    } else {
        /* TODO: Multicast */
        /*create_rtsp_unicast_connection(req->uri);*/
        fprintf(stderr, "caca8\n");
        return(rtsp_servererror(req));
    }
}

RTSP_RESPONSE *server_simple_command(WORKER *self, RTSP_REQUEST *req, RTSP_RESPONSE *(*rtsp_command)(RTSP_REQUEST *), int (*rtp_command)(char *, unsigned int)) {
    char *end_global_uri;
    int global_uri_len;
    int global_uri;
    unsigned int ssrc;
    INTERNAL_RTSP *rtsp_info;
    int i;
    int j;
    int st;
    RTP_TO_RTSP data_to_rtp;

    global_uri = 0;
    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri)
        end_global_uri = strstr(req->uri, "/video");
    if (!end_global_uri) {
        end_global_uri = req->uri + strlen(req->uri);
        global_uri = 1;
    }
    global_uri_len = end_global_uri - req->uri;

    fprintf(stderr, "server_simple_command\n");
    if (1/* TODO: Check if file exists */) {
        pthread_mutex_lock(&hash_mutex);
        rtsp_info = gethashtable(&session_hash, &req->Session);
        /* Check if the session has disappeared for some reason */
        if (!rtsp_info) {
            pthread_mutex_unlock(&hash_mutex);
            fprintf(stderr, "caca9\n");
            return(rtsp_servererror(req));
        }

        /* Get global uri */
        for (i = 0; i < rtsp_info->n_sources; ++i)
            if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
                break;

        /* If it doesn't exist return error*/
        if (i == rtsp_info->n_sources) {
            pthread_mutex_unlock(&hash_mutex);
            fprintf(stderr, "caca10\n");
            return(rtsp_servererror(req));
        }

        /* If the uri isn't global */
        if (!global_uri) {
            /* Get the media uri */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                    break;
            /* If it doesn't exist return error */
            if (j == rtsp_info->sources[i]->n_medias) {
                pthread_mutex_unlock(&hash_mutex);
                fprintf(stderr, "caca11\n");
                return(rtsp_servererror(req));
            }

            /* Apply to only one media */
            ssrc = rtsp_info->sources[i]->medias[j]->ssrc;
            st = rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
            if (!st) {
                pthread_mutex_unlock(&hash_mutex);
                fprintf(stderr, "caca12\n");
                return(rtsp_servererror(req));
            }
        } else {
            /* Apply to all the medias in the global uri */
            fprintf(stderr, "Número de medias: %d\n", rtsp_info->sources[i]->n_medias);
            st = 1;
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                ssrc = rtsp_info->sources[i]->medias[j]->ssrc;
                fprintf(stderr, "El ssrc del media %d es %d\n", j, ssrc);
                /* Try to free all medias */
                if (st)
                    st = rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
                else
                    rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
            }
            if (!st) {
                pthread_mutex_unlock(&hash_mutex);
                fprintf(stderr, "caca13\n");
                return(rtsp_servererror(req));
            }
        }

        pthread_mutex_unlock(&hash_mutex);

        return(rtsp_command(req));
    } else {
        return(rtsp_notfound(req));
    }
}

int send_to_rtp(RTSP_TO_RTP *message) {
    int st;
    int sockfd;
    int rcv_sockfd;
    int rtp_sockfd;
    char *host, *path;
    RTP_TO_RTSP response;
    struct addrinfo hints, *res;
    unsigned short port;
    char port_str[6];
    struct sockaddr rtp_addr;
    int rtp_addr_len = sizeof(struct sockaddr);

    fprintf(stderr, "extrayendo uri: %s\n", message->uri);
    st = extract_uri(message->uri, &host, &path);
    fprintf(stderr, "Status: %d\n", st);
    if (!st)
        return(0);

    /* Open receive socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    do {
        port = rand();
        if (!snprintf(port_str, 5, "%d", port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        rcv_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (rcv_sockfd == -1)
            return(0);

        st = bind(rcv_sockfd, res->ai_addr, res->ai_addrlen);

        port = ((struct sockaddr_in*)(res->ai_addr))->sin_port;
        freeaddrinfo(res);
    } while (st == -1);
    /* Listen in rcv_sockfd */
    st = listen(rcv_sockfd, 1);
    if (st == -1)
        return(0);

    message->response_port = port;

    sockfd = TCP_connect(host, rtp_comm_port);
    if (host)
        free(host);
    if (path)
        free(path);
    if(!sockfd)
        return(0);
    st = send(sockfd, message, sizeof(RTSP_TO_RTP), 0);
    if (!st)
        return(0);
    close(sockfd);

    rtp_sockfd = accept(rcv_sockfd, &rtp_addr, &rtp_addr_len);
    fprintf(stderr, "Valor de rcv_sockfd: %d\n", rcv_sockfd);
    fprintf(stderr, "Valor de rtp_sockfd: %d\n", rtp_sockfd);
    if (rtp_sockfd == -1) {
        perror("error: ");
        return(0);
    }
    st = recv(rtp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
    close(rcv_sockfd);
    close(rtp_sockfd);
    fprintf(stderr, "Status de receive: %d\n", st);
    if (st == -1) {
        perror("error: ");
        return(0);
    }
    fprintf(stderr, "Response order: %d\n", response.order);
    if (response.order == ERR_RTP)
        return(0);
    return(1);
}
int rtp_send_play(char *uri, unsigned int ssrc) {
    RTSP_TO_RTP play_msg;
    RTP_TO_RTSP response;
    play_msg.order = PLAY_RTP;
    play_msg.ssrc = ssrc;
    strcpy(play_msg.uri, uri);

    fprintf(stderr, "rtp_send_play\n");
    return send_to_rtp(&play_msg);
}
RTSP_RESPONSE *rtsp_server_play(WORKER *self, RTSP_REQUEST *req) {
    return(server_simple_command(self, req, rtsp_play_res, rtp_send_play));
}

int rtp_send_pause(char *uri, unsigned int ssrc) {
    RTSP_TO_RTP pause_msg;
    pause_msg.order = PAUSE_RTP;
    pause_msg.ssrc = ssrc;
    strcpy(pause_msg.uri, uri);

    return send_to_rtp(&pause_msg);
}
RTSP_RESPONSE *rtsp_server_pause(WORKER *self, RTSP_REQUEST *req) {
    return(server_simple_command(self, req, rtsp_pause_res, rtp_send_pause));
}

int rtp_send_teardown(char *uri, unsigned int ssrc) {
    RTSP_TO_RTP teardown_msg;
    teardown_msg.order = TEARDOWN_RTP;
    teardown_msg.ssrc = ssrc;
    strcpy(teardown_msg.uri, uri);

    return send_to_rtp(&teardown_msg);
}
RTSP_RESPONSE *rtsp_server_teardown(WORKER *self, RTSP_REQUEST *req) {
    int i, j;
    INTERNAL_RTSP *rtsp_info;
    RTSP_RESPONSE *res;
    char *end_global_uri;
    int global_uri = 0;
    int global_uri_len;

    res = server_simple_command(self, req, rtsp_teardown_res, rtp_send_teardown);

    fprintf(stderr, "Borrando uri: %s\n", req->uri);
    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri)
        end_global_uri = strstr(req->uri, "/video");
    if (!end_global_uri)
        global_uri = 1;
    if (global_uri)
        global_uri_len = strlen(req->uri);
    else
        global_uri_len = end_global_uri - req->uri;

    /* TODO: Borrar media */
    pthread_mutex_lock(&hash_mutex);
    /* Get the session */
    rtsp_info = gethashtable(&session_hash, &(req->Session));
    if (!rtsp_info) {
        pthread_mutex_unlock(&hash_mutex);
        fprintf(stderr, "caca20\n");
        free(res);
        return(rtsp_servererror(req));
    }
    /* Check if the global uri exists */
    for (i = 0; i < rtsp_info->n_sources; ++i)
        if (strlen(rtsp_info->sources[i]->global_uri) == global_uri_len &&
                !memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
            break;
    if (i == rtsp_info->n_sources) {
        pthread_mutex_unlock(&hash_mutex);
        fprintf(stderr, "caca21\n");
        free(res);
        return(rtsp_servererror(req));
    }
    if (global_uri) {
        /* Delete all medias with this uri */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
            free(rtsp_info->sources[i]->medias[j]->media_uri);
        /* Free medias array */
        free(rtsp_info->sources[i]->medias);
        /* Move the other sources */
        memmove(&(rtsp_info->sources[i]), &(rtsp_info->sources[i+1]), rtsp_info->n_sources - i - 1);
        (--rtsp_info->n_sources);
        /* Change size of sources array */
        rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * rtsp_info->n_sources);
    } else {
        /* Check if the media uri exists */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
            if (!strncmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                break;
        if (j == rtsp_info->sources[i]->n_medias) {
            pthread_mutex_unlock(&hash_mutex);
            fprintf(stderr, "caca22\n");
            free(res);
            return(rtsp_servererror(req));
        }
        /* Free this media */
        free(rtsp_info->sources[i]->medias[j]->media_uri);
        /* Move the other medias */
        memmove(&(rtsp_info->sources[i]->medias[j]), &(rtsp_info->sources[i]->medias[j+1]), rtsp_info->sources[i]->n_medias - j - 1);
        --(rtsp_info->sources[i]->n_medias);
        /* Change size of sources array */
        rtsp_info->sources[i]->medias = realloc(rtsp_info->sources[i]->medias, sizeof(INTERNAL_MEDIA) * rtsp_info->sources[i]->n_medias);

    }

    pthread_mutex_unlock(&hash_mutex);

    return res;
}
int rtp_send_create_unicast_connection(RTP_TO_RTSP *data_from_rtp, char *uri, int Session, struct sockaddr_storage *client_addr) {
    RTSP_TO_RTP setup_msg;
    int st;
    int sockfd;
    int rcv_sockfd;
    int rtp_sockfd;
    char *host, *path;
    struct addrinfo hints, *res;
    struct sockaddr rtp_addr;
    unsigned short port;
    char port_str[6];
    int rtp_addr_len = sizeof(struct sockaddr);

    setup_msg.order = SETUP_RTP_UNICAST;
    strcpy(setup_msg.uri, uri);
    setup_msg.Session = Session;
    setup_msg.client_ip = ((struct sockaddr_in *)client_addr)->sin_addr.s_addr;
    setup_msg.client_port = ((struct sockaddr_in *)client_addr)->sin_port;

    /* TODO: Send to rtp server */
    st = extract_uri(setup_msg.uri, &host, &path);
    if (!st || !host || !path)
        return(0);
    fprintf(stderr, "%s\n", host);

    /* Open receive socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    do {
        port = rand();
        if (!snprintf(port_str, 5, "%d", port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        rcv_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (rcv_sockfd == -1)
            return(0);

        st = bind(rcv_sockfd, res->ai_addr, res->ai_addrlen);

        port = ((struct sockaddr_in*)(res->ai_addr))->sin_port;
        freeaddrinfo(res);
    } while (st == -1);
    /* Listen in rcv_sockfd */
    st = listen(rcv_sockfd, 1);
    if (st == -1)
        return(0);

    setup_msg.response_port = port;
    fprintf(stderr, "Listening on port %d\n", port);

    sockfd = TCP_connect(host, rtp_comm_port);
    if (host)
        free(host);
    if (path)
        free(path);
    if(!sockfd)
        return(0);
    st = send(sockfd, &setup_msg, sizeof(RTSP_TO_RTP), 0);
    if (!st)
        return(0);
    close(sockfd);

    /* Open rtp_socket */
    rtp_sockfd = accept(rcv_sockfd, &rtp_addr, &rtp_addr_len);
    st = recv(rtp_sockfd, data_from_rtp, sizeof(RTP_TO_RTSP), 0);

    close(rcv_sockfd);
    close(rtp_sockfd);
    if (!st)
        return(0);
    fprintf(stderr, "order: %d\n", data_from_rtp->order);
    fprintf(stderr, "ssrc recibido: %d\n", data_from_rtp->ssrc);
    if (data_from_rtp->order == ERR_RTP)
        return(0);
    return(1);
}


/* Checks if a session already exists. If it doesn't, create one */
int get_session(int *ext_session, INTERNAL_RTSP **rtsp_info) {
    if (*ext_session > 0) {
        /* Check that the session truly exists */
        pthread_mutex_lock(&hash_mutex);
        *rtsp_info = gethashtable(&session_hash, ext_session);
        pthread_mutex_unlock(&hash_mutex);
        if (*rtsp_info)
            return 1;
    }
    while (*ext_session < 1) {
        *ext_session = rand();
        pthread_mutex_lock(&hash_mutex);
        *rtsp_info = gethashtable(&session_hash, ext_session);
        pthread_mutex_unlock(&hash_mutex);
        if (*rtsp_info)
            *ext_session = -1;
    }

    return 1;
}
