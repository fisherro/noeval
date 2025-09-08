#!/bin/zsh

{
    podman run --rm -v $(PWD):/workspace -w /workspace gcc-rlf:latest ./run-dependency-checker.bash &
    noeval_pid=$!
    podman stats &
    stats_pid=$!
    wait $noeval_pid
    kill $stats_pid 2>/dev/null
}