/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <stdlib.h>
#include "parse_rtsp.h"
#include "parse_sdp.h"

/* CSeq global variable for auto incrementing it inside the module */
int CSeq = 0;

RTSP_REQUEST *construct_rtsp_request(METHOD method, const unsigned char *uri, int Session, TRANSPORT_CAST cast, PORT client_port) {
    RTSP_REQUEST *req = 0;
    int uri_len = strlen((char *)uri);
    req = malloc(sizeof(RTSP_REQUEST));
    if (!req)
        return(0);
    req->method = method;
    req->uri = malloc(uri_len + 1);
    if (!req->uri) {
        free(req);
        return(0);
    }
    strcpy((char *)req->uri, (char *)uri);
    req->uri[uri_len] = 0;

    req->CSeq = ++CSeq;

    req->Session = Session;

    req->cast = cast;

    req->client_port = client_port;

    return req;
}

RTSP_REQUEST *rtsp_describe(const unsigned char *uri) {
    return construct_rtsp_request(DESCRIBE, uri, -1, UNICAST, 0);
}

RTSP_REQUEST *rtsp_setup(const unsigned char *uri, int Session, TRANSPORT_CAST cast, PORT client_port) {
    return construct_rtsp_request(SETUP, uri, Session, cast, client_port);
}

RTSP_REQUEST *rtsp_play(const unsigned char *uri, int Session) {
    return construct_rtsp_request(PLAY, uri, Session, UNICAST, 0);
}

RTSP_REQUEST *rtsp_pause(const unsigned char *uri, int Session) {
    return construct_rtsp_request(PAUSE, uri, Session, UNICAST, 0);
}

RTSP_REQUEST *rtsp_teardown(const unsigned char *uri, int Session) {
    return construct_rtsp_request(TEARDOWN, uri, Session, UNICAST, 0);
}

RTSP_RESPONSE *construct_rtsp_response(int code, int Session, TRANSPORT_CAST cast, PORT server_port, PORT client_port, int Content_Length, char *content, int options, const RTSP_REQUEST *req) {
    RTSP_RESPONSE *res = 0;

    res = malloc(sizeof(RTSP_RESPONSE));
    if (!res)
        return(0);

    res->code = code;
    res->CSeq = req->CSeq;
    res->Session = Session ? Session : req->Session;
    res->cast = cast ? cast : req->cast;
    res->client_port = client_port ? client_port : req->client_port;
    res->server_port = server_port;
    res->Content_Length = Content_Length;
    if (Content_Length > 0 && content) {
        res->content = malloc(Content_Length + 1);
        if (!res->content)
            return(0);
        memcpy(res->content, content, Content_Length);
        res->content[Content_Length] = 0;
    } else {
        res->Content_Length = 0;
        res->content = 0;
    }

    res->options = options;

    return(res);
}



RTSP_RESPONSE *rtsp_notfound(RTSP_REQUEST *req) {
    return construct_rtsp_response(404, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_servererror(RTSP_REQUEST *req) {
    return construct_rtsp_response(501, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_describe_res(RTSP_REQUEST *req) {
    SDP sdp;
    int uri_len = strlen(req->uri);
    char sdp_str[1024];
    int sdp_len;
    RTSP_RESPONSE *ret;
    /* Hardcoded medias */
    if (strstr(req->uri, "audio") || strstr(req->uri, "video"))
        return(0);
    sdp.n_medias = 2;
    sdp.uri = (unsigned char *)req->uri;
    sdp.medias = malloc(sizeof(MEDIA) * 2);
    if (!sdp.medias)
        return(0);
    sdp.medias[0]->type = AUDIO;
    sdp.medias[0]->port = 0;
    sdp.medias[0]->uri = malloc(uri_len + 8);
    if (!sdp.medias[0]->uri) {
        free(sdp.medias);
        return(0);
    }
    memcpy(sdp.medias[0]->uri, req->uri, uri_len);
    memcpy(sdp.medias[0]->uri + uri_len, "/audio", 7);
    sdp.medias[0]->uri[uri_len + 7] = 0;

    sdp.medias[1]->type = VIDEO;
    sdp.medias[1]->port = 0;
    sdp.medias[1]->uri = malloc(uri_len + 8);
    if (!sdp.medias[1]->uri) {
        free(sdp.medias[0]->uri);
        free(sdp.medias);
        return(0);
    }
    memcpy(sdp.medias[1]->uri, req->uri, uri_len);
    memcpy(sdp.medias[1]->uri + uri_len, "/video", 7);
    sdp.medias[1]->uri[uri_len + 7] = 0;

    if (!(sdp_len = pack_sdp(&sdp, (unsigned char *)sdp_str, 1024))) {
        free(sdp.medias[0]->uri);
        free(sdp.medias[1]->uri);
        free(sdp.medias);
        return(0);
    }
    ret = construct_rtsp_response(200, -1, 0, 0, 0, sdp_len, sdp_str, 0, req);
    free(sdp.medias[0]->uri);
    free(sdp.medias[1]->uri);
    free(sdp.medias);
    return(ret);

}

/* server_port and server_port + 1 must be used for this uri/session */
RTSP_RESPONSE *rtsp_setup_res(RTSP_REQUEST *req, PORT server_port, PORT client_port, TRANSPORT_CAST cast, int Session) {
    if (cast == MULTICAST)
        return construct_rtsp_response(200, Session, cast, server_port, client_port, 0, 0, 0, req);
    else
        return construct_rtsp_response(200, Session, cast, server_port, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_play_res(RTSP_REQUEST *req) {
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_pause_res(RTSP_REQUEST *req) {
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_teardown_res(RTSP_REQUEST *req) {
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_options_res(RTSP_REQUEST *req) {
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 1, req);
}

void free_rtsp_req(RTSP_REQUEST **req) {
    if ((*req)->uri)
        free((*req)->uri);
    free(*req);
    *req = 0;
}
void free_rtsp_res(RTSP_RESPONSE **res) {
    if((*res)->content)
        free((*res)->content);
    free(*res); 
    *res = 0;
}
