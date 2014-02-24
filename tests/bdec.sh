#!/bin/sh
set -e
AEC=../src/aec

if [ ! -f bench.rz ]; then
    echo "No encoded file found. Encoding now..."
    ./benc.sh
fi
rm -f dec.dat
bsize=$(stat -c "%s" bench.dat)
utime=$(
    (
        /usr/bin/time -f "%U" $AEC -d -n16 -j64 -r256 -m -c \
            bench.rz > dec.dat
    ) 2>&1
)
perf=$(echo "$bsize/1048576/$utime" | bc)
echo "[0;32m*** Decoding with $perf MiB/s user time ***[0m"
cmp bench.dat dec.dat
