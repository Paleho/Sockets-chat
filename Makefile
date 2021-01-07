###################################################
#
# Makefile
# Simple TCP/IP communication using sockets
#
# Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
#
###################################################

CC = gcc

CFLAGS = -Wall
CFLAGS += -g
# CFLAGS += -O2 -fomit-frame-pointer -finline-functions

LIBS =

BINS = server_crypto client_crypto

all: $(BINS)

server_crypto: server_crypto.c socket-common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

client_crypto: client_crypto.c socket-common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o *~ $(BINS)
