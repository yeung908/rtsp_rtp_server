/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parse_sdp.h"



int main () {
    int ret;
    int err = 0;
    SDP sdp;
    char **sdp_ptr;
    char packed_msg[1024];
    char *sdp_ok[] = {
        "m=video 5000 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\r\n\0",
        "a=control:rtsp://uri/cacosa\r\n"
            "m=video 5000 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\r\n\0",
        "a=control:rtsp://uri/cacosa\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\r\n\0",
        "a=control:rtsp://uri/cacosa\r\n"
            "m=video 5000 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n\0",
        0
    };

    char *sdp_err[] = {
        "m=video 5000 asdf/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video 5000 RTP/AVP\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video 5000 RTP/AVP 1\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video 5000 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video 5000 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "m=video 5000 RTP/AVP 1\r\n"
            "a=control:http://uri/cacosa/video\r\n"
            "m=audio 5002 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\0",
        "asldfaklsdajsd\0",
        "\0",
        0
    };

    sdp_ptr = sdp_ok;
    do {
        ret = unpack_sdp(&sdp, (unsigned char *)*sdp_ptr, strlen(*sdp_ptr));
        if (!ret) {
            err = 1;
            fprintf(stderr, "Error unpacking sdp:\n%s\n", *sdp_ptr);
            continue;
        }
        ret = pack_sdp(&sdp, (unsigned char *)packed_msg, 1024);
        if (!ret) {
            err = 1;
            fprintf(stderr, "Error packing sdp:\n%s\n\n", *sdp_ptr);
            continue;
        }

        if (strcmp(*sdp_ptr, packed_msg)) {
            err = 1;
            fprintf(stderr, "Error, different sdp:\n%s\n%s\n\n", *sdp_ptr, packed_msg);
        }

        if (sdp.medias) {
            free(sdp.medias);
            sdp.medias = 0;
        }
    } while (*(++sdp_ptr));

    sdp_ptr = sdp_err;
    do {
        ret = unpack_sdp(&sdp, (unsigned char *)*sdp_ptr, strlen(*sdp_ptr));
        if (ret) {
            err = 1;
            fprintf(stderr, "Unpacked incorrect sdp:\n%s\n\n", *sdp_ptr);
            continue;
        }
        if (sdp.medias) {
            free(sdp.medias);
            sdp.medias = 0;
        }
    } while (*(++sdp_ptr));

    if (!err)
        fprintf(stderr, "Correct tests\n");
    return 0;
}
