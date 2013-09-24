CC = gcc
LD = ld
OBJS = camera.o
#OBJS = vce.o
TARGET = MyCamera
#CFLAGS = -O2 -Wall -Werror -fomit-frame-pointer -c
#LDFLAGS = -lc

.SUFFIXES : .c .o

.PHONY: all clean

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

clean :
	rm -f $(OBJS) $(TARGET)