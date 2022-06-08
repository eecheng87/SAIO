#!/usr/bin/env bash

lib_saio_dir=$(find $(pwd) -type d -name "wrapper" | sed 's_/_\\/_g')
libevent_dir=$(find $(pwd) -type d -path "*/local/lib" | sed 's_/_\\/_g')
mempath=$1

# modify memcached
sed -i "s/LDFLAGS =.*/& -Wl,-rpath -Wl,${lib_saio_dir}/" ${mempath}/Makefile
sed -i "s/LDFLAGS =.*/& -Wl,-rpath -Wl,${libevent_dir}/" ${mempath}/Makefile
sed -i "s/LIBS =.*/& -L${lib_saio_dir} -ldummy/" ${mempath}/Makefile