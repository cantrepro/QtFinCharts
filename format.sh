#!/usr/bin/env bash
set -euo pipefail

mapfile -d '' files < <(
    find . -type f \
        \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.hh' -o -name '*.hxx' \) \
        -print0
)

if ((${#files[@]} == 0)); then
    exit 0
fi

clang-format -i -style=webkit "${files[@]}"
