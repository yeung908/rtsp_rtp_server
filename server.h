/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _SERVER_H_
#define _SERVER_H_

#include <netdb.h>
#include <sys/types.h>
#include "common.h"
/* Prototype for the function that creates workers
 * 1st parameter: Int where the socket will be passed
 * 2nd parameter: Structure with information of the client
 */
typedef int (*WORKER_CREATOR)(int, struct sockaddr_storage*);

/* Server loop that accepts requests y creates workers
 * port: Port where the server is listening
 * sockfd: Variable to save the socket
 * my_addr: Variable to save the server address
 * create_worker: Function that creates the new worker
 * return: 0 if error
 */
int accept_tcp_requests(PORT port, int *sockfd, unsigned int *my_addr, WORKER_CREATOR create_worker);
#endif
