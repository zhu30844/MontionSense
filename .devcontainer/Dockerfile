FROM ubuntu:22.04

LABEL maintainer="zhu30844"
LABEL description="Dev container for buildroot-based project on Luckfox board, place this file in the root of the SDK repo"


ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8


RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        tzdata \
        gcc gcc-multilib g++ g++-multilib \
        git ssh make \
        module-assistant expect \
        gawk texinfo libssl-dev \
        bison flex fakeroot cmake \
        unzip gperf autoconf \
        device-tree-compiler libncurses5-dev \
        pkg-config bc \
        python-is-python3 \
        passwd openssl \
        openssh-server openssh-client \
        vim file cpio rsync wget patch\
        usbutils adb sudo ca-certificates curl&& \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*


WORKDIR /workspace

RUN touch /root/.bash_profile && touch /root/.bashrc


