#!/bin/bash

current_arch=${1}
if [ "${current_arch}1" == "1" ]; then
    current_arch=arm
fi

#defalut value
#BUILD=debug
BUILD=release


#===================================
# mali environment config for user
#===================================
KDIR=../../../../../../kernel
#KDIR=~/devel/build/s5p6818/linux_ln
#KDIR=~/project/zynq/petalinux/build/linux/kernel/xlnx-3.19
#KDIR=~/project/zynq_nexell/kernel/kernel-3.18

CROSS_COMPILE=
makefile=
if [ "${current_arch}" == "arm" ]; then
    CROSS_COMPILE=arm-eabi-
    makefile=Makefile
else
    CROSS_COMPILE=aarch64-linux-gnu-
    makefile=Makefile64
fi
#CROSS_COMPILE=arm-cortex_a9-linux-gnueabi-
#CROSS_COMPILE=aarch64-linux-gnu-
#CROSS_COMPILE=arm-xilinx-linux-gnueabi-

USING_UMP=0
USING_PROFILING=0
MALI_SHARED_INTERRUPTS=1
CONFIG_ARCH_S5P6818=1
CONFIG_ARCH_ZYNQ=0


#===================================
# mali device driver build
#===================================
make clean -f ${makefile} KDIR=$KDIR BUILD=$BUILD \
	USING_UMP=$USING_UMP USING_PROFILING=$USING_PROFILING MALI_SHARED_INTERRUPTS=$MALI_SHARED_INTERRUPTS CONFIG_ARCH_S5P6818=$CONFIG_ARCH_S5P6818 CONFIG_ARCH_ZYNQ=$CONFIG_ARCH_ZYNQ

make -j7 -f ${makefile} KDIR=$KDIR BUILD=$BUILD \
	USING_UMP=$USING_UMP USING_PROFILING=$USING_PROFILING MALI_SHARED_INTERRUPTS=$MALI_SHARED_INTERRUPTS CONFIG_ARCH_S5P6818=$CONFIG_ARCH_S5P6818 CONFIG_ARCH_ZYNQ=$CONFIG_ARCH_ZYNQ

	
cp mali.ko ../
#sudo cp -a mali.ko ~/devel/nfs/kernel_rootfs-zynq/test/

echo "End of build mali module"
