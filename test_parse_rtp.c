/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parse_rtp.h"

int main() {
    int i;
    int ret;
    RTP_PKG pkg1[1], pkg2[1];
    char pkg[1024];
    
    pkg1->header->seq = 1;
    pkg1->header->timestamp = 100;
    pkg1->header->ssrc = 1000;

    pkg1->data = malloc(100);
    if (!pkg1->data)
        return(0);

    for (i = 0; i < 100; ++i)
        pkg1->data[i] = 'a';
    pkg1->d_size = 100;

    ret = pack_rtp(pkg1, (unsigned char *)pkg, 1024);
    if (!ret) {
        fprintf(stderr, "Error packing\n");
        free(pkg1->data);
        return(0);
    }


    ret = unpack_rtp(pkg2, (unsigned char *)pkg, ret);

    if (!ret) {
        fprintf(stderr, "Error unpacking\n");
        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }
    
    if (pkg1->header->seq != pkg2->header->seq) {
        fprintf(stderr, "Error: Different seq: %d != %d\n", pkg1->header->seq, pkg2->header->seq);
        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }

    if (pkg1->header->timestamp != pkg2->header->timestamp) {
        fprintf(stderr, "Error: Different timestamp\n");
        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }

    if (pkg1->header->ssrc != pkg2->header->ssrc) {
        fprintf(stderr, "Error: Different ssrc\n");
        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }

    if (pkg1->d_size != pkg2->d_size) {
        fprintf(stderr, "Error: Different d_size\n");
        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }

    if (memcmp(pkg1->data, pkg2->data, pkg1->d_size)) {
        fprintf(stderr, "Error: Different data\n");

        for (i = 0; i < 100; ++i)
            fprintf(stderr, "%x ", pkg1->data[i]);
        fprintf(stderr, "\r\n");

        for (i = 0; i < 100; ++i)
            fprintf(stderr, "%x ", pkg2->data[i]);
        fprintf(stderr, "\r\n");

        free(pkg1->data);
        if (pkg2->data)
            free(pkg2->data);
        return(0);
    }

    return(1);
}
