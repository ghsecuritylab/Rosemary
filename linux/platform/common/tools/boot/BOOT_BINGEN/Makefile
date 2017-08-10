CC		= gcc
TARGET 	= BOOT_BINGEN
OBJS 	= BOOT_BINGEN.o GEN_NANDBOOTEC.o

SRCS 	= $(OBJS:.o=.c)

all : $(TARGET)
	$(CC) BOOT_BINGEN.c GEN_NANDBOOTEC.c -o $(TARGET)
	
.c.o:
	$(CC) $<
dep:
	gccmakedep $(SRCS)
clean:
	rm -f *.o
