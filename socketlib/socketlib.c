/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>
Copyright (c) 2012, Bartosz Andrzej Zawada <bebour@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "socketlib.h"

int TCP_connect(const char *address, unsigned short port) {
    int i = 0;
    int sockfd = -1;
    struct sockaddr_in addr;
    struct hostent *host;
    struct protoent *TCP = getprotobyname("TCP");

    /* Configuramos las partes conocidas de la dirección */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    bzero(addr.sin_zero, 8);

    /* Obtenemos las IPs del host */
    host = gethostbyname(address);
    if (host == 0 || host->h_addrtype != addr.sin_family)
        return -1;

    /* Se intenta conectar a todas las IPs obtenidas */
    for (i = 0; (host->h_addr_list)[i]; ++i) {

        sockfd = socket(host->h_addrtype, SOCK_STREAM, TCP->p_proto);
        //sockfd = socket(host->h_addrtype, SOCK_STREAM, 0);
        if (sockfd == -1)
            continue;

        /* Copiamos la direccion al struct sockaddr_in */
        memcpy(&(addr.sin_addr), host->h_addr_list[i], host->h_length);

        /* Conectamos con el host destino */
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(sockfd);
            continue;
        }

        return sockfd;
    }

    /* Se devuelve error en caso de no poder establecer conexión */
    return -1;
}

void TCP_disconnect(int sockfd) {
    close(sockfd);
}

int UDP_open_socket() {
    struct protoent *udp_proto = getprotobyname("UDP");
    return socket(AF_INET, SOCK_DGRAM, udp_proto->p_proto);
}

int UDP_get_destination(const char *ip, unsigned short port,
        struct sockaddr_in *dest) {
    struct hostent *host;

    host = gethostbyname(ip);
    if (!host)
        return -1;
    if (host->h_addrtype != AF_INET)
        return -1;
    if (host->h_addr_list[0] == 0)
        return -1;

    dest->sin_family = AF_INET;
    dest->sin_port = htons(port);
    memcpy(&(dest->sin_addr), host->h_addr_list[0], host->h_length);
    bzero(dest->sin_zero, 8);
    return 0;
}

void UDP_disconnect(int sockfd) {
    close(sockfd);
}
