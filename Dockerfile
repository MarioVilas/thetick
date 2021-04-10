FROM ubuntu:latest
LABEL description="Build environment for The Tick"
RUN dpkg --add-architecture i386; \
    apt update
RUN apt install -y gcc-multilib; \
    apt install -y build-essential libc6-dev-i386-amd64-cross \
    gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu \
    gcc-mips-linux-gnu gcc-mips64-linux-gnuabi64
RUN mkdir /opt/src /opt/bin
