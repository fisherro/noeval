#!/bin/bash
# Report the builtins and library definitions that noeval-reference.md
# doesn't mention (as `name`).

set -euo pipefail
cd "$(dirname "$0")"

script=$(mktemp --suffix=.noeval)
trap 'rm -f "$script"' EXIT
cat > "$script" <<'NOEVAL'
(for-each (lambda (name) (displayln "name: " name))
          (append (environment-names (get-builtins-environment))
                  (environment-names (get-top-level-environment))))
NOEVAL

missing=0
while read -r name; do
    if ! grep -qF -- "\`$name\`" noeval-reference.md; then
        echo "$name"
        missing=$((missing + 1))
    fi
done < <(bin/noeval --skip-tests "$script" | sed -n 's/^name: //p')

echo "$missing name(s) missing from noeval-reference.md" >&2
