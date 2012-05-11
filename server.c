/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include "common.h"

#define MAX_QUEUE_SIZE 20
typedef int (*WORKER_CREATOR)(int, struct sockaddr_storage*);

int accept_tcp_requests(PORT port, int *sockfd, unsigned int *my_addr, WORKER_CREATOR create_worker) {
    int st;
    struct addrinfo hints, *res;
    char port_str[6];
    int tmp_sockfd;
    struct sockaddr_storage client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    /* Listen incoming connections */
    /* Code taken from http://beej.us/guide/bgnet */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (!snprintf(port_str, 5, "%d", port))
        return(0);
    if (getaddrinfo(0, port_str, &hints, &res))
        return(0);

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1)
        return(0);

    st = bind(*sockfd, res->ai_addr, res->ai_addrlen);
    /* Copy server address to my_addr */
    *my_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    if (st == -1) {
        *sockfd = -1;
        return(0);
    }

    st = listen(*sockfd, MAX_QUEUE_SIZE);
    if (st == -1)
        return(0);

    /* Server loop */
    for (;;) {
        /* Accept */
        tmp_sockfd = accept(*sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (tmp_sockfd == -1)
            return(0);
        /* Create worker */
        st = create_worker(tmp_sockfd, &client_addr);
        if (!st)
            close(tmp_sockfd);
    }
}

