/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "parse_rtsp.h"
#include "strnstr.h"

const int N_CAST = 2;
const char *CAST_STR[] = {"unicast\0", "multicast\0"};

const int N_ATTR = 6;
const char *ATTR_STR[] = {"Accept\0", "Content-Type\0", "Content-Length\0", "CSeq\0", "Session\0", "Transport\0"};

const int N_METHODS = 6;
const char *METHOD_STR[] = {"DESCRIBE\0", "PLAY\0", "PAUSE\0", "SETUP\0", "TEARDOWN\0", "OPTIONS\0"};

const char *RTSP_STR = "RTSP/1.0\0";
const char *RTSP_URI = "rtsp://\0";
const char *SDP_STR = "application/sdp\0";
const char *RTP_STR = "RTP/AVP\0";
const char *CLIENT_PORT_STR = "client_port=\0";
const char *SERVER_PORT_STR = "server_port=\0";
const char *OPTIONS_STR = "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\0";

const char *OK_STR = "OK\0";
const char *SERVERERROR_STR = "Internal server error\0";
const char *NOTFOUND_STR = "Not found\0";
const char *NULL_STR = "\0";
int unpack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size) {
    char *tok_start;
    int tok_len;
    int attr;
    tok_start = req_text;

    /* Initialize structure */
    req->method = -1;
    req->uri = 0;
    req->CSeq = -1;
    req->Session = -1;
    req->client_port = 0;

    /* Get method token */
    tok_len = strcspn(tok_start, " ");
    if (tok_len == text_size)
        return(0);

    /* Discover method */
    req->method = detect_method(tok_start, text_size);
    if (req->method == -1)
        return(0);
    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    /* Get uri token */
    tok_len = strcspn(tok_start, " ");
    if (tok_len == text_size)
        return(0);
    /* Reserve memory for uri */
    req->uri = malloc(tok_len + 1);
    if (!req->uri)
        return(0);
    /* Copy uri */
    memcpy(req->uri, tok_start, tok_len);
    req->uri[tok_len] = 0;
    if (!check_uri(req->uri))
        return(0);
    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    /* Get rtsp token */
    tok_len = strcspn(tok_start, "\r\n");
    if (tok_len == text_size) {
        free(req->uri);
        req->uri = 0;
        return(0);
    }
    /* Check if the rtsp token is valid */
    if (strncmp(tok_start, RTSP_STR, 8)) {
        free(req->uri);
        req->uri = 0;
        return(0);
    }
    /* Ignore next \r or \n */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;
    if (*tok_start == '\r' || *tok_start == '\n') {
        ++tok_start;
        --text_size;
    }

    /* Get Attributes until we find an empty line */
    while ( (tok_len = strcspn(tok_start, "\r\n")) ) {
        if (tok_len == text_size) {
            free(req->uri);
            req->uri = 0;
            return(0);
        }
        /* Get all the attributes */
        attr = detect_attr_req(req, tok_start, text_size);
        if (!attr) {
            free(req->uri);
            req->uri = 0;
            return(0);
        }
        tok_start += tok_len + 1;
        text_size -= tok_len + 1;
        if (*tok_start == '\r' || *tok_start == '\n') {
            ++tok_start;
            --text_size;
        }
    }
    /* If text_size is 0, a last \r\n wasn't received */
    if (text_size == 0)
        return(0);
    /* Obligatory */
    if (req->method == -1)
        return(0);
    /* Obligatory */
    if (req->uri == 0)
        return(0);
    /* Obligatory */
    if (req->CSeq == -1) {
        free(req->uri);
        req->uri = 0;
        return(0);
    }
    /* Session must be present if the method is PLAY, PAUSE or TEARDOWN */
    if (req->Session == -1 && (req->method == PLAY || req->method == PAUSE || req->method == TEARDOWN)) {
        free(req->uri);
        req->uri = 0;
        return(0);
    }
    /* client_port must be present if the method is SETUP */
    if (req->client_port == 0 && req->method == SETUP) {
        free(req->uri);
        req->uri = 0;
        return(0);
    }

    return(1);
}

int detect_method(char *tok_start, int text_size) {
    int i;
    int method_len;
    method_len = strcspn(tok_start, " ");
    if (method_len == text_size || method_len == 0)
        return(-1);
    /* Discover attribute */
    for (i = 0; i < N_METHODS; ++i) {
        if (!memcmp(METHOD_STR[i], tok_start, method_len))
            return(i);
    }
    return(-1);
}

int detect_attr_req(RTSP_REQUEST *req, char *tok_start, int text_size) {
    int i;
    int attr = -1;
    int attr_len;
    attr_len = strcspn(tok_start, ":");
    if (attr_len == text_size || attr_len == 0)
        return(0);
    /* Discover attribute */
    for (i = 0; i < N_ATTR; ++i) {
        if (!memcmp(ATTR_STR[i], tok_start, attr_len)) {
            attr = i;
            break;
        }
    }
    tok_start += attr_len;
    text_size -= attr_len - 1;
    /* Ignore spaces after ':' */
    while (*(++tok_start) == ' ')
        --text_size;

    attr_len = strcspn(tok_start, "\r\n");
    if (attr_len == text_size || attr_len == 0)
        return(0);

    switch (attr) {
        case ACCEPT_STR:
            /* Only can send SDP */
            if (!strnstr(tok_start, SDP_STR, attr_len))
                return(0);
            break;
        case CSEQ_STR:
            req->CSeq = atoi(tok_start);
            break;
        case SESSION_STR:
            req->Session = atoi(tok_start);
            break;
        case TRANSPORT_STR:
            /* The only acceptable transport is RTP */
            if (!strnstr(tok_start, RTP_STR, attr_len))
                return(0);
            /* Check if the transport is unicast or multicast */
            if (strnstr(tok_start, CAST_STR[UNICAST], attr_len))
                req->cast = UNICAST;
            else if (strnstr(tok_start, CAST_STR[MULTICAST], attr_len))
                req->cast = MULTICAST;
            else
                return(0);
            /* Get the client ports */
            if ( (tok_start = strnstr(tok_start, CLIENT_PORT_STR, attr_len)) ) {
                if (!tok_start)
                    return(0);
                tok_start += strlen(CLIENT_PORT_STR);
                req->client_port = (PORT)atoi(tok_start);
                if (req->client_port == 0)
                    return(0);
            }
            break;
        default:
            return(1);
    }
    return(1);
}

/*
 * Pack the structure RTSP_REQUEST in a string
 * req: request structure
 * req_text: String where the request will be packed
 * text_size: Length of req_text
 * return: number of characters written. 0 is error
 */
int pack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size) {
    int ret;
    int written;

    /* Save space for the last \0 */
    --text_size;
    req_text[text_size] = 0;

    /* Write method and uri */
    if (req->method == -1 || !req->uri)
        return(0);
    ret = written = snprintf(req_text, text_size, "%s %s RTSP/1.0\r\n", METHOD_STR[req->method], req->uri);
    if (ret < 0 || ret >= text_size)
        return(0);

    /* CSeq must have a value always*/ 
    if (!req->CSeq)
        return(0);
    /* Print to req_text */
    ret = snprintf(req_text + written, text_size - written, "CSeq: %d\r\n", req->CSeq);
    /* Check if the printed text was larger than the space available in req_text */
    if (ret < 0 || ret >= text_size-written)
        return(0);
    /* Add the number of characters written to written */
    written += ret;

    /* Write session number */
    if (req->Session != -1) {
        ret = snprintf(req_text + written, text_size - written, "Session: %d\r\n", req->Session);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    } else {
        /* Session must be present in PLAY, PASE and TEARDOWN */
        if (req->method == PLAY || req->method == PAUSE || req->method == TEARDOWN)
            return(0);
    }

    /* Accept only application/sdp in DESCRIBE */
    if (req->method == DESCRIBE) {
        ret = snprintf(req_text + written, text_size - written, "Accept: application/sdp\r\n");
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    /* Write client port */
    if (req->client_port) {
        ret = snprintf(req_text + written, text_size - written, "Transport: RTP/AVP;%s;client_port=%d-%d\r\n", CAST_STR[req->cast], req->client_port, req->client_port + 1);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    } else {
        /* client_port must be present in SETUP */
        if (req->method == SETUP)
            return(0);
    }
    ret = snprintf(req_text + written, text_size - written, "\r\n");
    if (ret < 0 || ret >= text_size - written)
        return(0);
    written += ret;

    req_text[written] = 0;
    return(written);
}

/* Convenience function, reserves RTSP_RESPONSE */
int unpack_rtsp_res2(RTSP_RESPONSE **res, char *res_text, int text_size) {
    *res = (RTSP_RESPONSE *)malloc(sizeof(RTSP_RESPONSE));
    if (!(*res))
      return 0;

    return unpack_rtsp_res(*res, res_text, text_size);
}

int unpack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size) {
    char *tok_start;
    int tok_len;
    int attr;
    tok_start = res_text;

    /* Initialize structure */
    res->code = -1;
    res->CSeq = -1;
    res->Session = -1;
    res->client_port = 0;
    res->server_port = 0;
    res->Content_Length = -1;
    res->content = 0;
    res->options = 0;

    /* Get rtsp token */
    tok_len = strcspn(tok_start, " ");
    if (tok_len == text_size)
        return(0);

    /* Check if the rtsp token is valid */
    if (strncmp(tok_start, RTSP_STR, 8))
        return(0);
    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    /* Check response code */
    if (*tok_start != '2')
        return(0);
    res->code = atoi(tok_start);


    /* Ignore rest of line */
    tok_len = strcspn(tok_start, "\r\n");
    if (tok_len == text_size) 
        return(0);
    /* Ignore next \r or \n */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;
    if (*tok_start == '\r' || *tok_start == '\n') {
        ++tok_start;
        --text_size;
    }

    /* Get Attributes until we find an empty line */
    while ( (tok_len = strcspn(tok_start, "\r\n")) ) {
        if (tok_len == text_size) {
            if (res->content)
                free(res->content);
            res->content = 0;
            return(0);
        }
        /* Get all the attributes */
        attr = detect_attr_res(res, tok_start, text_size);
        if (!attr) {
            if (res->content)
                free(res->content);
            res->content = 0;
            return(0);
        }
        tok_start += tok_len + 1;
        text_size -= tok_len + 1;
        if (*tok_start == '\r' || *tok_start == '\n') {
            ++tok_start;
            --text_size;
        }
    }
    /* If text_size is 0, a last \r\n wasn't received */
    if (text_size == 0)
        return(0);

    /* If there is content */
    if (res->Content_Length != -1) {
        /* Ignore empty line */
        tok_start += 2;
        text_size -= 2;
        if (res->Content_Length > text_size)
            return(0);

        res->content = malloc(res->Content_Length + 1);
        if (!res->content)
            return(0);

        /* Copy content */
        memcpy(res->content, tok_start, res->Content_Length);
        res->content[res->Content_Length] = 0;
    }

    /* Obligatory */
    if (res->code == -1) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }
    /* Obligatory */
    if (res->CSeq == -1) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    /* Is an error having Content-Length but not having content */
    if ((res->content && res->Content_Length == -1) ||
            (!res->content && res->Content_Length != -1)) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    /* Is an error having content and transport */
    if (res->content && res->client_port) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    return(1);
}
int pack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size) {
    int ret;
    int written;
    const char *status_str;

    /* Save space for the last \0 */
    --text_size;

    /* Write protocol and code*/
    if (res->code == -1)
        return(0);
    if (res->code == 200)
        status_str = OK_STR;
    else if (res->code == 500)
        status_str = SERVERERROR_STR;
    else if (res->code == 404)
        status_str = NOTFOUND_STR;
    else
        status_str = NULL_STR;

    ret = written = snprintf(res_text, text_size, "RTSP/1.0 %d %s\r\n", res->code, status_str);
    if (ret < 0 || ret >= text_size)
        return(0);

    /* CSeq must have a value always*/ 
    if (!res->CSeq)
        return(0);
    /* Print to res_text */
    ret = snprintf(res_text + written, text_size - written, "CSeq: %d\r\n", res->CSeq);
    /* Check if the printed text was larger than the space available in res_text */
    if (ret < 0 || ret >= text_size-written)
        return(0);
    /* Add the number of characters written to written */
    written += ret;

    /* Respond to options */
    if (res->options != 0) {
        ret = snprintf(res_text + written, text_size - written, "%s\r\n", OPTIONS_STR);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    /* Write session number */
    if (res->Session != -1 && res->Session != 0) {
        ret = snprintf(res_text + written, text_size - written, "Session: %d\r\n", res->Session);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    /* Write client and server ports*/
    if (res->client_port && res->server_port) {
        ret = snprintf(res_text + written, text_size - written, "Transport: RTP/AVP;%s;client_port=%d-%d;server_port=%d-%d\r\n", CAST_STR[res->cast], res->client_port, res->client_port + 1, res->server_port, res->server_port + 1);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    if (res->Content_Length > 0) {
        ret = snprintf(res_text + written, text_size - written, "Content-Length: %d\r\n", res->Content_Length);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }
    /* Empty line */
    ret = snprintf(res_text + written, text_size - written, "\r\n");
    if (ret < 0 || ret >= text_size - written)
        return(0);
    written += ret;

    if (res->content) {
        ret = snprintf(res_text + written, text_size - written, "%s", res->content);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    return(written);
}

int check_uri(char *uri) {
    if (strstr(uri, RTSP_URI) == uri)
        return(1);
    return(0);
}

int detect_attr_res(RTSP_RESPONSE *res, char *tok_start, int text_size) {
    int i;
    int attr = -1;
    int attr_len;
    attr_len = strcspn(tok_start, ":");
    if (attr_len == text_size || attr_len == 0)
        return(0);
    /* Discover attribute */
    for (i = 0; i < N_ATTR; ++i) {
        if (!memcmp(ATTR_STR[i], tok_start, attr_len)) {
            attr = i;
            break;
        }
    }
    tok_start += attr_len;
    text_size -= attr_len - 1;
    /* Ignore spaces after ':' */
    while (*(++tok_start) == ' ')
        --text_size;

    attr_len = strcspn(tok_start, "\r\n");
    if (attr_len == text_size || attr_len == 0)
        return(0);

    switch (attr) {
        case CSEQ_STR:
            res->CSeq = atoi(tok_start);
            break;
        case SESSION_STR:
            res->Session = atoi(tok_start);
            break;
        case CONTENT_TYPE_STR:
            if (memcmp(tok_start, SDP_STR, attr_len)) 
                return(0);
            break;
        case CONTENT_LENGTH_STR:
            res->Content_Length = atoi(tok_start);
            if (!res->Content_Length)
                return(0);
            break;
        case TRANSPORT_STR:
            /* The only acceptable transport is RTP */
            if (!strnstr(tok_start, RTP_STR, attr_len))
                return(0);
            /* Check if the transport is unicast or multicast */
            if (strnstr(tok_start, CAST_STR[UNICAST], attr_len))
                res->cast = UNICAST;
            else if (strnstr(tok_start, CAST_STR[MULTICAST], attr_len))
                res->cast = MULTICAST;
            else
                return(0);

            /* Get the client ports */
            if ( (tok_start = strnstr(tok_start, CLIENT_PORT_STR, attr_len)) ) {
                if (!tok_start)
                    return(0);
                tok_start += strlen(CLIENT_PORT_STR);
                res->client_port = (PORT)atoi(tok_start);
                if (res->client_port == 0)
                    return(0);
            }

            /* Get the client ports */
            if ( (tok_start = strnstr(tok_start, SERVER_PORT_STR, attr_len)) ) {
                if (!tok_start)
                    return(0);
                tok_start += strlen(SERVER_PORT_STR);
                res->server_port = (PORT)atoi(tok_start);
                if (res->server_port == 0)
                    return(0);
            }
            break;
        default:
            return(1);
    }
    return(1);
}
