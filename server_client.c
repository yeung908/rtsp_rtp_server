/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>
Copyright (c) 2012, Bartosz Andrzej Zawada <bebour@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "common.h"
#include "server_client.h"

/* Receive a message with a final empty line and possibly content
 * returns: 0 on bad message, -1 on closed socket, message length if ok */
int receive_message(int sockfd, char * buf, int buf_size) {
    int ret = 0;
    int read = 0;
    char * tmp = 0;
    size_t tmp_size = 0;
    int content_length = 0;
    FILE * f = 0;

    f = fdopen(sockfd, "r");

    do {
        read = getline(&tmp, &tmp_size, f);
        if (read <= 0) {
            fclose(f);
            free(tmp);
            return(-1);
        }

        if (read+ret >= buf_size) {
            fclose(f);
            free(tmp);
            return(0);
        }

        if (!content_length && !strncmp(tmp, "Content-Length:", 15))
            if (sscanf(tmp, "Content-Length: %d", &content_length) < 1)
                content_length = 0;

        strcpy(buf+ret, tmp);
        ret += read;
    } while (tmp[0] != '\r');

    if (content_length) {
        content_length += ret;
        do {
            read = getline(&tmp, &tmp_size, f);
            if (read <= 0) {
                fclose(f);
                free(tmp);
                return(-1);
            }

            if (read+ret >= buf_size) {
                fclose(f);
                free(tmp);
                return(0);
            }

            memcpy(buf+ret, tmp, read);
            ret += read;
        } while (ret < content_length);
    }

    buf[ret] = 0;
    fprintf(stderr, "\n########## RECEIVED ##########\n%s\n##############################\n", buf);
    free(tmp);
    return ret;
}


int extract_uri(char *uri, char **host, char **path) {
    char *path_start;
    int uri_len;

    *host = 0;
    *path = 0;

    if (!uri)
        return(0);
    uri_len = strlen(uri);
    if (uri_len < 8)
        return(0);
    *host = 0;
    *path = 0;

    if (memcmp(uri, "rtsp://", 7))
        return(0);
    uri += 7;
    uri_len -= 7;

    path_start = strstr(uri, "/");
    if (!path_start) {
        /* If there isn't a path */
        *host = malloc(uri_len + 1);
        if (!*host)
            return(0);
        strcpy(*host, uri);
        (*host)[uri_len] = 0;
        *path = 0;
        return(1);
    } 
    /* If there is a '/' separator */
    if ((path_start - uri) == 0) {
        *host = 0;
        *path = 0;
        return(0);
    }
    /* Copy host */
    *host = malloc(path_start - uri + 1);
    if (!*host) {
        *host = 0;
        *path = 0;
        return(0);
    }
    strncpy(*host, uri, path_start - uri);
    (*host)[path_start - uri] = 0;
    
    /* Check if there is a path after the separator */
    if (uri_len > path_start - uri + 1) {
        *path = malloc(strlen(path_start) + 1);
        if (!*path)
            return(0);
        strcpy(*path, path_start);
        (*path)[strlen(path_start)] = 0;
    }
    return(1);
}

/* Binds two consecutive UDP ports and returns the first port number
 * rtp_sockfd: file descriptor for the rtp socket. -1 if error
 * rtcp_sockfd: file descriptor for the rtcp socket. -1 if error
 * returns: number of the first port or 0 if error
 */
int bind_UDP_ports(int *rtp_sockfd, int *rtcp_sockfd) {
    unsigned short first_port;
    struct addrinfo hints, *res;
    char port_str[6];
    int st;
    int counter = 0;

    do {
        *rtp_sockfd = -1;
        *rtcp_sockfd = -1;
        if (counter == MAX_UDP_BIND_ATTEMPTS)
            return(0);
        first_port = rand();
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        if (!snprintf(port_str, 5, "%d", first_port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        *rtp_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (*rtp_sockfd == -1)
            return(0);
        st = bind(*rtp_sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (st == -1) {
            *rtp_sockfd = -1;
            continue;
        }

        if (!snprintf(port_str, 5, "%d", first_port + 1)) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }
        if (getaddrinfo(0, port_str, &hints, &res)) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }

        *rtcp_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (*rtcp_sockfd == -1) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }
        st = bind(*rtcp_sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (st == -1) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            *rtcp_sockfd = -1;
            continue;
        }
        ++counter;
    } while (*rtp_sockfd == -1);
    return(first_port);
}


/* Puts the thread to sleep
 * thread: Thread which will be put to sleep
 * sec: amount of seconds it will sleep
 * usec: amount of microseconds it will sleep
 * NOTE: It will sleep seconds + nanoseconds.
 */
void time_sleep(int sec, int usec) {
    struct timespec t, tr;
    t.tv_sec = sec;
    t.tv_nsec = usec * 1000;

    nanosleep(&t, &tr);
}
