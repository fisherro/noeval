#!/bin/bash
# Time the benchmarks.
#
# Usage: benchmarks/run.bash [-n runs] [-b binary] [-c previous] [name...]
#        benchmarks/run.bash -a other [-n runs] [-b binary] [name...]
#
#   -n runs      Run each benchmark this many times (default 5).
#   -b binary    The noeval executable to time (default bin/noeval).
#   -c previous  Compare against results saved from an earlier run.
#   -a other     Compare against another checkout (the top of its tree, with
#                bin/noeval built), alternating between them run by run.
#   name...      The benchmarks to run (default all of them).
#
# Each benchmark is a .noeval file in this directory, run as a script with
# --skip-tests so the C++ tests aren't timed. If NAME.stdin exists, it is the
# benchmark's standard input. Times include starting noeval and loading the
# library; the startup benchmark measures just that. Peak memory is the highest
# peak resident set size of any of the runs, which noeval reports when
# NOEVAL_REPORT_PEAK_MEMORY is set.
#
# The results are a table on stdout. To measure a change, compare against the
# code from before it with -a, for example with a git worktree:
#
#   git worktree add ../noeval-before HEAD
#   make -C ../noeval-before
#   (make the change and rebuild)
#   benchmarks/run.bash -a ../noeval-before
#
# With -a, each benchmark runs the two checkouts in turn, swapping which goes
# first each round, so a period when the machine is slower affects both
# alternately. Both run this checkout's copy of the benchmark, so they time the
# same code, but each runs from its own checkout and so loads its own library.
# The ratios are this checkout's time divided by the other's, of the minimums
# and of the medians. On a shared machine, runs can be faster as well as
# slower than usual, so neither ratio is reliable for a difference of a few
# percent. For that, count the instructions executed, which doesn't depend on
# the machine's speed, for example with
#   valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file=/dev/null
#       bin/noeval --skip-tests benchmarks/NAME.noeval
# run in each checkout.
#
# -c compares against results saved earlier (benchmarks/run.bash > before.txt),
# but a machine's speed can drift a lot between runs, so a difference it shows
# may not be real. -b isn't enough for a comparison either: noeval loads the
# library from the checkout it runs in, so -b would time an earlier binary
# with the current library.

set -euo pipefail

runs=5
binary=bin/noeval
previous=
other=
while getopts "n:b:c:a:" opt; do
    case $opt in
        n) runs=$OPTARG ;;
        b) binary=$OPTARG ;;
        c) previous=$OPTARG ;;
        a) other=$OPTARG ;;
        *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

if [[ -n $other && -n $previous ]]; then
    echo "-a and -c can't be used together." >&2
    exit 2
fi
if [[ -n $other ]]; then
    if [[ ! -x $other/bin/noeval ]]; then
        echo "$other/bin/noeval not found. Build it with make -C $other." >&2
        exit 1
    fi
    other=$(cd "$other" && pwd)
fi

# noeval loads the library by a path relative to the top of the repository.
cd "$(dirname "$0")/.."
here=$(pwd)

if [[ ! -x $binary ]]; then
    echo "$binary not found. Build it with make." >&2
    exit 1
fi
binary=$(cd "$(dirname "$binary")" && pwd)/$(basename "$binary")

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

# Run a benchmark once: run_once directory binary script input. Sets elapsed
# (in milliseconds), kb (the peak memory noeval reported), and output. Returns
# nonzero if noeval failed.
run_once() {
    local start
    start=$(now_ms)
    if ! output=$(cd "$1" && NOEVAL_REPORT_PEAK_MEMORY=1 "$2" --skip-tests "$3" < "$4" 2>&1); then
        return 1
    fi
    elapsed=$(( $(now_ms) - start ))
    # noeval reports "peak memory: N kB" as it exits. (It may follow output
    # that didn't end its line.)
    kb=$(sed -n 's/.*peak memory: \([0-9]*\) kB$/\1/p' <<< "$output" | tail -n 1)
    kb=${kb:-0}
}

# The minimum and median of the arguments: sets min and median.
min_and_median() {
    local sorted
    sorted=($(printf "%s\n" "$@" | sort -n))
    min=${sorted[0]}
    median=${sorted[$(( $# / 2 ))]}
}

# Kilobytes as megabytes to one decimal place
megabytes() {
    local tenths=$(( ($1 * 10 + 512) / 1024 ))
    echo "$(( tenths / 10 )).$(( tenths % 10 ))"
}

# numerator / denominator to two decimal places, using integer arithmetic
ratio() {
    if (( $2 > 0 )); then
        local r=$(( ($1 * 100 + $2 / 2) / $2 ))
        echo "$(( r / 100 )).$(printf %02d $(( r % 100 )))x"
    else
        echo "-"
    fi
}

if [[ -n $other ]]; then
    printf "%-20s %9s %9s %7s %10s %10s %7s %9s\n" benchmark \
        "other min" "this min" "ratio" "other med" "this med" "ratio" "peak MB"
elif [[ -n $previous ]]; then
    printf "%-20s %8s %10s %8s %10s\n" benchmark "min ms" "median ms" "peak MB" "vs. before"
else
    printf "%-20s %8s %10s %8s\n" benchmark "min ms" "median ms" "peak MB"
fi

status=0
for name in "${names[@]}"; do
    script=$here/benchmarks/$name.noeval
    input=$here/benchmarks/$name.stdin
    [[ -f $input ]] || input=/dev/null
    if [[ ! -f $script ]]; then
        echo "No benchmark named $name" >&2
        status=1
        continue
    fi

    times=()
    other_times=()
    peak_kb=0
    other_peak_kb=0
    failed=
    for (( i = 0; i < runs; i++ )); do
        if [[ -z $other ]]; then
            run_once "$here" "$binary" "$script" "$input" || { failed=this; break; }
            times+=("$elapsed")
            (( kb > peak_kb )) && peak_kb=$kb
            continue
        fi
        # Alternate which checkout goes first.
        for which in $( (( i % 2 == 0 )) && echo "other this" || echo "this other" ); do
            if [[ $which == other ]]; then
                run_once "$other" "$other/bin/noeval" "$script" "$input" || { failed=other; break 2; }
                other_times+=("$elapsed")
                (( kb > other_peak_kb )) && other_peak_kb=$kb
            else
                run_once "$here" "$binary" "$script" "$input" || { failed=this; break 2; }
                times+=("$elapsed")
                (( kb > peak_kb )) && peak_kb=$kb
            fi
        done
    done
    if [[ -n $failed ]]; then
        printf "%-20s %8s\n" "$name" "failed${other:+ ($failed)}"
        tail -n 5 <<< "$output" >&2
        status=1
        continue
    fi

    min_and_median "${times[@]}"
    if [[ -n $other ]]; then
        this_min=$min
        this_median=$median
        min_and_median "${other_times[@]}"
        printf "%-20s %9d %9d %7s %10d %10d %7s %9s\n" "$name" \
            "$min" "$this_min" "$(ratio "$this_min" "$min")" \
            "$median" "$this_median" "$(ratio "$this_median" "$median")" \
            "$(megabytes "$other_peak_kb")/$(megabytes "$peak_kb")"
        continue
    fi

    printf "%-20s %8d %10d %8s" "$name" "$min" "$median" "$(megabytes "$peak_kb")"
    if [[ -n $previous ]]; then
        printf " %10s" "$(ratio "$median" "${before[$name]:-0}")"
    fi
    printf "\n"
done

exit $status
