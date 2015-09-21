name=onionGet
CC=gcc
CFLAGS = -lpthread  -Wall -I. -o $(name)

VPATH=source

SRCS= $(VPATH)/client.c $(VPATH)/routerBank.c $(VPATH)/fileBank.c $(VPATH)/connection.c $(VPATH)/controller.c $(VPATH)/memoryManager.c $(VPATH)/router.c $(VPATH)/dataContainer.c $(VPATH)/server.c $(VPATH)/dll.c $(VPATH)/diskFile.c

all: main

main: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS)