FROM --platform=linux/amd64 dev-containers:txc-runtime-amd-ubuntu24

ARG ENABLE_QT=0
ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# Base utilities
# ------------------------------------------------------------
RUN apt update && apt install -y \
    ca-certificates \
    curl \
    python3 \
    git \
    bsdmainutils \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Build toolchain
# ------------------------------------------------------------
RUN apt update && apt install -y \
    build-essential \
    g++-multilib \
    make \
    cmake \
    binutils-gold \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Autotools (required for ./autogen.sh)
# ------------------------------------------------------------
RUN apt update && apt install -y \
    autoconf \
    automake \
    autotools-dev \
    libtool \
    pkg-config \
    m4 \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Development headers (for compiling)
# ------------------------------------------------------------
RUN apt update && apt install -y \
    libssl-dev \
    libevent-dev \
    libminiupnpc-dev \
    libzmq3-dev \
    libsqlite3-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libboost-program-options-dev \
    libboost-test-dev \
    libboost-chrono-dev \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Build TEXITcoin depends from sources
# ------------------------------------------------------------
COPY ./depends /deps

WORKDIR /deps

# build dependencies (conditionally)
RUN if [ "${ENABLE_QT}" = "1" ]; then \
      echo "Building depends WITH Qt"; \
      make -j$(nproc); \
    else \
      echo "Building depends WITHOUT Qt"; \
      make NO_QT=1 -j$(nproc); \
    fi

# ------------------------------------------------------------
# New workspace folder and TXC Depends environment
# ------------------------------------------------------------

ENV CONFIG_SITE=/deps/x86_64-pc-linux-gnu/share/config.site

WORKDIR /workspace

CMD ["/bin/bash"]