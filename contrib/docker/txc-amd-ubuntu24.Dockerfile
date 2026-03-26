# ---------- Stage 1: Build ----------
FROM --platform=linux/amd64 dev-containers:txc-deps-amd-ubuntu24 AS builder

WORKDIR /src

COPY . .

RUN ./autogen.sh

RUN ./configure   BDB_LIBS="-L$(pwd)/contrib/db4/db4/lib -ldb_cxx-4.8" \
                        BDB_CFLAGS="-I$(pwd)/contrib/db4/db4/include" \
                        CXXFLAGS="-O2" \
                        LDFLAGS="-s" \
                        --with-gui=no \
                        --disable-tests \
                        --disable-bench \
                        --disable-man \
                        --enable-reduce-exports \
                        --disable-shared \
                        --with-pic \
                        --enable-benchmark=no \
                        --with-bignum=no \
                        --enable-module-generator \
                        --enable-module-recovery \
                        --enable-module-commitment \
                        --enable-module-bulletproof \
                        --enable-module-aggsig \
                        --enable-module-schnorrsig \
                        --enable-module-extrakeys \
                        --enable-experimental \
                        --disable-jni \
                        --enable-feature="block-all-mining"

RUN make -j$(nproc)

RUN make install DESTDIR=/build
# /build/usr/bin/texitcoind
# /build/usr/bin/texitcoin-cli

# ---------- Stage 2: Runtime ----------
FROM --platform=linux/amd64 dev-containers:txc-runtime-amd-ubuntu24

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /app
COPY --from=builder /build /

# Add non-root user
# RUN groupadd -r texitcoin && useradd -r -g texitcoin -s /usr/sbin/nologin texitcoin

# Prepare volumes and permissions
RUN mkdir -p /texitcoin-data /texitcoin-config
#    chown -R texitcoin:texitcoin /texitcoin-data /texitcoin-config


VOLUME ["/texitcoin-data", "/texitcoin-config"]

EXPOSE 15739 15740 28337

# USER texitcoin

# Explicitly specify config file path
ENTRYPOINT ["/usr/local/bin/texitcoind"]
CMD ["-datadir=/texitcoin-data", "-conf=/texitcoin-config/texitcoin.conf"]