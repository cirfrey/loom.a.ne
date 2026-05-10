FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc-9 g++-9 \
    python3 python3-pip \
    git curl wget \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install packaging

WORKDIR /workspace
