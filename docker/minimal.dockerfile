FROM ubuntu:18.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc-7 g++-7 \
    python3 \
    git curl wget \
    && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 100

WORKDIR /workspace
