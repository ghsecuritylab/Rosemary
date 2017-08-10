#!/bin/bash
current_arch=${1}
if [ "${current_arch}1" == "1" ];then
    current_arch=arm
fi

makefile=
if [ "${current_arch}" == "arm" ]; then
    makefile=Makefile
else
    makefile=Makefile64
fi

make -f ${makefile} ARCH=${current_arch} clean
make -f ${makefile} ARCH=${current_arch} modules -j4
cp wlan.ko ../../../../../hardware/samsung_slsi/slsiap/prebuilt/modules/
