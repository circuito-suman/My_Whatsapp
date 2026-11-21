#!/usr/bin/env bash
# WhatsApp Desktop - Linux perf Hardware Counter Profiler
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/build/WHATSAPP_REBASE"

if [[ ! -f "$BIN" ]]; then
    BIN="$SCRIPT_DIR/WHATSAPP_REBASE"
fi

echo "================================================================="
echo " Starting WhatsApp Desktop with Linux perf stat..."
echo " Measuring CPU cycles, cache references, branch misses, page faults"
echo "================================================================="

exec perf stat \
    -e cycles,instructions,cache-references,cache-misses,branches,branch-misses,page-faults \
    "$BIN" "$@"
