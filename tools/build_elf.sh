#!/bin/bash

script_prefix="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
path_prefix="$(dirname "$(dirname "${script_prefix}" )" )"

path_to_tinsel="${path_prefix}/tinsel"
path_to_graph_schema="${path_prefix}/graph_schema"
path_to_softswitch="${path_prefix}/toy_softswitch"

output_file="tinsel.elf"
input_file=""
threads="1024"
placement="default"
contraction="dense"

keep_cpp="0"
verbose="0"
release_build="0"

hardware_log_level=""
hardware_assert_enable=""


function print_usage()
{
    echo "build_elf [-o destFile] [-p placementMode] sourceFile"
    echo ""
    echo "  sourceFile : XML instance to read from, use - for stdin"
    echo ""
    echo "  -o | --output=destFile : Name of the elf to create"
    echo ""
    echo "  --threads=n : Number of active threads to target"
    echo ""
    echo "  --placement=method : Placement method to use"
    echo "         cluster : generate a new clustered placement based on thread count"
    echo "         random : use a different placement each time"
    echo ""
    echo "  --contraction=method : How to place n threads on the h hardware threads"
    echo "         dense : map active thread i to hardware thread i"
    echo "         sparse : map active threads i to hardware thread floor(i*(h/n))"
    echo "         random : select a random subset"
    echo ""
    echo "  --hardware-log-level=level : Minimum log-level to enable in compiled elf"
    echo "          0 : all logging code is disabled (minimal code)"
    echo "          i : logging enabled for at most level i"
    echo "              default is 4 (essentially all debug will come out)"
    echo ""
    echo "  --hardware-assert-enable=value : Whether to check assertions in hardware"
    echo "          0 : debug checks ignored at run-time (faster code, smaller code)"
    echo "          1 : check assertions at run-time (bigger but safer code)"
    echo "              default is 1 (check as much as possible)"
    echo ""
    echo "  --release : create a release build, designed to be as small and fast as possible"
    echo "         implied settings are:"
    echo "            --hardware-log-level=0"
    echo "            --hardware-assert-enable=0"
    echo ""
    echo "  -v | --verbose : Print debug information to stderr"
    echo ""
    echo "  --keep-cpp : Leave the intermediate cpp files in place"
    
}

# https://stackoverflow.com/a/31443098
while [ "$#" -gt 0 ]; do
  case "$1" in
    --help) print_usage; exit 1;;
  
    -i) input_file="$2"; shift 2;;
    --input=*) input_file="${1#*=}"; shift 1;;
    --input) echo "$1 requires an argument" >&2; exit 1;;
  
    -o) output_file="$2"; shift 2;;
    --output=*) output_file="${1#*=}"; shift 1;;
    --output) echo "$1 requires an argument" >&2; exit 1;;
    
    --threads=*) threads="${1#*=}"; shift 1;;
    --threads) echo "$1 requires an argument" >&2; exit 1;;
    
    --placement=*) placement="${1#*=}"; shift 1;;
    --placement) echo "$1 requires an argument" >&2; exit 1;;
    
    --contraction=*) contraction="${1#*=}"; shift 1;;
    --contraction) echo "$1 requires an argument" >&2; exit 1;;
    
    --keep-cpp) keep_cpp="1"; shift 1;;
    
    --release) release_build="1"; shift 1;;
    
    --hardware-log-level=*) hardware_log_level="${1#*=}"; shift 1;;
    --hardware-log-level) echo "$1 requires an argument" >&2; exit 1;;
    
    --hardware-assert-enable=*) hardware_assert_enable="${1#*=}"; shift 1;;
    --hardware-assert-enable) echo "$1 requires an argument" >&2; exit 1;;
    
    
    -v|--verbose)  verbose=$(($verbose+1)); shift 1;;

    --*) echo "unknown long option: $1" >&2; exit 1;;
    
    *) if [[ "${input_file}" == "" ]] ; then
        input_file="$1"; shift 1;
       else
        echo "unexpected argument: $1" >&2; exit 1;
       fi;;
  esac
done

if [[ "${output_file}" == "-" ]] ; then
    >&2 echo "Output elf target must be a real file."
fi
  
temp_dir=$(mktemp -d)

if [[ "${release_build}" -eq 1 ]] ; then
    hardware_log_level=${hardware_log_level:-0}
    hardware_assert_enable=${hardware_assert_enable:-0}
fi
hardware_log_level=${hardware_log_level:-4}
hardware_assert_enable=${hardware_assert_enable:-1}

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Files:"
    >&2 echo "  Input XML instance = ${input_file}"
    >&2 echo "  Output elf file = ${output_file}"
    >&2 echo "Paths:"
    >&2 echo "  graph_schema path = ${path_to_graph_schema}"
    >&2 echo "  toy_softswitch path = ${path_to_softswitch}"
    >&2 echo "  tinsel path = ${path_to_tinsel}"
    >&2 echo "  temp path = ${temp_dir}"
    >&2 echo "Params:"
    >&2 echo "  threads = ${threads}"
    >&2 echo "  placement = ${placement}"
    >&2 echo "  contraction = ${contraction}"
    >&2 echo "  keep_cpp = ${keep_cpp}"
    >&2 echo "  hardware_log_level = ${hardware_log_level}"
    >&2 echo "  hardware_assert_enable = ${hardware_assert_enable}"
fi

if [[ "${placement}" == "cluster" ]] ; then
    part_file="${temp_dir}/partitioned.xml" 
    if [[ verbose -gt 0 ]] ; then
        >&2 echo "Generating a new partitioning using metis with ${partition} clusters."
        >&2 echo "   Partitioned file=${part_file}."
    fi
    ${path_to_graph_schema}/tools/render_graph_as_metis.py ${input_file} ${threads} > ${part_file}
    input_file=${part_file}
elif [[ "${placement}" == "random" ]] ; then
    # Do nothing
    true
else
    >&2 echo "Didn't understand placement method ${placement}."
    exit 1
fi

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Generating cpp soft-switch configuration files"
fi

${path_to_graph_schema}/tools/render_graph_as_softswitch.py \
    --log-level INFO \
    ${input_file} \
    --threads ${threads} \
    --hardware-threads=1024 \
    --contraction=${contraction} \
    --dest ${temp_dir}
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while generating soft-switch configuration files"
    exit 1
fi

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Compiling soft-switch configuration to elf"
fi

make -f ${path_to_softswitch}/src/true_tinsel/Makefile \
    POETS_TOY_SOFTSWITCH_ROOT=${path_to_softswitch} \
    POETS_TINSEL_ROOT=${path_to_tinsel} \
    APPLICATION=${temp_dir} \
    DESIGN_ELF=${output_file} \
    HARDWARE_LOG_LEVEL=${hardware_log_level} \
    HARDWARE_ASSERT_ENABLE=${hardware_assert_enable} \
    ${output_file}
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while building elf."
    exit 1
fi


if [[ ${keep_cpp} -ne 1 ]] ; then
    rm ${temp_dir}/*.cpp
    rm ${temp_dir}/*.hpp
fi
