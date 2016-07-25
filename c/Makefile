cflags += -DLINUX_2_6_KERNEL
clibs += -ldl -lpthread -lm
TARGET = libyue-soft.so
OBJS = logger.o timer.o utils.o network.o threads.o sqlite3.o\
	   base64.o cfg_op.o crc.o trace.o

all:$(TARGET)

$(TARGET):$(OBJS)
	gcc -g -fPIC -shared -o $@ $(OBJS)
.SUFFIXES: .c .o
.c.o:
	gcc -c -fPIC -g $(clibs) $(cflags) $*.c

install:
	cp libyue-soft.so /usr/lib64/

uninstall:
	rm -rf /usr/lib64/libyue-soft.so

clean:
	rm -rf *.so
	rm -rf *.o
