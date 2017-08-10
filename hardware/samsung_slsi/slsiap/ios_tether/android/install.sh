#!/bin/bash

PATH_LOCAL_LIB="libs/armeabi-v7a"
PATH_DEVICE_LIB="/vendor/lib"
PATH_DEVICE_BIN="/vendor/bin"

# Make the system partition writable
#adb shell su -c "mount -o remount rw /vendor"

# Init Env
adb shell "mkdir /vendor/lib"
adb shell "mkdir /vendor/bin"
adb shell "mkdir /var"
adb shell "mkdir /var/lib"
adb shell "mkdir /var/run"

# Install so
echo "----------Install so--------------"
for S in libiconv.so libxml2.so libplist.so libplist++.so libusb.so libusbmuxd.so libcrypto.so libssl.so libimobiledevice.so libzip.so
do
#  adb shell rm "$PATH_DEVICE_LIB/$S"
  adb push "$PATH_LOCAL_LIB/$S" $PATH_DEVICE_LIB
done


# Install binary
echo "----------Install binary--------------"
for B in listdevs openssl usbmuxdd ideviceid ideviceinfo idevicecrashreport idevicedate idevicename idevicediagnostics idevicescreenshot idevicesyslog ideviceinstaller ifuse fusermount
do
#  adb shell rm "$PATH_DEVICE_BIN/$B"
  adb push "$PATH_LOCAL_LIB/$B" $PATH_DEVICE_BIN
  adb shell "chmod 0755 $PATH_DEVICE_BIN/$B"
done

# Make the system partition read only again
#adb shell su -c "mount -o remount ro /vendor"

# Run listdevs to
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/data/local/tmp
#adb shell su -c "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$PATH_DEVICE_LIB"
#adb shell su -c "export PATH=\$PATH:$PATH_DEVICE_LIB"

echo "----------list usb devs--------------"
adb shell  "$PATH_DEVICE_BIN/listdevs"
adb shell  "$PATH_DEVICE_BIN/usbmuxdd -v"
adb shell  "$PATH_DEVICE_BIN/ideviceid -l"

echo "----------ls $PATH_DEVICE_BIN--------------"
adb shell  "ls $PATH_DEVICE_BIN"
