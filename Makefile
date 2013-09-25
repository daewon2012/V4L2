CC = gcc
LD = ld
OBJS = camera.o
#OBJS = vce.o
TARGET = MyCamera
CFLAGS = -O2 -Wall -c
LDFLAGS = -ljpeg

.PHONY: all clean

all : $(TARGET)

clean :
	rm -f $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ -ljpeg

.c.o :
	$(CC) $(CFLAGS) -o $@ $<