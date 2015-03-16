#!/bin/bash

export KERNELDIR=/home/zwliew/android/ghost
export MKBOOTIMGDIR=/home/zwliew/android/mkbootimg
export OUTDIR=/home/zwliew/android/ghost/out/ghost
export ZWLIEW_VERSION=r$1

cd $KERNELDIR

make zwliew_defconfig
time make -j4 > ../a
cp arch/arm/boot/zImage $MKBOOTIMGDIR/ghost/zImage
cp -r ramdisk $MKBOOTIMGDIR/ghost
find . -name "*.ko" -exec cp {} $OUTDIR/system/lib/modules \;
if [ "$2" = "1" ]
then
  make mrproper
fi

cd $MKBOOTIMGDIR

./mkboot ghost boot.img
mv boot.img $OUTDIR/boot.img

cd ghost

rm -r ramdisk
rm zImage

cd $OUTDIR/system/lib/modules

mv wlan.ko prima/prima_wlan.ko

cd $OUTDIR

zip -r zwliew_Kernel-ghost-$ZWLIEW_VERSION.zip META-INF
zip -r zwliew_Kernel-ghost-$ZWLIEW_VERSION.zip system
zip zwliew_Kernel-ghost-$ZWLIEW_VERSION.zip boot.img
rm system/lib/modules/adsprpc.ko
rm system/lib/modules/utags.ko
rm system/lib/modules/prima/prima_wlan.ko
rm boot.img

cd $KERNELDIR
