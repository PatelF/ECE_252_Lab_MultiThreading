CC = gcc 
CFLAGS = -Wall -std=gnu99 -g # "curl-config --cflags" output is empty  
LD = gcc
LDFLAGS = -g 
LDLIBS = -lcurl -lz -pthread # "curl-config --libs" output 

LIB_UTIL = crc.o zutil.o
SRCS   = paster2.c crc.c zutil.c
OBJS1  = paster2.o $(LIB_UTIL)
TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS1) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

.c.o:
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) all.png
