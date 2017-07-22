#!/usr/bin/env bash

set -e

script_prefix="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
path_prefix="$(dirname "$(dirname "${script_prefix}" )" )"

path_to_tinsel="${path_prefix}/tinsel"
path_to_graph_schema="${path_prefix}/graph_schema"
path_to_softswitch="${path_prefix}/toy_softswitch"

output_file="tinsel.elf"
input_file=""
measure_file="tinsel.build.csv"
threads="1024"
hardware_threads="1024" # not currently user configurable
placement="random"
contraction="dense"

keep_cpp="0"
verbose="0"
release_build="0"

hardware_log_level=""
hardware_assert_enable=""
hardware_intrathread_send_enable=""

profile_softswitch_render=""

function print_usage()
{
    echo "build_elf [-o destFile] [-p placementMode] sourceFile"
    echo ""
    echo "  sourceFile : XML instance to read from, use - for stdin"
    echo ""
    echo "  -o | --output=destFile : Name of the elf to create (default: tinsel.elf)"
    echo ""
    echo "  --measure=measureFile : Where to record build measurements (default: tinsel.buil.csv)"
    echo ""
    echo "  --threads=n : Number of active threads to target"
    echo ""
    echo "  --placement=method : Placement method to use"
    echo "         cluster : generate a new clustered placement based on thread count"
    echo "         random : use a different placement each time (default)"
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
    echo "  --hardware-intrathread-send-enable=value : Whether to allows intrathread messages to bypass mailbox"
    echo "          0 : everything goes via mailbox"
    echo "          1 : deliver intra-thread messages directly"
    echo "              default is 0 (all via mailbox)"
    echo ""
    echo "  --release : create a release build, designed to be as small and fast as possible"
    echo "         implied settings are:"
    echo "            --hardware-log-level=0"
    echo "            --hardware-assert-enable=0"
    echo "            --hardware-intrathread-send-enable=1"
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

    --measure=*) measure_file="${1#*=}"; shift 1;;
    --measure) echo "$1 requires an argument" >&2; exit 1;;
    
    --threads=*) threads="${1#*=}"; shift 1;;
    --threads) echo "$1 requires an argument" >&2; exit 1;;
    
    --placement=*) placement="${1#*=}"; shift 1;;
    --placement) echo "$1 requires an argument" >&2; exit 1;;
    
    --contraction=*) contraction="${1#*=}"; shift 1;;
    --contraction) echo "$1 requires an argument" >&2; exit 1;;

    --destination-order=*) destination_order="${1#*=}"; shift 1;;
    --destination-order) echo "$1 requires an argument" >&2; exit 1;;
    
    --keep-cpp) keep_cpp="1"; shift 1;;
    
    --release) release_build="1"; shift 1;;
    
    --hardware-log-level=*) hardware_log_level="${1#*=}"; shift 1;;
    --hardware-log-level) echo "$1 requires an argument" >&2; exit 1;;
    
    --hardware-assert-enable=*) hardware_assert_enable="${1#*=}"; shift 1;;
    --hardware-assert-enable) echo "$1 requires an argument" >&2; exit 1;;

    --hardware-intrathread-send-enable=*) hardware_intrathread_send_enable="${1#*=}"; shift 1;;
    --hardware-intrathread-send-enable) echo "$1 requires an argument" >&2; exit 1;;
    
    --profile-softswitch-render=*) profile_softswitch_render="${1#*=}"; shift 1;;
    --profile-softswitch-render) echo "$1 requires an argument" >&2; exit 1;;

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
    >&2 echo "Output elf target must be a real file, can't use stdout."
    exit 1
fi
  
temp_dir=$(mktemp -d)

if [[ "${release_build}" -eq 1 ]] ; then
    hardware_log_level=${hardware_log_level:-0}
    hardware_assert_enable=${hardware_assert_enable:-0}
    hardware_intrathread_send_enable=${hardware_intrathread_send_enable:-1}
fi
hardware_log_level=${hardware_log_level:-4}
hardware_assert_enable=${hardware_assert_enable:-1}
hardware_intrathread_send_enable=${hardware_intrathread_send_enable:-0}

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Files:"
    >&2 echo "  Input XML instance = ${input_file}"
    >&2 echo "  Output elf file = ${output_file}"
    >&2 echo "  Measure file = ${output_file}"
    >&2 echo "Paths:"
    >&2 echo "  graph_schema path = ${path_to_graph_schema}"
    >&2 echo "  toy_softswitch path = ${path_to_softswitch}"
    >&2 echo "  tinsel path = ${path_to_tinsel}"
    >&2 echo "  temp path = ${temp_dir}"
    >&2 echo "Params:"
    >&2 echo "  threads = ${threads}"
    >&2 echo "  placement = ${placement}"
    >&2 echo "  contraction = ${contraction}"
    >&2 echo "  destination_order = ${destination_order}"
    >&2 echo "  keep_cpp = ${keep_cpp}"
    >&2 echo "  hardware_log_level = ${hardware_log_level}"
    >&2 echo "  hardware_assert_enable = ${hardware_assert_enable}"
    >&2 echo "  hardware_intrathread_send_enable = ${hardware_intrathread_send_enable}"
fi

if [[ -f ${measure_file} ]] ; then
    rm ${measure_file}
fi


echo "buildTinselSettings, threads, ${threads}, -" >> ${measure_file}
echo "buildTinselSettings, hardware_threads, ${hardware_threads}, -" >> ${measure_file}
echo "buildTinselSettings, placement, ${placement}, -" >> ${measure_file}
echo "buildTinselSettings, contraction, ${contraction}, -" >> ${measure_file}
echo "buildTinselSettings, release_build, ${release_build}, -" >> ${measure_file}
echo "buildTinselSettings, hardware_log_level, ${hardware_log_level}, -" >> ${measure_file}
echo "buildTinselSettings, hardware_assert_enable, ${hardware_assert_enable}, -" >> ${measure_file}
echo "buildTinselSettings, hardware_intrathread_send_enable, ${hardware_intrathread_send_enable}, -" >> ${measure_file}

###################################################################################
## Placement

if [[ "${placement}" == "cluster" ]] ; then
    part_file="${temp_dir}/partitioned.xml" 
    if [[ verbose -gt 0 ]] ; then
        >&2 echo "Generating a new partitioning using metis with ${threads} clusters."
        >&2 echo "   Partitioned file=${part_file}."
    fi
    /usr/bin/time -o ${temp_dir}/partition.time -f %e ${path_to_graph_schema}/tools/render_graph_as_metis.py ${input_file} ${threads} > ${part_file}
    RES=$?
    if [[ $RES -ne 0 ]] ; then
	>&2 echo "Got error code $RES while generating metis clusters"
    fi
    echo "buildTinselPartition, -, $(cat ${temp_dir}/partition.time), sec" >> ${measure_file}
   
    input_file="${part_file}"
elif [[ "${placement}" == "random" ]] ; then
    # Do nothing
    true
else
    >&2 echo "Didn't understand placement method ${placement}."
    exit 1
fi


###################################################################################
## rendering

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Generating cpp soft-switch configuration files"
fi

render_softswitch_command="/usr/bin/time -o ${temp_dir}/build_softswitch.time -f %e python3 "
if [[ "${profile_softswitch_render}" != "" ]] ; then
    render_softswitch_command="${render_softswitch_command} -m cProfile -o ${profile_softswitch_render}"
fi

${render_softswitch_command} ${path_to_graph_schema}/tools/render_graph_as_softswitch.py \
    --log-level INFO \
    ${input_file} \
    --threads ${threads} \
    --hardware-threads=${hardware_threads} \
    --contraction=${contraction} \
    --dest ${temp_dir}
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while generating soft-switch configuration files"
    exit 1
fi
echo "buildSoftswitchRender, -, $(cat ${temp_dir}/build_softswitch.time), sec" >> ${measure_file}

###################################################################################
## compilation

if [[ verbose -gt 0 ]] ; then
    >&2 echo "Compiling soft-switch configuration to elf"
fi

/usr/bin/time -o ${temp_dir}/compile_softswitch.time -f %e make -f ${path_to_softswitch}/src/true_tinsel/Makefile \
    POETS_TOY_SOFTSWITCH_ROOT=${path_to_softswitch} \
    POETS_TINSEL_ROOT=${path_to_tinsel} \
    APPLICATION=${temp_dir} \
    DESIGN_ELF=${output_file} \
    HARDWARE_LOG_LEVEL=${hardware_log_level} \
    HARDWARE_ASSERT_ENABLE=${hardware_assert_enable} \
    HARDWARE_INTRATHREAD_SEND_ENABLE=${hardware_intrathread_send_enable} \
    ${output_file}
RES=$?
if [[ $RES -ne 0 ]] ; then
    >&2 echo "Got error code $RES while building elf."
    exit 1
fi
echo "buildTinselCompile, -, $(cat ${temp_dir}/compile_softswitch.time), sec" >> ${measure_file}


if [[ ${keep_cpp} -ne 1 ]] ; then
    rm ${temp_dir}/*.cpp
    rm ${temp_dir}/*.hpp
fi

