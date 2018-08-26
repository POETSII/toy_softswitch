#!/bin/bash

if [[ "${POETS_TINSEL_ROOT}" == "" ]] ; then
  >&2 echo "ERROR : POETS_TINSEL_ROOT environement variable not set."
fi

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ${POETS_TINSEL_ROOT}/config.py envs`

# Compute space available for instructions
MaxInstrBytes=$((4 * 2**$LogInstrsPerCore - $MaxBootImageBytes))

cat - << EOF
/* THIS FILE HAS BEEN GENERATED AUTOMATICALLY. */
/* DO NOT MODIFY. INSTEAD, MODIFY THE genld.sh SCRIPT. */

OUTPUT_ARCH( "riscv" )

MEMORY
{
  instrs  : ORIGIN = $MaxBootImageBytes, LENGTH = $MaxInstrBytes
  globals : ORIGIN = $DRAMBase, LENGTH = $DRAMGlobalsLength
}

SECTIONS
{
  .text   : { *.o(.text*) }             > instrs
  .bss    : { *.o(.bss*) }              > globals = 0
  .rodata : { *.o(.rodata*) }           > globals
  .sdata  : { *.o(.sdata*) }            > globals
  .data   : { *.o(.data*) }             > globals
  __heapBase = ALIGN(.);
}
EOF
