CC=$(CROSS_COMPILE)gcc

#drag in another Makefile for the client

all: lfmbd
	cd client; make

.c.o:
	$(CC) -c $<

OBJECTS= message.o \
		usb_transport.o \
		usb_ffs.o \
		protocol.o \
		file.o \
		shell.o \
		lfmbd.o

lfmbd: $(OBJECTS)
	$(CC) -o lfmbd -lutil $(OBJECTS)

clean:
	-rm *.o
	-rm lfmbd
	-cd client; make clean
