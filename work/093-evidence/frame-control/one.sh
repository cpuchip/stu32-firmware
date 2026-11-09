#!/bin/bash
# one.sh ROOT TAG: flash ROOT's stand-in image (talk ignored), then measure with FLASH=0 on the port as it is now
S=/home/claude/scratch/soroban/claude-1001/-home-claude-code-stuffleberry-workspace/153a9ea4-8327-4f6d-b9bf-7e56bb37ca2d/scratchpad
cd "$1" || exit 9
P=$(readlink -f /dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00)
GAMES_REPOS=$S/g077 CASIM_REPO=/home/claude/code/stuffleberry/workspace/projects-internal/casim PATH=$S/shim:$PATH timeout 420 device/standin/run.sh $P < /dev/null > $S/p3/ctl/$2-flash.out 2>&1
for c in $(docker ps --format '{{.Names}}' | grep '^soroban-'); do docker stop -t 2 $c >/dev/null; done
sleep 3
P=$(readlink -f /dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00)
FLASH=0 PATH=$S/shim:$PATH timeout 500 device/standin/run.sh $P device/standin/rowbench090.txt > $S/p3/ctl/$2.out 2>&1
echo "$2 rc=$? $(date -u +%H:%M:%SZ) port $P"
grep -a 'core=\|a frame\|a key\|NOT MET' $S/p3/ctl/$2.out | grep -v PASS
