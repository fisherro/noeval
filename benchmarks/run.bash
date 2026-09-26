#!/bin/bash
# Time the benchmarks.
#
# Usage: benchmarks/run.bash [-n runs] [-b binary] [-c previous] [name...]
#
#   -n runs      Run each benchmark this many times (default 5).
#   -b binary    The noeval executable to time (default bin/noeval).
#   -c previous  Compare against results saved from an earlier run.
#   name...      The benchmarks to run (default all of them).
#
# Each benchmark is a .noeval file in this directory, run as a script with
# --skip-tests so the C++ tests aren't timed. If NAME.stdin exists, it is the
# benchmark's standard input. Times include starting noeval and loading the
# library; the startup benchmark measures just that. Peak memory is the highest
# peak resident set size of any of the runs, which noeval reports when
# NOEVAL_REPORT_PEAK_MEMORY is set.
#
# The results are a table on stdout. To measure a change, save the results
# from before it and compare the results from after it against them:
#
#   benchmarks/run.bash > before.txt
#   (make the change and rebuild)
#   benchmarks/run.bash -c before.txt
#
# A machine's speed can drift between runs, so saved results can mislead.
# For a reliable comparison, run an earlier version's own copy of this script
# (for example, from a git worktree) just before this one, and then in the
# other order. -b isn't enough for that: this script runs from its own
# checkout, and noeval loads the library from there.

set -euo pipefail

runs=5
binary=bin/noeval
previous=
while getopts "n:b:c:" opt; do
    case $opt in
        n) runs=$OPTARG ;;
        b) binary=$OPTARG ;;
        c) previous=$OPTARG ;;
        *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

# noeval loads the library by a path relative to the top of the repository.
cd "$(dirname "$0")/.."

if [[ ! -x $binary ]]; then
    echo "$binary not found. Build it with make." >&2
    exit 1
fi

if (( $# > 0 )); then
    names=("$@")
else
    names=()
    for file in benchmarks/*.noeval; do
        name=$(basename "$file" .noeval)
        [[ $name == common ]] || names+=("$name")
    done
fi

# The previous median for each benchmark
declare -A before
if [[ -n $previous ]]; then
    while read -r name _ median _; do
        [[ $median =~ ^[0-9]+$ ]] && before[$name]=$median
    done < "$previous"
fi

# Milliseconds since the epoch
now_ms() {
    local t=${EPOCHREALTIME/./}
    echo $(( t / 1000 ))
}

printf "%-20s %8s %10s %8s" benchmark "min ms" "median ms" "peak MB"
[[ -n $previous ]] && printf " %10s" "vs. before"
printf "\n"

status=0
for name in "${names[@]}"; do
    script=benchmarks/$name.noeval
    input=benchmarks/$name.stdin
    [[ -f $input ]] || input=/dev/null
    if [[ ! -f $script ]]; then
        echo "No benchmark named $name" >&2
        status=1
        continue
    fi

    times=()
    peak_kb=0
    failed=
    for (( i = 0; i < runs; i++ )); do
        start=$(now_ms)
        if ! output=$(NOEVAL_REPORT_PEAK_MEMORY=1 "$binary" --skip-tests "$script" < "$input" 2>&1); then
            failed=1
            break
        fi
        times+=($(( $(now_ms) - start )))
        # noeval reports "peak memory: N kB" as it exits. (It may follow
        # output that didn't end its line.)
        kb=$(sed -n 's/.*peak memory: \([0-9]*\) kB$/\1/p' <<< "$output" | tail -n 1)
        (( ${kb:-0} > peak_kb )) && peak_kb=$kb
    done
    if [[ -n $failed ]]; then
        printf "%-20s %8s\n" "$name" failed
        tail -n 5 <<< "$output" >&2
        status=1
        continue
    fi

    sorted=($(printf "%s\n" "${times[@]}" | sort -n))
    min=${sorted[0]}
    median=${sorted[$(( runs / 2 ))]}
    # The highest peak of any run, in megabytes to one decimal place
    tenths=$(( (peak_kb * 10 + 512) / 1024 ))
    printf "%-20s %8d %10d %8s" "$name" "$min" "$median" \
        "$(( tenths / 10 )).$(( tenths % 10 ))"
    if [[ -n $previous ]]; then
        if [[ -n ${before[$name]:-} ]] && (( before[$name] > 0 )); then
            # The ratio to two decimal places, using integer arithmetic
            ratio=$(( (median * 100 + before[$name] / 2) / before[$name] ))
            printf " %10s" "$(( ratio / 100 )).$(printf %02d $(( ratio % 100 )))x"
        else
            printf " %10s" "-"
        fi
    fi
    printf "\n"
done

exit $status
