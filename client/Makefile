CC=gcc
DEFINES = -DLFMB_CLIENT

#drag in another Makefile for the client

all: lfmb

.c.o:
	$(CC) -c $(DEFINES) $<
#FIXME this is ugly:
file.o:
	$(CC) -c $(DEFINES) ../file.c
protocol.o:
	$(CC) -c $(DEFINES) ../protocol.c
usb_transport.o:
	$(CC) -c $(DEFINES) ../usb_transport.c
usb_ffs.o:
	$(CC) -c $(DEFINES) ../usb_ffs.c
message.o:
	$(CC) -c $(DEFINES) ../message.c
shell.o:
	$(CC) -c $(DEFINES) ../shell.c

OBJECTS= protocol.o \
		file.o \
		usb_transport.o \
		usb_ffs.o \
		message.o \
		shell.o \
		lfmb.o

lfmb: $(BUILDARCHDIR) $(OBJECTS)
	$(CC) -o lfmb $(OBJECTS)

clean:
	-rm *.o
	-rm lfmb
