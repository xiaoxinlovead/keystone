#!/bin/bash

SSH_VERSION_MAJOR=$(ssh -V 2>&1 | sed 's/^OpenSSH_\([0-9]*\).*$/\1/')

SSH_OPTIONS="-i /home/yangxin/xs-env/keystone/build64/overlay/root/.ssh/id_rsa"
SSH_OPTIONS+=" -o StrictHostKeyChecking=no"
SSH_OPTIONS+=" -o UserKnownHostsFile=/dev/null"

upload_to_qemu() {
    # See "Potentially incompatible changes" section in
    # https://www.openssh.com/txt/release-9.0
    SCP_OPTIONS=
    if [ $SSH_VERSION_MAJOR -ge 9 ]; then
        SCP_OPTIONS+=" -O"
    fi
    echo "Uploading \"$(basename $1)\" to QEMU ..."
    scp ${SSH_OPTIONS} ${SCP_OPTIONS} -P 5055 $1 root@localhost:.
}

run_in_qemu() {
    echo "Running \"$1\" in QEMU ..."
    ssh ${SSH_OPTIONS} -p 5055 root@localhost "$1"
}

run_in_qemu "insmod keystone-driver.ko"

upload_to_qemu "/home/yangxin/xs-env/keystone/build64/examples/tests/tests.ke"
run_in_qemu "./tests.ke"

upload_to_qemu "/home/yangxin/xs-env/keystone/build64/examples/attestation/attestor.ke"
upload_to_qemu "/home/yangxin/xs-env/keystone/build64/sm.build/platform/generic/firmware/fw_jump.bin"
run_in_qemu "./attestor.ke"

run_in_qemu "poweroff"
