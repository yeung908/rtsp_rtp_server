/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _SERVERS_COMM_H_
#define _SERVERS_COMM_H_

#define MAX_URI_LENGTH 1024

typedef enum {SETUP_RTP_UNICAST = 0, PLAY_RTP, PAUSE_RTP, TEARDOWN_RTP, CHECK_EXISTS_RTP} ORDER;

typedef struct {
    ORDER order;
    char uri[MAX_URI_LENGTH]; /* SETUP_RTP and CHECK_EXISTS_RTP */
    int Session; /* SETUP_RTP */
    unsigned int ssrc; /* PLAY_RTP, PAUSE_RTP, TEARDOWN_RTP */
    unsigned int client_ip; /* SETUP_RTP */
    unsigned short client_port; /* SETUP_RTP */
    unsigned short response_port; /* PLAY_RTP, PAUSE_RTP, TEARDOWN_RTP */
} RTSP_TO_RTP;

typedef enum {OK_RTP = 0, ERR_RTP, FINISHED_RTP} RESPONSE;

typedef struct {
    RESPONSE order;
    int Session; /* This */
    /* TODO: This pointer isn't well  managed when sending and receiving */
    char uri[MAX_URI_LENGTH];  /* and this identify uniquely the corresponding RTSP stream to the sender RTP stream */
    unsigned int ssrc;
    unsigned short server_port;
} RTP_TO_RTSP;
#endif
