/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#include <unistd.h>
#include <pthread.h>

#define MAX_RTSP_WORKERS 20 /* Number of processes listening for rtsp connections */
#define MAX_IDLE_TIME 60 /* Number of seconds a worker can be idle before is killed */

typedef struct {
    int used;
    pthread_t thread_id;
    time_t time_contacted;
    int sockfd;
    struct sockaddr_storage client_addr;
} WORKER;

#endif
