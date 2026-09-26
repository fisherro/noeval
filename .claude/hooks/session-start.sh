#!/bin/bash
# Install what building and testing noeval needs in a Claude Code on the web
# session: GCC 14, Boost, and readline, the same as CI.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

packages=(g++-14 libboost-dev libreadline-dev)

missing=()
for package in "${packages[@]}"; do
    if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null \
            | grep -q "install ok installed"; then
        missing+=("$package")
    fi
done

if [ "${#missing[@]}" -gt 0 ]; then
    # Some third-party apt sources may be unreachable; the Ubuntu ones are
    # all that's needed, so don't fail on the others.
    apt-get update || true
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        --no-install-recommends "${missing[@]}"
fi

# The Makefile sets CXX with :=, so an environment variable can't override
# it. local.mk is the Makefile's untracked place for per-machine overrides.
cd "$CLAUDE_PROJECT_DIR"
if [ ! -e local.mk ]; then
    echo 'CXX := g++-14' > local.mk
fi
