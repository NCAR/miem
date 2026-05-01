FROM fedora:latest

ARG BUILD_TYPE=Release

RUN dnf -y update \
    && dnf -y install \
        cmake \
        gcc-c++ \
        gdb \
        git \
        make \
        tree \
    && dnf clean all

COPY . miem

RUN cd miem \
    && cmake -S . \
             -B build \
             -D CMAKE_BUILD_TYPE=${BUILD_TYPE} \
    && cd build \
    && make -j 8

WORKDIR miem/build
