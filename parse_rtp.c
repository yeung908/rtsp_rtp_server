/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "parse_rtp.h"

const unsigned char start_pkg[] = {128, 0};

/*
 * return: Size of packet. 0 is error
 */
int pack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size) {
    unsigned short num_s;
    unsigned long num_l;
    if (pkg_max_size < RTP_MIN_SIZE || pkg->d_size > pkg_max_size - RTP_MIN_SIZE)
        return(0);

    /* Set header */
    memcpy(packet, start_pkg, 2);
    packet += 2;
    num_s = htons(pkg->header->seq);
    memcpy(packet, &num_s, 2);
    packet += 2;
    num_l = htonl(pkg->header->timestamp);
    memcpy(packet, &num_l, 4);
    packet += 4;
    num_l = htonl(pkg->header->ssrc);
    memcpy(packet, &num_l, 4);
    packet += 4;

    /* Copy data */
    memcpy(packet, pkg->data, pkg->d_size);

    return(RTP_MIN_SIZE + pkg->d_size);
}

/*
 * return: Size of data. 0 if error
 */
int unpack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size) {
    unsigned short num_s;
    unsigned long num_l;
    if (pkg_max_size < RTP_MIN_SIZE)
        return(0);

    /* Check header */
    if (memcmp(packet, start_pkg, 2))
        return(0);
    packet += 2;
    memcpy(&num_s, packet, 2);
    pkg->header->seq = ntohs(num_s);
    packet += 2;
    memcpy(&num_l, packet, 4);
    pkg->header->timestamp = htonl(num_l);
    packet += 4;
    memcpy(&num_l, packet, 4);
    pkg->header->ssrc = ntohl(num_l);
    packet += 4;

    pkg_max_size -= RTP_MIN_SIZE;

    pkg->d_size = pkg_max_size;
    pkg->data = malloc(pkg_max_size);

    /* Copy data */
    memcpy(pkg->data, packet, pkg_max_size);

    return(pkg_max_size);
}
