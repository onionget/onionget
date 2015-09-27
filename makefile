name=onionGet
CC=gcc
CFLAGS = -lpthread -lcap -Wall -I. -o $(name)

VPATH=source

SRCS= $(VPATH)/client.c $(VPATH)/connection.c $(VPATH)/systemManager.c $(VPATH)/macros.c $(VPATH)/controller.c $(VPATH)/memoryManager.c $(VPATH)/router.c $(VPATH)/server.c $(VPATH)/diskFile.c

all: main

main: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS)