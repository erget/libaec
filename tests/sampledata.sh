#!/bin/sh
set -e
AEC=../src/aec
CCSDS_DATA=121B2TestData
ALLO=${CCSDS_DATA}/AllOptions
EXTP=${CCSDS_DATA}/ExtendedParameters
LOWE=${CCSDS_DATA}/LowEntropyOptions
archive=121B2TestData.zip

if [ ! -f $archive ]; then
    wget http://cwe.ccsds.org/sls/docs/SLS-DC/BB121B2TestData/$archive
fi
unzip -oq $archive

echo All Options
ln -f ${ALLO}/test_P512n22.dat ${ALLO}/test_p512n22.dat
for i in 01 02 03 04
do
    $AEC -c -d -n$i -j16 -r16 $ALLO/test_p256n${i}-basic.rz > test.dat
    ref=$ALLO/test_p256n${i}.dat
    refsize=$(stat -c "%s" $ref)
    cmp -n $refsize $ref test.dat
    $AEC -c -n$i -j16 -r16  $ref > test.rz
    cmp $ALLO/test_p256n${i}-basic.rz test.rz

    $AEC -c -d -n$i -j16 -r16 -t $ALLO/test_p256n${i}-restricted.rz > test.dat
    cmp -n $refsize $ref test.dat
    $AEC -c -n$i -j16 -r16  -t $ref > test.rz
    cmp $ALLO/test_p256n${i}-restricted.rz test.rz
done
for i in 05 06 07 08 09 10 11 12 13 14 15 16
do
    $AEC -c -d -n$i -j16 -r16 $ALLO/test_p256n${i}.rz > test.dat
    cmp $ALLO/test_p256n${i}.dat test.dat
done
for i in 17 18 19 20 21 22 23 24
do
    $AEC -c -d -n$i -j16 -r32 $ALLO/test_p512n${i}.rz > test.dat
    cmp $ALLO/test_p512n${i}.dat test.dat
done

echo Low Entropy Options
for i in 1 2 3
do
    for j in 01 02 03 04
    do
        $AEC -c -d -n$j -j16 -r64 $LOWE/Lowset${i}_8bit.n${j}-basic.rz \
            > test.dat
        ref=$LOWE/Lowset${i}_8bit.dat
        refsize=$(stat -c "%s" $ref)
        cmp -n $refsize $ref test.dat
        $AEC -c -d -n$j -j16 -r64 -t $LOWE/Lowset${i}_8bit.n${j}-restricted.rz \
            > test.dat
        cmp -n $refsize $ref test.dat
    done
    for j in 05 06 07 08
    do
        $AEC -c -d -n$j -j16 -r64 $LOWE/Lowset${i}_8bit.n${j}.rz \
            > test.dat
        ref=$LOWE/Lowset${i}_8bit.dat
        refsize=$(stat -c "%s" $ref)
        cmp -n $refsize $ref test.dat
    done
done

echo Extended Parameters

$AEC -c -d -n32 -j16 -r256 -p $EXTP/sar32bit.j16.r256.rz > test.dat
ref=$EXTP/sar32bit.dat
refsize=$(stat -c "%s" $ref)
cmp -n $refsize $ref test.dat
$AEC -c -d -n32 -j64 -r4096 -p $EXTP/sar32bit.j64.r4096.rz > test.dat
cmp -n $refsize $ref test.dat
