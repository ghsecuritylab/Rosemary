########################################################################
# Get Linux Build Enviornment:
include ../../build.env

######################################################################
# Build options
INCLUDE += -I./
INCLUDE += -I../vpu_test2/ffmpeg/include
LIBRARY	+= -L$(LIBSDIR)/lib -L$(LIBSDIR)/lib/ratecontrol
LIBRARY += -lnxvpu -lnxvidrc -lnxdsp -lnxv4l2 -lnxgraphictools -lnxvmem -lnxscaler -lion
LIBRARY += -lpthread -lstdc++
LIBRARY	+= -L../vpu_test2/ffmpeg/libs -lavcodec -lavformat -lavutil
LIBRARY += -lEGL -lGLESv2 -lVR

######################################################################
# Target
TRNAS_OBJS	:= codec_info.o
TRNAS_OBJS	+= queue.o
TRNAS_OBJS	+= NX_Semaphore.o
TRNAS_OBJS	+= trans_main.o

COBJS	:= queue.o codec_info.o NX_Semaphore.o trans_main.o

CPPOBJS	:=  

CFLAGS	+= -g -pthread

TRNASAPP := trans_test2

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TRNASAPP)

$(TRNASAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(TRNAS_OBJS) -o $@ $(LIBRARY)

clean:
	rm -f $(TRNASAPP) *.o .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
