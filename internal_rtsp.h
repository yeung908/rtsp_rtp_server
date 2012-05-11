/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _INTERNAL_RTSP_H_
#define _INTERNAL_RTSP_H_

typedef struct {
    unsigned char *media_uri; /* Uri for the media */
    unsigned int ssrc; /* Use the ssrc to locate the corresponding RTP session */
} INTERNAL_MEDIA;

typedef struct {
    unsigned char *global_uri; /* Global control uri */
    INTERNAL_MEDIA (*medias)[1];
    int n_medias;
} INTERNAL_SOURCE;

typedef struct {
    int Session;
    int CSeq;
    struct sockaddr_storage client_addr;
    INTERNAL_SOURCE (*sources)[1];
    int n_sources;
} INTERNAL_RTSP;

#endif
