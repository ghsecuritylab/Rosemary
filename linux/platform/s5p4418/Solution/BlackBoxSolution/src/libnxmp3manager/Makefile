ifndef	TOPDIR
TOPDIR	:= ../..
endif
########################################################################
# Get Linux Build Enviornment:
include ../../../../build.env

######################################################################
# Build options

# Prevent: "warning: creating a DT_TEXTREL in object"
CFLAGS		+= -fpic -DARM_11
CPPFLAGS	+= -fpic -DARM_11

INCLUDE		+= -I./
INCLUDE		+= -I$(LIBSDIR)/include
INCLUDE		+= -I$(TOPDIR)/include
INCLUDE		+= -I../libnxfilters

LIBRARY		+= -L$(LIBSDIR)/lib
LIBRARY		+= -L$(TOPDIR)/lib
LIBRARY		+= -L../libnxfilters/ -lnxfilters
LIBRARY		+= -lnxmp3enc
LIBRARY		+= -lstdc++ -lpthread -lasound

######################################################################
# Target
COBJS  	:= 
CPPOBJS	:= CNX_Mp3Manager.o CNX_Mp3Notify.o

LIBNAME := libnxmp3manager

SO_NAME	:= $(LIBNAME).so
TARGET	:= $(LIBNAME).so
######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): depend $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(SO_NAME) -o $@ $(OBJS) $(LIBRARY)
	chmod 644 $(TARGET)

clean:
	@find $(OBJTREE) -type f \
		\( -name '*.o'	-o -name $(TARGET) -o -name '*.a' -o -name '*.so.*' -o \
		-name '*.bin' -o -name '*.map' -o -name 'core' -o -name '*~' -o -name '.depend' \) \
		-print | xargs rm -f

install:
	cp -a $(SO_NAME) $(TOPDIR)/lib
	cp -a INX_Mp3Manager.h $(TOPDIR)/include

distclean:
	rm -f $(TOPDIR)/lib/$(SO_NAME)
	rm -f $(TOPDIR)/include/INX_Mp3Manager.h
	@find $(OBJTREE) -type f \
		\( -name '*.o'	-o -name $(TARGET) -o -name '*.a' -o -name '*.so.*' -o \
		-name '*.bin' -o -name '*.map' -o -name 'core' -o -name '*~' -o -name '.depend' \) \
		-print | xargs rm -f

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CPPFLAGS)   $(INCS) $(SRCS) > .depend
