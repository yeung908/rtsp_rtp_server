# Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>
# Copyright (c) 2012, Bartosz Andrzej Zawada <bebour@gmail.com>
# 
# Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
CC=gcc
CFLAGS=-Wall -g
TEST=test_parse_rtsp test_parse_sdp test_rtsp test_parse_rtp
EXE=rtsp_server rtp_server
OBJ_MSG=@echo "\n\033[33;01mCompilando objeto: $@\033[00m"
TST_MSG=@echo "\n\033[34;01mCompilando test: $@\033[00m"
EXE_MSG=@echo "\n\033[32;01mCompilando ejecutable: $@\033[00m"
#video

all: $(TEST) $(EXE)

#=== EXECUTABLE FILES

rtsp_server: rtsp_server.c server.o server_client.o hashtable.o hashfunction.o parse_rtsp.o rtsp.o parse_sdp.o strnstr.o socketlib.o
	$(EXE_MSG)
	$(CC) $(CFLAGS) -o $@ $^ -pthread

rtp_server: rtp_server.c server.o server_client.o hashtable.o hashfunction.o strnstr.o parse_rtp.o rtcp.o
	$(EXE_MSG)
	$(CC) $(CFLAGS) -o $@ $^ -pthread `pkg-config --libs gtk+-2.0 gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-interfaces-0.10` `pkg-config --cflags gtk+-2.0 gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-interfaces-0.10`

#--- TESTS
test_rtsp: test_rtsp.c rtsp.o parse_rtsp.o parse_sdp.o strnstr.o
	$(TST_MSG)
	$(CC) $(CFLAGS) -o $@ $^

test_parse_sdp: test_parse_sdp.c parse_sdp.o strnstr.o
	$(TST_MSG)
	$(CC) $(CFLAGS) -o $@ $^

test_parse_rtsp: test_parse_rtsp.c parse_rtsp.o strnstr.o
	$(TST_MSG)
	$(CC) $(CFLAGS) -o $@ $^

test_parse_rtp: test_parse_rtp.c parse_rtp.o strnstr.o
	$(TST_MSG)
	$(CC) $(CFLAGS) -o $@ $^

#==== OBJECT FILES
rtp_client.o: rtp_client.c rtp_client.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< `pkg-config --cflags gtk+-2.0 gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-interfaces-0.10`

socketlib.o: socketlib/socketlib.c socketlib/socketlib.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

strnstr.o: strnstr.c strnstr.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

server.o: server.c server.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

rtcp.o: rtcp.c rtcp.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

server_client.o: server_client.c server_client.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

rtsp.o: rtsp.c rtsp.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $< 

parse_rtp.o: parse_rtp.c parse_rtp.h
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $<

parse_sdp.o: parse_sdp.c parse_sdp.h 
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $<

parse_rtsp.o: parse_rtsp.c parse_rtsp.h 
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $<

hashtable.o: hashtable/hashtable.c hashtable/hashtable.h 
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $<

hashfunction.o: hashtable/hashfunction.c hashtable/hashfunction.h 
	$(OBJ_MSG)
	$(CC) $(CFLAGS) -o $@ -c $<

#=== AUXILIARY COMMANDS

clean_obj:
	@echo "\033[31;01mBorrando los ficheros objeto\033[00m"
	@rm -rf *.o

clean: clean_obj
	@echo "\033[31;01mBorrando los ficheros ejecutables\033[00m"
	@rm -rf $(TEST) $(EXE)
