/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include "parse_rtsp.h"
#include "rtsp.h"

int main() {
    int st;
    int err = 0;
    RTSP_REQUEST *req = 0;
    RTSP_RESPONSE *res = 0;
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
            "CSeq: 3\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001\r\n"
            "\r\n\0",
        "SETUP rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 4\r\n"
            "Session: 10\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001\r\n"
            "\r\n\0",
        "PLAY rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 5\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        "PAUSE rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 6\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        "TEARDOWN rtsp://uri/cacosa RTSP/1.0\r\n"
            "CSeq: 7\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        0
    };

    char *res_ok[] = {
        "RTSP/1.0 200\r\n"
            "CSeq: 1\r\n"
            "Content-Length: 141\r\n"
            "\r\n"
            "a=control:rtsp://uri/cacosa\r\n"
            "m=audio 0 RTP/AVP 0\r\n"
            "a=control:rtsp://uri/cacosa/audio\r\n"
            "m=video 0 RTP/AVP 1\r\n"
            "a=control:rtsp://uri/cacosa/video\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 2\r\n"
            "Transport: RTP/AVP;unicast;client_port=9000-9001;server_port=9000-9001\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 3\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001;server_port=9000-9001\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 4\r\n"
            "Session: 10\r\n"
            "Transport: RTP/AVP;multicast;client_port=9000-9001;server_port=9000-9001\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 5\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 6\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        "RTSP/1.0 200\r\n"
            "CSeq: 7\r\n"
            "Session: 10\r\n"
            "\r\n\0",
        0
    };

    req = rtsp_describe((unsigned char *)"rtsp://uri/cacosa");
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating describe: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[0]);
        } else {
            if (strcmp(msg_ok[0], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[0], packed_msg);
            } else {
                res = rtsp_describe_res(req);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating describe respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing describe response:\n%s\n", msg_ok[0]);
                    } else {
                        if (strcmp(res_ok[0], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[0], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    req = 0;
    res = 0;

    req = rtsp_setup((unsigned char *)"rtsp://uri/cacosa", -1, UNICAST, 9000);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating setup unicast without session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[1]);
        } else {
            if (strcmp(msg_ok[1], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[1], packed_msg);
            } else {
                res = rtsp_setup_res(req, 9000, 0, 0, 0);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating setup unicast without session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing setup unicast without session response:\n%s\n", msg_ok[1]);
                    } else {
                        if (strcmp(res_ok[1], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[1], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    res = 0;
    req = 0;

    req = rtsp_setup((unsigned char *)"rtsp://uri/cacosa", -1, MULTICAST, 9000);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating setup multicast without session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[2]);
        } else {
            if (strcmp(msg_ok[2], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[2], packed_msg);
            } else {
                res = rtsp_setup_res(req, 9000, 0, 0, 0);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating setup multicast without session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing setup multicast without session response:\n%s\n", msg_ok[2]);
                    } else {
                        if (strcmp(res_ok[2], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[2], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    res = 0;
    req = 0;

    req = rtsp_setup((unsigned char *)"rtsp://uri/cacosa", 10, MULTICAST, 9000);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating setup multicast with session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[3]);
        } else {
            if (strcmp(msg_ok[3], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[3], packed_msg);
            } else {
                res = rtsp_setup_res(req, 9000, 0, 0, 0);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating setup multicast with session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing setup multicast with session response:\n%s\n", msg_ok[3]);
                    } else {
                        if (strcmp(res_ok[3], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[3], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    res = 0;
    req = 0;

    req = rtsp_play((unsigned char *)"rtsp://uri/cacosa", 10);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating play with session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[4]);
        } else {
            if (strcmp(msg_ok[4], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[4], packed_msg);
            } else {
                res = rtsp_play_res(req);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating play with session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing play with session response:\n%s\n", msg_ok[4]);
                    } else {
                        if (strcmp(res_ok[4], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[4], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    res = 0;
    req = 0;

    req = rtsp_pause((unsigned char *)"rtsp://uri/cacosa", 10);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating pause with session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[5]);
        } else {
            if (strcmp(msg_ok[5], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[5], packed_msg);
            } else {
                res = rtsp_pause_res(req);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating pause with session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing pause with session response:\n%s\n", msg_ok[5]);
                    } else {
                        if (strcmp(res_ok[5], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[5], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);

    res = 0;
    req = 0;

    req = rtsp_teardown((unsigned char *)"rtsp://uri/cacosa", 10);
    if (!req) {
        err = 1;
        fprintf(stderr, "Error creating teardown with session: rtsp://uri/cacosa\n");
    } else {
        st = pack_rtsp_req(req, packed_msg, 1024);
        if (!st) {
            err = 1;
            fprintf(stderr, "Error packing request:\n%s\n", msg_ok[6]);
        } else {
            if (strcmp(msg_ok[6], packed_msg)) {
                err = 1;
                fprintf(stderr, "Error, different request:\n%s\n%s\n", msg_ok[6], packed_msg);
            } else {
                res = rtsp_teardown_res(req);
                if (!res) {
                    err = 1;
                    fprintf(stderr, "Error creating teardown with session respose: rtsp://uri/cacosa\n");
                } else {
                    st = pack_rtsp_res(res, packed_msg, 1024);
                    if (!st) {
                        err = 1;
                        fprintf(stderr, "Error packing teardown with session response:\n%s\n", msg_ok[6]);
                    } else {
                        if (strcmp(res_ok[6], packed_msg)) {
                            err = 1;
                            fprintf(stderr, "Error, different response:\n%s\n%s\n", res_ok[6], packed_msg);
                        }
                    }
                }
            }
        }
    }
    if (res)
        free_rtsp_res(&res);
    if (req)
        free_rtsp_req(&req);
    return 0;
}
