#!/bin/sh

# This script will build for all supported platforms at once.
# It launches a nonprivileged Docker container running as the current user.
# Naturally, Docker must already be installed and configured for this to work.

set -x
set -e
cd "$(dirname "$0")"
mkdir -p bin
docker build -t thetick-builder .
for t in generic-arm-android generic-arm64-android generic-mips-android generic-mips64-android generic-x86-android generic-x86_64-android generic-arm-linux generic-arm64-linux generic-mips-linux generic-mips64-linux generic-x86-linux generic-x86_64-linux
do
    docker run -it -u $(id -u) -v $(pwd)/src:/opt/src -v $(pwd)/bin:/opt/bin thetick-builder /bin/sh -c "cd /opt/src; TARGET=$t make clean all"
done
docker rm $(docker ps -aq --filter="ancestor=thetick-builder")
set +x
echo "-------------------------------------------------------------------------------"
ls -lh bin/ticksvc-*
echo "-------------------------------------------------------------------------------"
file bin/ticksvc-*
echo "-------------------------------------------------------------------------------"
