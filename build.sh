#!/bin/sh

# This script will build for all supported platforms at once.

set -x
set -e
cd "$(dirname "$0")"
mkdir -p bin
docker build -t thetick-builder .
for t in generic-arm-32 generic-arm-64 generic-intel-32 generic-intel-64 generic-mips-32 generic-mips-64 lexmark-cx310dn
do
    docker run -it -v $(pwd)/src:/opt/src -v $(pwd)/bin:/opt/bin thetick-builder /bin/sh -c "cd /opt/src; TARGET=$t make clean all; chown 1000:1000 /opt/bin/ticksvc-*"
done
docker rm $(docker ps -aq --filter="ancestor=thetick-builder")
