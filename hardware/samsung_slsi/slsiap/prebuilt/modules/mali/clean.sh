#!/bin/bash

#defalut value
#BUILD=debug
BUILD=release


#===================================
# mali environment config for user
#===================================
KDIR=../../../../../../kernel
CROSS_COMPILE=arm-eabi-
#KDIR=~/devel/build/s5p6818/linux_ln
#KDIR=~/project/zynq/petalinux/build/linux/kernel/xlnx-3.19
#CROSS_COMPILE=arm-cortex_a9-linux-gnueabi-
#CROSS_COMPILE=aarch64-linux-gnu-
#CROSS_COMPILE=arm-xilinx-linux-gnueabi-

USING_UMP=0
USING_PROFILING=0
MALI_SHARED_INTERRUPTS=1


#===================================
# mali device driver build
#===================================
make clean KDIR=$KDIR BUILD=$BUILD \
	USING_UMP=$USING_UMP USING_PROFILING=$USING_PROFILING MALI_SHARED_INTERRUPTS=$MALI_SHARED_INTERRUPTS
