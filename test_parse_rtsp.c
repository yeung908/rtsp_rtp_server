/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parse_rtsp.h"

int main() {
    int st;
    int err = 0;
    RTSP_REQUEST req;
    RTSP_RESPONSE res;
    char **msg_ptr;
    char packed_msg[1024];
    char *msg_ok[] = {
        "DESCRIBE rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 1\r\n"
            "Accept: application/sdp\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Transport: RTP/AVP;unicast;client_port=9000-9001\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Session: 2234234\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001\r\n"
            "\r\n\0",
        "PLAY rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 3\r\n"
            "Session: 123\r\n"
            "\r\n\0",
        "PAUSE rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 4\r\n"
            "Session: 123\r\n"
            "\r\n\0",
        "TEARDOWN rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 4\r\n"
            "Session: 123\r\n"
            "\r\n\0",
        0
    };
    char *msg_err[] = {
        "DESCRIBE rtsp://uri/cacosa HTTP/1.1\r\n"
            "CSeq: 1\r\n"
            "X-Error: Bad protocol\r\n"
            "\r\n\0",
        "GET rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 1\r\n"
            "X-Error: Bad method\r\n"
            "\r\n\0",
        "DESCRIBE ://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 1\r\n"
            "X-Error: Bad uri\r\n"
            "\r\n\0",
        "DESCRIBE rtsp://uri/cacosa RTSP/1.0\r\n"
            "X-Error: Missing CSeq\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "X-Error: Missing Transport in setup\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Transport: unicast;client_port=9000-9001\r\n"
            "X-Error: Missing RTP/AVP in transport\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Transport: RTP/AVP;client_port=9000-9001\r\n"
            "X-Error: Missing cast in setup transport\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 2\r\n"
            "Transport: RTP/AVP;unicast\r\n"
            "X-Error: Missing client_port in setup transport\r\n"
            "\r\n\0",
        "PLAY rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 3\r\n"
            "X-Error: Missing session in play\r\n"
            "\r\n\0",
        "PAUSE rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 4\r\n"
            "X-Error: Missing session in pause\r\n"
            "\r\n\0",
        "TEARDOWN rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 4\r\n"
            "X-Error: Missing session in teardown\r\n"
            "\r\n\0",
        "",
        "\r\n",
        "DESCRIBE",
        "DESCRIBE rtsp://uri/cacosa",
        "DESCRIBE rtsp://uri/cacosa RTSP/1.0",
        "DESCRIBE rtsp://uri/cacosa RTSP/1.0\r\n",
        "DESCRIBE rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 1\r\n"
            "Accept: application/sdp\r\n"
            "X-Error: Missing last \r\n",
        0
    };

    char *res_ok[] = {
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "Transport: RTP/AVP;unicast;client_port=9000-9001;server_port=8000-8001\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "Content-Length: 10\r\n"
            "\r\n"
            "0123456789\0",
        0
    };
    char *res_err[] = {
        "HTTP/1.1 200\r\n"
            "CSeq: 1\r\n"
            "X-Error: Invalid protocol\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "X-Error: Missing CSeq\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "X-Error: Missing empty line\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "Content-Length: 10\r\n"
            "X-Error: Missing empty line before data\r\n"
            "0123456789\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "Transport: RTP/AVP;unicast;client_port=9000-9001;server_port=8000-8001\r\n"
            "Content-Length: 10\r\n"
            "X-Error: Using content and transport in same response\r\n"
            "\r\n"
            "0123456789\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "Content-Length: 10\r\n"
            "X-Error: Having Content-Length but not content\r\n"
            "\r\n\0"
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Session: 1523523\r\n"
            "X-Error: Having content but not content-length\r\n"
            "\r\n"
            "0123456789\0",
        "",
        "RTSP",
        "RTSP/1.0",
        "RTSP/1.0\r\n",
        0
    };

    msg_ptr = msg_ok;
    do {
        st = unpack_rtsp_req(&req, *msg_ptr, strlen(*msg_ptr));
        if (!st) {
            err = 1;
            fprintf(stderr, "Error unpacking request:\n%s\n", *msg_ptr);
            continue;
        }
        st = pack_rtsp_req(&req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", *msg_ptr);
            continue;
        }
        if (strcmp(*msg_ptr, packed_msg)) {
            err = 1;
            fprintf(stderr, "Error, different request:\n%s\n%s\n", *msg_ptr, packed_msg);
        }
        if (req.uri)
            free(req.uri);
    } while (*(++msg_ptr));

    msg_ptr = msg_err;
    do {
        st = unpack_rtsp_req(&req, *msg_ptr, strlen(*msg_ptr));
        if (st) {
            fprintf(stderr, "Unpacked incorrect request:\n%s\n", *msg_ptr);
            continue;
        }
        if (req.uri)
            free(req.uri);
    } while (*(++msg_ptr));

    msg_ptr = res_ok;
    do {
        st = unpack_rtsp_res(&res, *msg_ptr, strlen(*msg_ptr));
        if (!st) {
            fprintf(stderr, "Error unpacking response:\n%s\n", *msg_ptr);
            continue;
        }
        st = pack_rtsp_res(&res, packed_msg, 1024);
        if (!st) {
            fprintf(stderr, "Error packing response:\n%s\n", *msg_ptr);
            continue;
        }
        if (strcmp(*msg_ptr, packed_msg))
            fprintf(stderr, "Error, different responses:\n%s\n%s\n", *msg_ptr, packed_msg);
        if (res.content)
            free(res.content);
    } while (*(++msg_ptr));

    msg_ptr = res_err;
    do {
        st = unpack_rtsp_res(&res, *msg_ptr, strlen(*msg_ptr));
        if (st) {
            fprintf(stderr, "Unpacked incorrect response:\n%s\n", *msg_ptr);
            continue;
        }
        if (res.content)
            free(res.content);
    } while (*(++msg_ptr));

    if (!err)
        fprintf(stderr, "Correct tests\n");
    return 0;
}

        
