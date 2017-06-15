#!/bin/bash

export PYTHONPATH=~/graph_schema/tools

#TS="1 2 4 8 16 32 64 128 256 512 1024"
TS="1024 512 256 128 64 32 16 8 4 2 1"

WORKING=$(mktemp -d)

N=256
T=1
MAXTIME=100

(cd ~/graph_schema && python3 apps/ising_spin_fix/create_ising_spin_fix_instance.py $N $T $MAXTIME > ${WORKING}/base.xml  )

BASE="ising_spin_fix_${N}_${T}_${MAXTIME}"

rm ${BASE}.csv
for T in $TS; do
    NAME="${BASE}_threads${T}"
    mkdir ${WORKING}/$NAME
    (cd ~/graph_schema && python3 tools/render_graph_as_softswitch.py \
	 ${WORKING}/base.xml \
	 --dest ${WORKING}/${NAME} \
	 --threads ${T} )
    (make -B APPLICATION=${WORKING}/${NAME})
    (cd ~/tinsel && make -C de5 download-sof)
    /usr/bin/time -f "%e" -o ${WORKING}/${NAME}.time make run-jtag
    TT=$(cat ${WORKING}/${NAME}.time)
    echo "${NAME}, ${T}, ${TT}" | tee - >> ${BASE}.csv
    rm ${WORKING}/${NAME}/*.cpp
done
