FROM debian:bookworm-slim AS src
LABEL Description="Tilemaker" Version="1.4.0"

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    libsqlite3-dev \
    cmake \
    zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY CMakeLists.txt ./
COPY cmake ./cmake
COPY src ./src
COPY include ./include

RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel $(nproc) && \
    strip tilemaker

ENV PATH="/usr/src/app/build:$PATH"

FROM debian:bookworm-slim
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app
COPY --from=src /usr/src/app/build/tile-smush .
COPY resources ./resources

ENV PATH="/usr/src/app/build:$PATH"

# Entrypoint for docker, wrapped with /bin/sh to remove requirement for executable permissions on script
ENTRYPOINT ["/bin/sh", "/usr/src/app/resources/docker-entrypoint.sh"]
CMD ["--help"]
