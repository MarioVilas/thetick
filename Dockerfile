# Dockerfile for the build environment for The Tick.

# Using the latest Ubuntu image as a base.
FROM ubuntu:latest
LABEL description="Build environment for The Tick"

# These are the source and output directories for the container.
RUN mkdir /opt/src /opt/bin; \
    chmod 777 /opt/src /opt/bin

# Enable the i386 architecture to cross-compile to 32 bit Intel.
RUN dpkg --add-architecture i386; \
    apt update

#----------------------------------------------------------------------------#
# Install the GNU toolchain for various architectures and versions.
#
# We cannot install gcc-multilib because it does not work well when multiple
# toolchains are installed. Therefore we install each gcc version manually.
#
# See: https://bugs.launchpad.net/ubuntu/+source/gcc-defaults/+bug/1300211
#
RUN DEBIAN_FRONTEND=noninteractive apt install -y gcc-multilib; \
    DEBIAN_FRONTEND=noninteractive apt install -y build-essential libc6-dev-i386-amd64-cross \
        gcc-aarch64-linux-gnu \
        gcc-arm-linux-gnueabi \
        gcc-i686-linux-gnu \
        gcc-mingw-w64-i686 \
        gcc-mingw-w64-x86-64 \
        gcc-mips-linux-gnu \
        gcc-mips64-linux-gnuabi64

# If you want to go for the overkill, try this instead ;)
#
#RUN DEBIAN_FRONTEND=noninteractive apt install -y gcc-multilib; \
#    DEBIAN_FRONTEND=noninteractive apt install -y build-essential libc6-dev-i386-amd64-cross \
#        gcc-8-aarch64-linux-gnu \
#        gcc-8-alpha-linux-gnu \
#        gcc-8-arm-linux-gnueabi \
#        gcc-8-arm-linux-gnueabihf \
#        gcc-8-hppa-linux-gnu \
#        gcc-8-hppa64-linux-gnu \
#        gcc-8-i686-linux-gnu \
#        gcc-8-m68k-linux-gnu \
#        gcc-8-powerpc-linux-gnu \
#        gcc-8-powerpc64-linux-gnu \
#        gcc-8-powerpc64le-linux-gnu \
#        gcc-8-riscv64-linux-gnu \
#        gcc-8-s390x-linux-gnu \
#        gcc-8-sh4-linux-gnu \
#        gcc-8-sparc64-linux-gnu \
#        gcc-8-x86-64-linux-gnux32 \
#        gcc-9-aarch64-linux-gnu \
#        gcc-9-alpha-linux-gnu \
#        gcc-9-arm-linux-gnueabi \
#        gcc-9-arm-linux-gnueabihf \
#        gcc-9-hppa-linux-gnu \
#        gcc-9-hppa64-linux-gnu \
#        gcc-9-i686-linux-gnu \
#        gcc-9-m68k-linux-gnu \
#        gcc-9-mips-linux-gnu \
#        gcc-9-mips64-linux-gnuabi64 \
#        gcc-9-mips64el-linux-gnuabi64 \
#        gcc-9-mipsel-linux-gnu \
#        gcc-9-mipsisa32r6-linux-gnu \
#        gcc-9-mipsisa32r6el-linux-gnu \
#        gcc-9-mipsisa64r6-linux-gnuabi64 \
#        gcc-9-mipsisa64r6el-linux-gnuabi64 \
#        gcc-9-powerpc-linux-gnu \
#        gcc-9-powerpc64-linux-gnu \
#        gcc-9-powerpc64le-linux-gnu \
#        gcc-9-riscv64-linux-gnu \
#        gcc-9-s390x-linux-gnu \
#        gcc-9-sh4-linux-gnu \
#        gcc-9-sparc64-linux-gnu \
#        gcc-9-x86-64-linux-gnu \
#        gcc-9-x86-64-linux-gnux32 \
#        gcc-10-aarch64-linux-gnu \
#        gcc-10-alpha-linux-gnu \
#        gcc-10-arm-linux-gnueabi \
#        gcc-10-arm-linux-gnueabihf \
#        gcc-10-hppa-linux-gnu \
#        gcc-10-hppa64-linux-gnu \
#        gcc-10-i686-linux-gnu \
#        gcc-10-m68k-linux-gnu \
#        gcc-10-mips-linux-gnu \
#        gcc-10-mips64-linux-gnuabi64 \
#        gcc-10-mips64el-linux-gnuabi64 \
#        gcc-10-mipsel-linux-gnu \
#        gcc-10-mipsisa32r6-linux-gnu \
#        gcc-10-mipsisa32r6el-linux-gnu \
#        gcc-10-mipsisa64r6-linux-gnuabi64 \
#        gcc-10-mipsisa64r6el-linux-gnuabi64 \
#        gcc-10-powerpc-linux-gnu \
#        gcc-10-powerpc64-linux-gnu \
#        gcc-10-powerpc64le-linux-gnu \
#        gcc-10-riscv64-linux-gnu \
#        gcc-10-s390x-linux-gnu \
#        gcc-10-sh4-linux-gnu \
#        gcc-10-sparc64-linux-gnu \
#        gcc-10-x86-64-linux-gnu \
#        gcc-10-x86-64-linux-gnux32 \
#        gcc-aarch64-linux-gnu \
#        gcc-alpha-linux-gnu \
#        gcc-arm-linux-gnueabi \
#        gcc-arm-linux-gnueabihf \
#        gcc-hppa-linux-gnu \
#        gcc-hppa64-linux-gnu \
#        gcc-i686-linux-gnu \
#        gcc-m68k-linux-gnu \
#        gcc-mingw-w64-i686 \
#        gcc-mingw-w64-x86-64 \
#        gcc-mips-linux-gnu \
#        gcc-mips64-linux-gnuabi64 \
#        gcc-mips64el-linux-gnuabi64 \
#        gcc-mipsel-linux-gnu \
#        gcc-mipsisa32r6-linux-gnu \
#        gcc-mipsisa32r6el-linux-gnu \
#        gcc-mipsisa64r6-linux-gnuabi64 \
#        gcc-mipsisa64r6el-linux-gnuabi64 \
#        gcc-powerpc-linux-gnu \
#        gcc-powerpc64-linux-gnu \
#        gcc-powerpc64le-linux-gnu \
#        gcc-riscv64-linux-gnu \
#        gcc-s390x-linux-gnu \
#        gcc-sh4-linux-gnu \
#        gcc-sparc64-linux-gnu

#----------------------------------------------------------------------------#
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

#----------------------------------------------------------------------------#
# Install the musl libc toolchains.
#
# These allow for more portable binaries. For more details see:
# https://stackoverflow.com/questions/57476533/why-is-statically-linking-glibc-discouraged
#
# NOTE: this will overwrite the symlinks for the "official" mingw toolchain!
#       the rest of the symlinks have different names so it's fine
#
RUN apt install -y curl; \
    mkdir -p /opt/musl; cd /opt/musl; \
    for url in \
        https://musl.cc/aarch64-linux-musl-cross.tgz \
        https://musl.cc/arm-linux-musleabi-cross.tgz \
        https://musl.cc/i686-linux-musl-cross.tgz \
        https://musl.cc/i686-w64-mingw32-cross.tgz \
        https://musl.cc/mips-linux-musl-cross.tgz \
        https://musl.cc/mips64-linux-musl-cross.tgz \
        https://musl.cc/x86_64-linux-musl-cross.tgz \
        https://musl.cc/x86_64-w64-mingw32-cross.tgz ; \
    do curl -s $url | tar -xvz; done; \
    ln -sf /opt/musl/*-cross/bin/* /usr/bin

# If you want to go for the overkill, try this instead ;)
#
#RUN apt install -y curl; \
#    mkdir -p /opt/musl; cd /opt/musl; \
#    for url in $(curl -s musl.cc | grep cross); do curl -s $url | tar -xvz; done; \
#    ln -sf /opt/musl/*-cross/bin/* /usr/bin
