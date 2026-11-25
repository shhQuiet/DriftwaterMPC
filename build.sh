#!/bin/sh
set -eu

project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${MPC_SDK:?Set MPC_SDK to the Akai MPC Plugin Builder SDK checkout}"
runtime="$project/generated/rnbo/rnbo"
export CPLUS_INCLUDE_PATH="$runtime:$runtime/src:$runtime/common${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
python3 "$MPC_SDK/tools/build.py" "$project" --define RNBO_USE_FLOAT32 "$@"

for arg in "$@"; do
    case "$arg" in
        native|desktop|--target=native|--target=desktop|--no-ui) exit 0 ;;
    esac
done
bundle="$project/build/Elsewave - VST - Driftwater"
python3 "$project/space-skin.py" "$bundle"
for arg in "$@"; do
    [ "$arg" != --no-validate ] || exit 0
done
python3 "$MPC_SDK/tools/validate.py" "$bundle" --params "$project/build/params.json"
