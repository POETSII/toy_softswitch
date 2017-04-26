#!/bin/bash

if [ ! -f riscv-toolchain-ubuntu-16.04.tar.gz ] ; then
    BOXOBS=hpeizds5xe5bfiiw5n7j2173krwq3p57
    wget https://imperialcollegelondon.box.com/shared/static/${BOXOBS}.gz
    mv ${BOXOBS}.gz riscv-toolchain-ubuntu-16.04.tar.gz
fi

if [ ! -d riscv-toolchain ] ; then
    tar -xzf riscv-toolchain-ubuntu-16.04.tar.gz
fi

grep DT10_RISCV_TOOLCHAIN_ROOT ~/.bashrc
if [ $? -ne 0 ] ; then
    echo "export DT10_RISCV_TOOLCHAIN_ROOT=$(pwd)/riscv-toolchain/" >> ~/.bashrc
    echo 'export PATH=${PATH}:${DT10_RISCV_TOOLCHAIN_ROOT}/bin' >> ~/.bashrc
fi
