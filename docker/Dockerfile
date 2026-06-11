FROM fedora:latest

ARG BUILD_TYPE=Release

RUN dnf -y update \
    && dnf -y install \
        cmake \
        gcc-c++ \
        gdb \
        git \
        make \
        netcdf-devel \
        tree \
        udunits2-devel \
    && dnf clean all

COPY . miem

RUN cd miem \
    && cmake -S . \
             -B build \
             -D CMAKE_BUILD_TYPE=${BUILD_TYPE} \
             -D MIEM_ENABLE_TESTS=ON \
    && cd build \
    && make -j 8

WORKDIR miem/build
