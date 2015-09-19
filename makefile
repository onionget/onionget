name=onionGet
CC=gcc
CFLAGS = -lpthread  -Wall -I. -o $(name)

VPATH=source

SRCS= $(VPATH)/client.c $(VPATH)/activeconnection.c $(VPATH)/controller.c $(VPATH)/memorymanager.c $(VPATH)/router.c $(VPATH)/datacontainer.c $(VPATH)/server.c $(VPATH)/dlinkedlist.c $(VPATH)/diskfile.c

all: main

main: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS)