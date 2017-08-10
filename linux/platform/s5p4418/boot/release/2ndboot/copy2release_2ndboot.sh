#!/bin/bash

SEC4418_NAME=S5P4418_2ndboot_DDR3_V061
SEC4330_NAME=NXP4330_2ndboot_DDR3_V061

DST4418_NAME1=2ndboot_avn_ref
DST4418_NAME2=2ndboot_drone

DST4330_NAME1=2ndboot_lepus

echo "$SECBOOT_NAME"

cp ../../temporary/2ndboot/"$SEC4418_NAME"_ALL.bin ./"$SEC4418_NAME"_ALL.bin

cp ../../temporary/2ndboot/"$SEC4418_NAME"_AVN_ALL.bin ./"$DST4418_NAME1".bin
cp ../../temporary/2ndboot/"$SEC4418_NAME"_AVN_ALL.bin ./"$DST4418_NAME1"_sdmmc.bin

cp ../../temporary/2ndboot/"$SEC4418_NAME"_DRONE_ALL.bin ./"$DST4418_NAME2".bin
cp ../../temporary/2ndboot/"$SEC4418_NAME"_DRONE_ALL.bin ./"$DST4418_NAME2"_sdmmc.bin

cp ../../temporary/2ndboot/"$SEC4330_NAME"_SDMMC.bin ./"$DST4330_NAME1"_sdmmc.bin
cp ../../temporary/2ndboot/"$SEC4330_NAME"_USB.bin   ./"$DST4330_NAME1"_usb.bin
cp ../../temporary/2ndboot/"$SEC4330_NAME"_SPI.bin   ./"$DST4330_NAME1"_spi.bin
