name=onionGet
CC=gcc
CFLAGS = -lpthread  -Wall -I. -o $(name)

VPATH=source

SRCS= $(VPATH)/client.c $(VPATH)/connection.c $(VPATH)/systemManager.c $(VPATH)/macros.c $(VPATH)/connectionBank.c $(VPATH)/fileBank.c $(VPATH)/connection.c $(VPATH)/controller.c $(VPATH)/memoryManager.c $(VPATH)/router.c $(VPATH)/dataContainer.c $(VPATH)/server.c $(VPATH)/bank.c $(VPATH)/diskFile.c

all: main

main: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS)