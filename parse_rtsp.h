/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _PARSE_RTSP_H_
#define _PARSE_RTSP_H_
#include "common.h"

typedef enum {UNICAST = 0, MULTICAST} TRANSPORT_CAST;

typedef enum {ACCEPT_STR = 0, CONTENT_TYPE_STR, CONTENT_LENGTH_STR, CSEQ_STR, SESSION_STR, TRANSPORT_STR} ATTR;


typedef enum {DESCRIBE = 0, PLAY, PAUSE, SETUP, TEARDOWN, OPTIONS} METHOD;

typedef struct {
    METHOD method;
    char *uri; /* Memory reserved in unpack_rtsp_req */
    int CSeq;
    int Session;
    TRANSPORT_CAST cast;
    PORT client_port;
} RTSP_REQUEST;

typedef struct {
    int code;
    int CSeq;
    int Session;
    TRANSPORT_CAST cast;
    PORT client_port;
    PORT server_port;
    int Content_Length;
    char *content;
    int options;
} RTSP_RESPONSE;

int unpack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size);

int detect_method(char *tok_start, int text_size);

int detect_attr_req(RTSP_REQUEST *req, char *tok_start, int text_size);

/*
 * Pack the structure RTSP_REQUEST in a string
 * req: request structure
 * req_text: String where the request will be packed
 * text_size: Length of req_text
 * return: number of characters written. 0 is error
 */
int pack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size);

int unpack_rtsp_res2(RTSP_RESPONSE **res, char *res_text, int text_size);
int unpack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size);
int pack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size);

int detect_attr_res(RTSP_RESPONSE *res, char *tok_start, int text_size);

int check_uri(char *uri);

#endif
