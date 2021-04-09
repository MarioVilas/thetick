FROM ubuntu:latest
LABEL description="Build environment for The Tick"
RUN dpkg --add-architecture i386; \
    apt update
RUN apt install -y gcc-multilib; \
    apt install -y build-essential gcc-arm-linux-gnueabi libc6-dev-i386-amd64-cross
RUN mkdir /opt/src /opt/bin
