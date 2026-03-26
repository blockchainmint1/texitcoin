FROM --platform=linux/amd64 ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# Runtime libraries (for running built binary)
# ------------------------------------------------------------

RUN apt update && apt install -y \
    libboost-filesystem1.83.0 \
    libboost-thread1.83.0 \
    libboost-system1.83.0 \
    libevent-2.1-7t64 \
    libevent-pthreads-2.1-7t64 \
    libzmq5 \
    libfmt9 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]