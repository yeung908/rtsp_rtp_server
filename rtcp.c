/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include "rtcp.h"

char *pack_rtcp_sr(unsigned int ssrc, struct timeval ntp_timestamp,
        unsigned int rtp_timestamp, unsigned int packet_count, unsigned long octet_count) {
    char *packet;
    unsigned int tmp;
    /* Reserve memory for the RTCP packet */
    packet = malloc(32*7);
    if (!packet)
        return(0);
    packet[0] = 0x80;
    packet[1] = 0xc8;
    packet[2] = 0x00;
    packet[3] = 0x06;

    tmp = htonl(ssrc);
    memcpy(&(packet[4]), &tmp, 4);

    tmp = htonl(ntp_timestamp.tv_sec);
    memcpy(&(packet[8]), &tmp, 4);
    tmp = htonl(ntp_timestamp.tv_usec);
    memcpy(&(packet[12]), &tmp, 4);

    tmp = htonl(rtp_timestamp);
    memcpy(&(packet[16]), &tmp, 4);

    tmp = htonl(packet_count);
    memcpy(&(packet[16]), &tmp, 4);

    tmp = htonl(octet_count);
    memcpy(&(packet[16]), &tmp, 4);

    return packet;
}
