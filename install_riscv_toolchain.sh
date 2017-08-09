#!/bin/bash

if [ ! -f riscv-toolchain-32-ubuntu-16.04.tar.gz ] ; then
    BOXOBS=4lyu9meltq6fjfbbb6l2ob3qvc0xr2at
    wget https://imperialcollegelondon.box.com/shared/static/${BOXOBS}.gz
    mv ${BOXOBS}.gz riscv-toolchain-32-ubuntu-16.04.tar.gz
fi

if [ ! -d riscv-toolchain-32 ] ; then
    tar -xzf riscv-toolchain-32-ubuntu-16.04.tar.gz
fi

grep DT10_RISCV_TOOLCHAIN_ROOT ~/.bashrc
if [ $? -ne 0 ] ; then
    echo "export DT10_RISCV_TOOLCHAIN_ROOT=$(pwd)/riscv-toolchain-32/" >> ~/.bashrc
    echo 'export PATH=${PATH}:${DT10_RISCV_TOOLCHAIN_ROOT}/bin' >> ~/.bashrc
fi
