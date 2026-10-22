#!/bin/bash
# Probes the kernel instead of reading /proc/modules, so it works whether netfilter is built in or
# loaded as modules. Failures only warn: most work never reaches iptc_commit.
set -e

check_table() {
    local table="$1"
    if iptables -t "$table" -L -n >/dev/null 2>&1; then
        echo "[sandbox] ok: iptable_$table"
    else
        echo "[sandbox] WARNING: iptable_$table not available — rules in '$table' cannot be applied"
        echo "[sandbox]          On the host: sudo modprobe ip_tables iptable_$table"
    fi
}

echo "[sandbox] checking kernel capabilities..."
check_table filter
check_table mangle
check_table nat
echo "[sandbox] ready — project root is mounted at /workspace"

exec "$@"
