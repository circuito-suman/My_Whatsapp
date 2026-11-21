#!/usr/bin/env bash
# WhatsApp Desktop - GDB Interactive Debugging & Analysis Harness
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/build/WHATSAPP_REBASE"

if [[ ! -f "$BIN" ]]; then
    BIN="$SCRIPT_DIR/WHATSAPP_REBASE"
fi

echo "================================================================="
echo " Starting WhatsApp Desktop Native under GDB"
echo " Binary: $BIN"
echo " Symbols: RelWithDebInfo / Debug"
echo "================================================================="
echo "GDB Tips:"
echo "  - Type 'r' or 'run' to start"
echo "  - Type 'bt' on pause or crash to see full backtrace"
echo "  - Type 'info threads' to list active threads"
echo "  - Type 'thread apply all bt' to see backtrace of all WebEngine threads"
echo "  - Type 'q' to quit"
echo "================================================================="

# Set environment
export QT_LOGGING_RULES="qt.webenginecontext.debug=false"

exec gdb -q \
    -ex "set confirm off" \
    -ex "set pagination off" \
    -ex "set print pretty on" \
    -ex "set print thread-events off" \
    -ex "handle SIGPIPE nostop noprint pass" \
    -ex "run" \
    -ex "thread apply all bt" \
    -ex "quit" \
    --args "$BIN" "$@"
