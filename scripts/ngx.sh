#!/usr/bin/env bash

libpath=$(find $(pwd) -type f -name "libdummy.so" | sed 's_/_\\/_g')
webpath=$(readlink --canonicalize web | sed 's_/_\\/_g')
authpath=$(readlink --canonicalize auth | sed 's_/_\\/_g')
ngxpath=$1

# modify nginx
sed -i 's/-Werror//' ${ngxpath}/objs/Makefile
sed -i "s/-Wl,-E/-Wl,-E ${libpath}/" ${ngxpath}/objs/Makefile
sed -i "s/root.*;/root ${webpath};/" configs/nginx.conf

if [ $2 = "tls" ]; then
    # enable TLS
    sed -i "s/listen       8081;/# listen       8081;/" configs/nginx.conf
    sed -i "23i ssl_session_timeout 5m;" configs/nginx.conf
    sed -i "23i ssl_certificate_key ${authpath}/RSA/saio-root.key;" configs/nginx.conf
    sed -i "23i ssl_certificate ${authpath}/RSA/saio-root.crt;" configs/nginx.conf
    sed -i "23i listen [::]:443 ssl;" configs/nginx.conf
    sed -i "23i listen 443 ssl;" configs/nginx.conf
fi