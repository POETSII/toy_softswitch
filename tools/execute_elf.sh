#!/bin/bash

script_prefix="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
path_prefix="$(dirname "$(dirname "${script_prefix}" )" )"

path_to_tinsel="${path_prefix}/tinsel"
path_to_graph_schema="${path_prefix}/graph_schema"
path_to_softswitch="${path_prefix}/toy_softswitch"

input_file=""

verbose="0"
timeout=""

function print_usage()
{
    echo "execute_elf sourceFile"
    echo ""
    echo "  sourceFile : elf file to use"
    echo ""
    echo "  --timeout=seconds : terminate hostlink after this many seconds and return 124 exit code"
    echo ""
    echo "  -v | --verbose : Print debug information to stderr"
    echo ""
    echo "  -keep-v : Don't delete code.v and data.v after execution"
}

# https://stackoverflow.com/a/31443098
while [ "$#" -gt 0 ]; do
  case "$1" in
    --help) print_usage; exit 1;;
  
    -i) input_file="$2"; shift 2;;
    --input=*) input_file="${1#*=}"; shift 1;;
    --input) echo "$1 requires an argument" >&2; exit 1;;
    
    --timeout=*) timeout="${1#*=}"; shift 1;;
    --timeout) echo "$1 requires an argument" >&2; exit 1;;
  
    -v|--verbose)  verbose=$(($verbose+1)); shift 1;;
    
    -keep-v)  keep_v=1; shift 1;;

    --*) echo "unknown long option: $1" >&2; exit 1;;
    
    *) if [[ "${input_file}" == "" ]] ; then
        input_file="$1"; shift 1;
       else
        echo "unexpected argument: $1" >&2; exit 1;
       fi;;
  esac
done

temp_dir=$(mktemp -d)

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Files:"
    >&2 echo "  Input elf file = ${input_file}"
    >&2 echo "Paths:"
    >&2 echo "  graph_schema path = ${path_to_graph_schema}"
    >&2 echo "  toy_softswitch path = ${path_to_softswitch}"
    >&2 echo "  tinsel path = ${path_to_tinsel}"
    >&2 echo "  temp path = ${temp_dir}"
fi

if [[ ! -f ${path_to_tinsel}/bin/hostlink ]] ; then
    >&2 echo "hostlink doesn't exist, trying to build it."
    (cd ${path_to_tinsel} && make -c hostlink)
    RES=$?
    if [[ $RES -ne 0 ]] ; then
        >&2 echo "Build of hostlink failed."
        exit 1
    fi
fi

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Configuring tinsel"
fi


(cd ${path_to_tinsel} && make -c de5 download-sof)
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while trying to configure FPGA."
    exit 1
fi

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Extracting code.v and data.v"
fi

riscv64-unknown-elf-objcopy -O verilog --only-section=.text ${input_file} ${temp_dir}/code.v
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while trying to extra code.v"
    exit 1
fi

riscv64-unknown-elf-objcopy -O verilog --remove-section=.text --set-section-flags .bss=alloc,load,contents ${input_file} ${temp_dir}/data.v
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while trying to extra data.v"
    exit 1
fi

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Running hostlink"
fi

if [[ "${QUARTUS_ROOTDIR}" == "" ]] ; then
    >&2 echo "Env variable QUARTUS_ROOTDIR is not set."
    exit 1
fi

${hostlink}=${path_to_tinsel}/bin/hostlink
if [[ "${timeout}" != "" ]] ; then
    hostlink="/usr/bin/timeout ${timeout} -k ${timeout} "
fi

LD_LIBRARY_PATH=${QUARTUS_ROOTDIR}/linux64 \
    ${hostlink} \
    ${temp_dir}/code.v ${temp_dir}/data.v \
    -n 230400 \
    -p
RES=$?
if [[ ( $RES -eq 124 ) || ( $RES -eq 133 ) ]] ; then
    >&2 echo "hostlink timed out with code $RES";
    exit 124
elif [[ $RES -ne 1 ]] ; then
    >&2 echo "hostlink return error code $RES";
    exit 1
fi

if [[ ${keep_v} -ne 1 ]] ; then
    rm ${temp_dir}/*.v
fi

