CC=gcc
DEFINES = -DLFMB_CLIENT

#drag in another Makefile for the client

all: lfmb

.c.o:
	$(CC) -c $(DEFINES) $<
#FIXME this is ugly:
file.o: ../file.c ../file.h ../protocol.h
	$(CC) -c $(DEFINES) ../file.c
protocol.o: ../protocol.c ../protocol.h ../usb_transport.h ../shell.h 
	$(CC) -c $(DEFINES) ../protocol.c
usb_transport.o: ../usb_transport.c ../usb_transport.h ../usb_ffs.h ../message.h
	$(CC) -c $(DEFINES) ../usb_transport.c
usb_ffs.o:
	$(CC) -c $(DEFINES) ../usb_ffs.c
message.o:
	$(CC) -c $(DEFINES) ../message.c
shell.o:
	$(CC) -c $(DEFINES) ../shell.c
io.o:
	$(CC) -c $(DEFINES) ../io.c

OBJECTS= protocol.o \
		file.o \
		usb_transport.o \
		usb_ffs.o \
		message.o \
		shell.o \
		io.o \
		lfmb.o

lfmb: $(OBJECTS)
	$(CC) -o lfmb $(OBJECTS)

clean:
	-rm *.o
	-rm lfmb
