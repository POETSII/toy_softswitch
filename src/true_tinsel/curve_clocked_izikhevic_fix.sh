#!/bin/bash

export PYTHONPATH=~/graph_schema/tools

#TS="1 2 4 8 16 32 64 128 256 512 1024"
TS="1024 512 256 128 64 32 16 8 4 2 1"

WORKING=$(mktemp -d)

NE=80
NI=20
K=40
MAXTICKS=100
MAXFANIN=16
MAXFANOUT=16

(cd ~/graph_schema && python3 apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py $NE $NI $K $MAXTICKS $MAXFANIN $MAXFANOUT  > ${WORKING}/base.xml  )

BASE="clocked_izhikevich_fix_${NE}_${NI}_${K}_${MAXTICKS}"

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
done
