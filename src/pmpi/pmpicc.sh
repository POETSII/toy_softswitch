#!/bin/bash

# For getopt template: https://stackoverflow.com/a/29754866

OPTIONS=p:o:v
LONGOPTIONS=platform:,output:,verbose

# -temporarily store output to be able to check for errors
# -e.g. use “--options” parameter by name to activate quoting/enhanced mode
# -pass arguments only via   -- "$@"   to separate them correctly
PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTIONS --name "$0" -- "$@")
if [[ $? -ne 0 ]]; then
    # e.g. $? == 1
    #  then getopt has complained about wrong arguments to stdout
    exit 2
fi

# read getopt’s output this way to handle the quoting right:
eval set -- "$PARSED"

poets_root="/POETS"

known_platforms="unix"
platform="unix"
verbose=0
outFile="a.out"

# now enjoy the options in order and nicely split until we see --
while true; do
    case "$1" in
        -p|--platform)
            platform="$2"
            shift 2
            ;;
        -v|--verbose)
            verbose=1
            shift
            ;;
        -o|--output)
            outFile="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "$0 : Internal error in compiler driver script"
            exit 3
            ;;
    esac
done

# Currently we only support one input source file. Can easily
# be extended
if [[ $# -ne 1 ]]; then
    echo "$0: A single source file is required."
    exit 4
fi
inFile=$1

function error {
    >&2 "Error : $1";
    exit 1
}

function log {
    if [[ $verbose -gt 0 ]] ; then
        >&2 echo "$1"
    fi
}


log "verbose: $v, platform: $platform, out: $outFile, in: $inFile"

log "Choosing platform specific sources"
platformCppFlags="-std=c++11"
platformLinkFlags=""
platformSources=""
platformCompiler=""
if [[ "$platform" == "unix" ]] ; then
    log "Selecting unix platform (so use threads and DGRAM unix sockets)"
    platformSources+=" ${poets_root}/toy_softswitch/src/tinsel/tinsel_on_unix.cpp"
    platformSources+=" ${poets_root}/toy_softswitch/src/tinsel/tinsel_mbox.hpp"
    platformCompiler="g++"
    platformCppFlags+=" -I ${poets_root}/toy_softswitch/include"
    platformCppFlags+=" -I ${poets_root}/toy_softswitch/src/pmpi/include"
    platformCppFlags+=" -DNO_TINSEL_API_SHIM=1"
    platformCppFlags+=" -pthread"
else
    error "Didn't understand platform $platform. Known platforms are $knownPlatforms"
fi

cmd="$platformCompiler $platformCppFlags -o $outFile $platformSources $inFile $platformLinkFlags"
log "Compiling executable"
log "$cmd"
$cmd

