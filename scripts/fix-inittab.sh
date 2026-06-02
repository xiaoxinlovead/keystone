#!/bin/sh
# Replace getty with direct shell for auto-login
sed -i 's|^console::respawn:/sbin/getty.*|console::respawn:-/bin/sh|' "$1"
