#!/bin/bash
#
# The Tick, a simple backdoor for servers and embedded systems.
# 
# Developed by Mario Vilas, mvilas@gmail.com
# http://www.github.com/MarioVilas/thetick
# 
# Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
# http://www.github.com/nccgroup/thetick
# 
# See the LICENSE file for further details.
#

# This script will build for all supported platforms at once.
# It launches a nonprivileged Docker container running as the current user.
# Naturally, Docker must already be installed and configured for this to work.
#
# Usage:
#
# To build all platforms:
#     ./build.sh
#
# To build only some platforms:
#     ./build.sh windows linux



# Stop on all errors.
set -e

# Uncomment this for debugging.
#set -x

# Here is the list of targets. MUST match the one in the Makefile.
TARGETS="
generic-arm-android
generic-arm64-android
generic-mips-android
generic-mips64-android
generic-x86-android
generic-x86_64-android
generic-arm-linux
generic-arm64-linux
generic-mips-linux
generic-mips64-linux
generic-x86-linux
generic-x86_64-linux
generic-x86-windows
generic-x86_64-windows
"

# Helper variables for color output.
# Disable color automatically if not running on a TTY.
if [ -t 1 ]
then
    RED='\033[1;31m'
    GREEN='\033[1;32m'
    BLUE='\033[1;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    BLUE=''
    NC=''
fi

# Filter the list of targets if substrings are given.
if [ $# -ne 0 ]
then
    LIST=""
    for t in $TARGETS
    do
        for p in "$@"
        do
            if [[ $t == *"$p"* ]]
            then
                LIST+=" $t"
                break
            fi
        done
    done
    TARGETS="$LIST"
fi

# Trim leading and trailing whitespace in the list of targets.
TARGETS="${TARGETS#"${TARGETS%%[![:space:]]*}"}"
TARGETS="${TARGETS%"${TARGETS##*[![:space:]]}"}"

# Stop early if we don't have a valid list of targets.
if [ -z "$TARGETS" ]
then
    echo -e "${RED}ERROR: no targets to build${NC}"
    exit 1
fi

# Switch to the directory where the script lives.
# This is just in case the script was accidentally called from somewhere else.
cd "$(dirname "$0")"

# Ensure the output directory exists.
mkdir -p bin

# Build the Docker image with all the toolchains.
# This will take quite a while on the first run...
echo -e "${RED}-------------------------------------------------------------------------------${NC}"
echo -e "${RED}Preparing the build image. If this is the first run, it will take a while...${NC}"
echo -e "${RED}-------------------------------------------------------------------------------${NC}"
docker build -t thetick-builder .

# Build each target in the container.
# The src/ and bin/ directories are mapped into the container.
# The container is run as the current user (not root).
# Each time we clean the build to remove the .o files from the previous run.
echo -e "${GREEN}-------------------------------------------------------------------------------${NC}"
echo -e "${GREEN}Building for targets:${NC}"
for t in $TARGETS
do
    echo -e "${GREEN} - $t${NC}"
done
for t in $TARGETS
do
    echo -e "${GREEN}-------------------------------------------------------------------------------${NC}"
    # Remove -s to see all the files being compiled (noisy!).
    # Remove -j to compile sequentially (slow!)
    docker run -it -u $(id -u) -v $(pwd)/src:/opt/src -v $(pwd)/bin:/opt/bin thetick-builder /bin/sh -c "cd /opt/src; TARGET=$t make -s -j clean all"
done

# Remove any dangling containers we might have left.
docker rm $(docker ps -aq --filter="ancestor=thetick-builder") >/dev/null

# Proudly show the user what we've accomplished today. :D
echo -e "${BLUE}-------------------------------------------------------------------------------${NC}"
ls -lh bin/ticksvc-*
echo -e "${BLUE}-------------------------------------------------------------------------------${NC}"
file bin/ticksvc-*
echo -e "${BLUE}-------------------------------------------------------------------------------${NC}"
