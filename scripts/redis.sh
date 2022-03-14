#!/usr/bin/env bash

libpath=$(find $(pwd) -type f -name "libdummy.so" | sed 's_/_\\/_g')
webpath=$(readlink --canonicalize web | sed 's_/_\\/_g')
redispath=$1

# modify redis
sed -i 's/FINAL_LIBS=-lm/FINAL_LIBS=-lm ${libpath}/' ${redispath}/src/Makefile