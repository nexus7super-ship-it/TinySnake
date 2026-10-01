#!/bin/sh
# Runs the end-to-end tests on a private Xvfb server, never on the desktop.
#
# Usage: tests/run-e2e.sh <e2e binary> <TinySnake binary>...

set -e

e2e=$1
shift

if ! command -v Xvfb >/dev/null 2>&1; then
    echo "The end-to-end tests need Xvfb. Install it with:"
    echo "  Ubuntu / Debian: sudo apt install xvfb"
    echo "  Fedora:          sudo dnf install xorg-x11-server-Xvfb"
    echo "  Arch Linux:      sudo pacman -S xorg-server-xvfb"
    exit 1
fi

# Xvfb picks a free display number and writes it to file descriptor 3.
display=$(mktemp)
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$display" 2>/dev/null &
xvfb=$!
trap 'kill $xvfb; rm -f "$display"' EXIT

for _ in $(seq 50); do
    [ -s "$display" ] && break
    sleep 0.1
done
if [ ! -s "$display" ]; then
    echo "Xvfb didn't start"
    exit 1
fi
export DISPLAY=":$(cat "$display")"

status=0
for binary; do
    "$e2e" "$binary" || status=1
done
exit $status
