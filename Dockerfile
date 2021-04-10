# Dockerfile for the build environment for The Tick.

# Using the latest Ubuntu image as a base.
FROM ubuntu:latest
LABEL description="Build environment for The Tick"

# Enable the i386 architecture to cross-compile to 32 bit Intel.
RUN dpkg --add-architecture i386; \
    apt update

# Install the GNU toolchain for various architectures.
#
# This one is tricky. The gcc-multilib metapackage does not play well with non x86 architectures.
# Therefore we install it first to pull the dependencies, then install the other toolchains,
# but we preserve the packages we need from gcc-multilib.
#
# See: https://bugs.launchpad.net/ubuntu/+source/gcc-defaults/+bug/1300211
#
RUN DEBIAN_FRONTEND=noninteractive apt install -y gcc-multilib; \
    DEBIAN_FRONTEND=noninteractive apt install -y build-essential libc6-dev-i386-amd64-cross \
    gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu \
    gcc-mips-linux-gnu gcc-mips64-linux-gnuabi64

# Install the Android NDK.
RUN echo "google-android-ndk-installer google-android-installers/mirror select https://dl.google.com" | debconf-set-selections; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata; \
    DEBIAN_FRONTEND=noninteractive apt install -y google-android-ndk-installer; \
    ln -s /usr/lib/android-ndk/toolchains/arm-linux-androideabi-*/prebuilt/linux-x86_64/bin/arm-linux-androideabi-* /usr/bin; \
    ln -s /usr/lib/android-ndk/toolchains/aarch64-linux-android-*/prebuilt/linux-x86_64/bin/aarch64-linux-android-* /usr/bin; \
    ln -s /usr/lib/android-ndk/toolchains/mipsel-linux-android-*/prebuilt/linux-x86_64/bin/mipsel-linux-android-* /usr/bin; \
    ln -s /usr/lib/android-ndk/toolchains/mips64el-linux-android-*/prebuilt/linux-x86_64/bin/mips64el-linux-android-* /usr/bin; \
    ln -s /usr/lib/android-ndk/toolchains/x86-*/prebuilt/linux-x86_64/bin/i686-linux-android-* /usr/bin; \
    ln -s /usr/lib/android-ndk/toolchains/x86_64-*/prebuilt/linux-x86_64/bin/x86_64-linux-android-* /usr/bin

# Install the musl libc toolchains.
RUN apt install -y curl; \
    mkdir -p /opt/musl; cd /opt/musl; \
    for url in $(curl -s musl.cc | grep cross); do curl -s $url | tar -xvz; done; \
    ln -s /opt/musl/*-cross/bin/* /usr/bin

# These are the source and output directories for the container.
RUN mkdir /opt/src /opt/bin; \
    chmod 777 /opt/src /opt/bin

# Note that we don't specify a USER at the end. This does not mean we will be running as root.
# Check out build.sh to see how we invoke the container from the currently running user.
