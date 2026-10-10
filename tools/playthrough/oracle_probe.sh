#!/usr/bin/env bash
set -e

mkdir -p /lab/out/oracle
rm -rf /lab/wine && mkdir -p /lab/wine
export WINEPREFIX=/lab/wine
echo "Testing: xvfb-run wine /lab/SIMTOWER.EXE with screenshot capture..."
xvfb-run -a -s "-screen 0 1280x800x24" bash -c '
  wine /lab/SIMTOWER.EXE >/lab/out/oracle/wine_probe.log 2>&1 &
  WPID=$!
  sleep 2
  import -window root /lab/out/oracle/wine_screenshot.png || true
  wait $WPID || true
'
cat /lab/out/oracle/wine_probe.log

echo "--- Oracle Verdict Summary ---"
python3 -c '
with open("/lab/out/oracle/wine_probe.log", "r") as f:
    log = f.read()

print("Log length:", len(log), "bytes")
if "WAVMIX16.DLL" in log or "16-bit" in log or "bad CPU type" in log or "could not load kernel32" in log or "nodrv_CreateWindow" in log:
    print("VERDICT: UNSUPPORTED. The 64-bit Wine 11.0 Staging installation cannot execute 16-bit NE executables without Win16/32-bit WoW64 runtime.")
    print("Error Evidence:")
    for line in log.splitlines():
        if any(k in line for k in ["WAVMIX16", "kernel32", "nodrv", "err:"]):
            print("  ", line)
'
