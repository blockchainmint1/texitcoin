FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# Runtime libraries (for running built binary)
# ------------------------------------------------------------
RUN apt update && apt install -y \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-system1.74.0 \
    libboost-program-options1.74.0 \
    libboost-chrono1.74.0 \
    libminiupnpc17 \
    libevent-2.1-7 \
    libevent-pthreads-2.1-7 \
    libzmq5 \
    libsqlite3-0 \
    libssl3 \
    libfmt8 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]