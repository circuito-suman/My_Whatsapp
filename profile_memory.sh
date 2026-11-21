#!/usr/bin/env bash
# WhatsApp Desktop - Real-Time Linux Memory & Heap Profiler
set -euo pipefail

echo "================================================================="
echo " WhatsApp Desktop Memory & V8 Heap Tracker"
echo " Monitoring /proc/\$PID/status & glibc arenas (Press Ctrl+C to stop)"
echo "================================================================="
printf "%-8s %-14s %-14s %-14s %-12s\n" "PID" "Private Anon" "Shared Libs" "Total RSS" "Swap"
printf "%s\n" "-----------------------------------------------------------------"

while true; do
    PID=$(pgrep -f "WHATSAPP_REBASE" | head -n 1 || true)
    if [[ -n "$PID" && -d "/proc/$PID" ]]; then
        ANON=$(grep -i "RssAnon:" "/proc/$PID/status" 2>/dev/null | awk '{printf "%.2f MB", $2/1024}' || echo "N/A")
        FILE=$(grep -i "RssFile:" "/proc/$PID/status" 2>/dev/null | awk '{printf "%.2f MB", $2/1024}' || echo "N/A")
        RSS=$(grep -i "VmRSS:" "/proc/$PID/status" 2>/dev/null | awk '{printf "%.2f MB", $2/1024}' || echo "N/A")
        SWAP=$(grep -i "VmSwap:" "/proc/$PID/status" 2>/dev/null | awk '{printf "%.2f MB", $2/1024}' || echo "N/A")
        printf "%-8s %-14s %-14s %-14s %-12s\n" "$PID" "$ANON" "$FILE" "$RSS" "$SWAP"
    else
        echo "Waiting for WHATSAPP_REBASE process to start..."
    fi
    sleep 3
done
