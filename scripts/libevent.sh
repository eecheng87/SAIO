#!/usr/bin/env bash

lib_saio_dir=$(find $(pwd) -type d -name "wrapper" | sed 's_/_\\/_g')
libpath=$1

# modify libevent
sed -i "s/LDFLAGS =.*/& -Wl,-rpath -Wl,${lib_saio_dir}/" ${libpath}/Makefile
sed -i "s/LIBS =.*/& -L${lib_saio_dir} -ldummy/" ${libpath}/Makefile