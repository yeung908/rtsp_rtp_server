/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "parse_sdp.h"
#include "strnstr.h"

const char * MEDIA_TYPE_STR[] = {"audio\0", "video\0"};

/*
 * return: Size of sdp_text. 0 is error
 */
int pack_sdp(SDP *sdp, unsigned char *sdp_text, int sdp_max_size) {
    int ret;
    int i;
    int written = 0;
    /* Save space for the last \0 */
    --sdp_max_size;
    sdp_text[sdp_max_size] = 0;

    /* Write global control uri if exists */
    if (sdp->uri) {
        ret = snprintf((char *)sdp_text + written, sdp_max_size, "a=control:%s\r\n", sdp->uri);
        if (ret < 0 || ret + written >= sdp_max_size)
            return(0);
        written += ret;
        sdp_max_size -= ret;
    }

    /* Write each media */
    for (i = 0; i < sdp->n_medias; ++i) {
        ret = snprintf((char *)sdp_text + written, sdp_max_size, "m=%s %d RTP/AVP %d\r\n", MEDIA_TYPE_STR[sdp->medias[i]->type], sdp->medias[i]->port, sdp->medias[i]->type);
        if (ret < 0 || ret + written >= sdp_max_size)
            return(0);
        written += ret;
        sdp_max_size -= ret;

        ret = snprintf((char *)sdp_text + written, sdp_max_size, "a=control:%s\r\n", sdp->medias[i]->uri);
        if (ret < 0 || ret + written >= sdp_max_size)
            return(0);
        written += ret;
        sdp_max_size -= ret;


    }
    sdp_text[written] = 0;
    return(written);
}

int unpack_sdp2(SDP **sdp, unsigned char *sdp_text, int sdp_size) {
    *sdp = (SDP*)malloc(sizeof(SDP));
    if (!(*sdp))
        return(0);

    return unpack_sdp(*sdp, sdp_text, sdp_size);
}

/*
 * return: 1 ok 0 err
 */
int unpack_sdp(SDP *sdp, unsigned char *sdp_text, int sdp_size) {
    unsigned char *media;
    unsigned char *control;
    int tok_len;
    int media_type;
    int i;

    /* Initialize structure */
    sdp->uri = 0;
    sdp->n_medias = 0;
    sdp->medias = malloc(sizeof(MEDIA));

    media = (unsigned char *)strnstr((char *)sdp_text, "m=", sdp_size);
    control = (unsigned char *)strnstr((char *)sdp_text, "a=control:", sdp_size);
    if (!media || !control) {
        free(sdp->medias);
        sdp->medias = 0;
        return(0);
    }
    media += 2;
    control += 10;

    /* If there is a url to control all medias */
    if (control < media) {
        tok_len = strcspn((char *)control, "\r\n");
        if (tok_len == sdp_size - (control - sdp_text)) {
            free(sdp->medias);
            sdp->medias = 0;
            return(0);
        }
        sdp->uri = malloc(tok_len + 1);
        memcpy(sdp->uri, control, tok_len);
        sdp->uri[tok_len] = 0;

        /* Find control for the first media */
        control = (unsigned char *)strnstr((char *)control, "a=control:", sdp_size - (control - sdp_text));
        if (!control) {
            free(sdp->medias);
            sdp->medias = 0;
            return(0);
        }
        control += 10;
    }

    for (;;) {
        if (control < media) {
            free(sdp->medias);
            sdp->medias = 0;
            return(0);
        }
        for (i = 0; i < N_MEDIA_TYPE; ++i) {
            if (!strncmp((char *)media, MEDIA_TYPE_STR[i], strlen(MEDIA_TYPE_STR[i]))) {
                media_type = i;
                break;
            }
        }
        /* Get media type */
        sdp->medias[sdp->n_medias]->type = media_type;
        media += 6;
        /* Get port */
        sdp->medias[sdp->n_medias]->port = atoi((char *)media);

        /* Get media uri */
        tok_len = strcspn((char *)control, "\r\n");
        if (tok_len == sdp_size - (control - sdp_text)) {
            free(sdp->medias);
            sdp->medias = 0;
            return(0);
        }
        sdp->medias[sdp->n_medias]->uri = malloc(tok_len + 1);
        memcpy(sdp->medias[sdp->n_medias]->uri, control, tok_len);
        sdp->medias[sdp->n_medias]->uri[tok_len] = 0;

        ++sdp->n_medias;

        media = (unsigned char *)strnstr((char *)media, "m=", sdp_size);
        control = (unsigned char *)strnstr((char *)control, "a=control:", sdp_size);
        if (!media || !control)
            break;
        media += 2;
        control += 10;
        sdp->medias = realloc(sdp->medias, sizeof(MEDIA) * (sdp->n_medias + 1));
        if (!sdp->medias) {
            sdp->medias = 0;
            return(0);
        }

    }
    return(1);

}

void free_sdp(SDP **sdp) {
    int i;

    if ((*sdp)->uri)
        free((*sdp)->uri);

    for (i=0; i < (*sdp)->n_medias; ++i)
        if ((*sdp)->medias[i]->uri)
            free((*sdp)->medias[i]->uri);

    free((*sdp)->medias);
    free(*sdp);
    *sdp = 0;
}
