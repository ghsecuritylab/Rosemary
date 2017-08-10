CONFIG_KANT_MIPI := m

KDIR	:= ../../../../../../../kernel/kernel-3.4.39
PWD     := $(shell pwd)

DHDOFILES = display_mipi.o

# Modified by ddanggzi
ifneq ($(CONFIG_KANT_MIPI),m)	
obj-$(CONFIG_KANT_MIPI) += display_mipi.o
kant_mipi-objs += $(DHDOFILES)
else
TARGET = kant_mipi
obj-m			:= $(TARGET).o
$(TARGET)-objs	+= $(DHDOFILES)
endif
# ddanggzi -- end

ifeq ($(CONFIG_KANT_MIPI),m)
DHDCFLAGS += -DMULTIPLE_SUPPLICANT
EXTRA_LDFLAGS += --strip-debug
else
DHDCFLAGS += -DBUILD_IN_KERNEL
endif

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	@find . -type f \( -name '*.ko' -o -name '*.mod.*'		\
					-o -name '*.o' -o -name '.*.cmd'		\
					-o -name '*.order' -o -name '*.symvers'	\
					-o -name '*.' \) -print | xargs rm -f
	rm -rf .tmp_versions