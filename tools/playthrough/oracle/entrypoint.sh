#!/usr/bin/env bash
set -e

mkdir -p /lab/wine32 /lab/out
chmod 777 /lab/wine32 /lab/out || true

if [ "$(id -u)" = "0" ]; then
    chown -R lab:lab /lab/wine32 /lab/out 2>/dev/null || true
    exec gosu lab "$@"
else
    exec "$@"
fi
