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

BINS = server.exec client.exec key_gen.exec my-crypto-test.exec my_aes.exec

all: $(BINS)

server.exec: server.c socket-common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

client.exec: client.c socket-common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

key_gen.exec: key_gen.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

my-crypto-test.exec: my-crypto-test.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

my_aes.exec: my_aes.c 
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o *~ $(BINS)
