#!/bin/bash

export PYTHONPATH=~/graph_schema/tools

#TS="1 2 4 8 16 32 64 128 256 512 1024"
TS="1024 512 256 128 64 32 16 8 4 2 1"

WORKING=$(mktemp -d)

N=8
M=4
MAXTICKS=1000

SEEDS="1 2 3 4 5 6 7 8"
REP="0"

(cd ~/graph_schema && python3 apps/clock_tree/create_clock_tree_instance.py $M $N $MAXTICKS > ${WORKING}/base.xml  )

BASE="clock_tree_${N}_${M}_${MAXTICKS}"

rm ${BASE}.csv
for T in $TS; do
    for S in $SEEDS; do
	NAME="${BASE}_seed${S}"
	mkdir -p ${WORKING}/$NAME/$S
	(cd ~/graph_schema && python3 tools/render_graph_as_softswitch.py \
				      ${WORKING}/base.xml \
				      --dest ${WORKING}/${NAME}/$S \
				      --threads ${T} \
				      --placement-seed ${S} )
	(make -B APPLICATION=${WORKING}/${NAME}/$S )
	for R in ${REP}; do
	    (cd ~/tinsel && make -C de5 download-sof)
	    /usr/bin/time -f "%e" -o ${WORKING}/${NAME}/$S.time make run-jtag
	    TT=$(cat ${WORKING}/${NAME}/$S.time)
	    echo "${NAME}, ${T}, ${S}, ${R}, ${TT}" | tee - >> ${BASE}.csv
	done
    done
done
