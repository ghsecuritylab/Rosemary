CC		= gcc
TARGET 	= BOOT_BINGEN_NAND
OBJS 	= BOOT_BINGEN_NAND.o

SRCS 	= $(OBJS:.o=.c)

all : $(TARGET)
	$(CC) $(SRCS) -O2 -o $(TARGET)
	
.c.o:
	$(CC) $<
dep:
	gccmakedep $(SRCS)
clean:
	rm -f *.o
