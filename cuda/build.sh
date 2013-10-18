#!/bin/sh
rm -R -f example1.devcode
nvcc example1.cu -arch=sm_20 --ptx
nvcc example1.cu -arch=sm_20 -ext=all -dir=example1.devcode -o example1
javac glink.java
java glink > compiled.ptx

_dfiles="./example1.devcode/*"
for f in $_dfiles
do
        cp compiled.ptx $f
done
