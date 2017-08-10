#!/bin/sh
make -j8 -C ../src/libnxfilters || exit $?
make install -C ../src/libnxfilters

make -j8 -C ../src/libnxdvr || exit $?
make install -C ../src/libnxdvr

make -j8 -C ../apps/nxdvrsol || exit $?
make install -C ../apps/nxdvrsol

make -j8 -C ../apps/nxdvrmonitor || exit $?
make install -C ../apps/nxdvrmonitor
