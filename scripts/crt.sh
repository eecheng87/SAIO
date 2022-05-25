#!/bin/bash

SUBJ="/C=TW/ST=Test/L=Test/O=Test/OU=Test/CN=localhost/emailAddress=info@localhost"
KEY_NAME="saio-root.key"
CERT_NAME="saio-root.crt"

echo Generating RSA key...

mkdir -p auth/RSA
cd auth/RSA
openssl req -new -days 365 -nodes -x509					\
	-newkey rsa:2048						\
	-subj "${SUBJ}" -keyout ${KEY_NAME} -out ${CERT_NAME}
cd ..

echo Generating ECDSA key...

mkdir -p ECDSA
cd ECDSA
openssl req -new -days 365 -nodes -x509					\
	-newkey ec -pkeyopt ec_paramgen_curve:prime256v1		\
	-subj "${SUBJ}" -keyout ${KEY_NAME} -out ${CERT_NAME}
cd ..

echo Done.