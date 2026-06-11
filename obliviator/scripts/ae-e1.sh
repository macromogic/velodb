#!/bin/bash -x

cd ~/obliviator/join/;
make clean; make;
./host/parallel ./enclave/parallel_enc.signed 1 ./test.txt;

cd ~/obliviator/join_kks/;
make clean; make L3=1;
./app ./output.txt ./test.txt;