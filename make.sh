#!/usr/bin/env bash
# 使い方: ./make.sh [target...]   (既定: yave_app)
set -euo pipefail

cd "$(dirname "$0")"

preset=win-mingw
build_dir="build/$preset"

if (($# == 0)); then
    set -- yave_app
fi

# 実行中の exe はロックされ、リンクが Permission denied で失敗する
if tasklist //FI "IMAGENAME eq yave_app.exe" //NH 2>/dev/null | grep -qi "yave_app.exe"; then
    echo "yave_app.exe is running. Close it and run again." >&2
    exit 1
fi

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake --preset "$preset"
fi

cmake --build "$build_dir" --target "$@"
