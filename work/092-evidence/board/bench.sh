#!/bin/bash
# bench.sh ROOT TAG RUNNER(standin|reva): flash ROOT's image (talk ignored), then SAVEBENCH with FLASH=0 on the port as it is now
S=/home/claude/scratch/soroban/claude-1001/-home-claude-code-stuffleberry-workspace/153a9ea4-8327-4f6d-b9bf-7e56bb37ca2d/scratchpad
cd "$1" || exit 9
R=device/standin/run.sh; [ "$3" = reva ] && R=device/reva/headless.sh
P=$(readlink -f /dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00); [ "$3" = reva ] && P=/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00
GAMES_REPOS=$S/g077 CASIM_REPO=/home/claude/code/stuffleberry/workspace/projects-internal/casim PATH=$S/shim:$PATH timeout 1500 $R $P < /dev/null > $S/p5/$2-flash.out 2>&1
echo "$2 flash rc=$? $(grep -ac 'Hash of data verified' $S/p5/$2-flash.out) verified $(date -u +%H:%M:%SZ)"
for c in $(docker ps --format '{{.Names}}' | grep '^soroban-'); do docker stop -t 2 $c >/dev/null; done
sleep 3
P=$(readlink -f /dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00); [ "$3" = reva ] && P=/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00
FLASH=0 PATH=$S/shim:$PATH timeout 900 $R $P device/standin/savebench090.txt > $S/p5/$2.out 2>&1
echo "$2 rc=$? $(date -u +%H:%M:%SZ)"
grep -a 'core=\|savebench:\|NOT MET\|ALL MET' $S/p5/$2.out | grep -v '^talk' | head -40
