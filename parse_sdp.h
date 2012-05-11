/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _PARSE_SDP_H
#define _PARSE_SDP_H

#include "common.h"

#define N_MEDIA_TYPE 2

/* No dar valores especificos a los MEDIA_TYPE */
typedef enum {AUDIO = 0, VIDEO} MEDIA_TYPE;

typedef struct {
    MEDIA_TYPE type;
    PORT port;
    unsigned char *uri;
} MEDIA;

typedef struct {
    unsigned char *uri;
    MEDIA (*medias)[1];
    int n_medias;
} SDP;

int pack_sdp(SDP *sdp, unsigned char *sdp_text, int sdp_max_size);
int unpack_sdp(SDP *sdp, unsigned char *sdp_text, int sdp_size);
int unpack_sdp2(SDP **sdp, unsigned char *sdp_text, int sdp_size);

void free_sdp(SDP **sdp);

#endif
