#!/bin/sh

export HOST_PORT=4938;

echo "**** Running QEMU SSH on port ${HOST_PORT} ****";

export SMP=1;

while [ "$1" != "" ]; do
    if [ "$1" = "-debug" ];
    then
        echo "**** GDB port $((HOST_PORT + 1)) ****";
        DEBUG="-gdb tcp::$((HOST_PORT + 1)) -S -d in_asm -D debug.log";
    fi;
    if [ "$1" = "-smp" ];
    then
        SMP="$2";
        shift;
    fi;
    shift;
done;

/home/yangxin/xs-env/keystone/qemu/build/riscv64-softmmu/qemu-system-riscv64 \
 $DEBUG \
 -m 2G \
 -nographic \
 -machine virt,rom=/home/yangxin/xs-env/keystone/build64-xs/bootrom.build/bootrom.bin \
 -bios /home/yangxin/xs-env/keystone/build64-xs/sm.build/platform/generic/firmware/fw_jump.elf \
 -kernel /home/yangxin/xs-env/keystone/build64-xs/linux.build/arch/riscv/boot/Image \
       -append "console=ttyS0 ro root=/dev/vda"       -drive file=/home/yangxin/xs-env/keystone/build64-xs/buildroot.build/images/rootfs.ext2,format=raw,id=hd0       -device virtio-blk-device,drive=hd0    \
 -netdev user,id=net0,net=192.168.100.1/24,dhcpstart=192.168.100.128,hostfwd=tcp::${HOST_PORT}-:22 \
 -device virtio-net-device,netdev=net0 \
 -device virtio-rng-pci \
 -smp $SMP
