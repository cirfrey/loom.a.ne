FROM alpine:3.21

RUN apk add --no-cache \
    gcc g++ musl-dev \
    cmake ninja \
    python3 py3-pip \
    boost-dev \
    git curl wget

RUN pip3 install --break-system-packages meson packaging rich

WORKDIR /workspace
