FROM ubuntu:18.04 as base

RUN apt-get update && \
    apt-get install build-essential python3 git-core ca-certificates -y \
    --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

FROM base as cppcheck-build

ARG CPPCHECK_GIT_REPO="https://github.com/danmar/cppcheck.git"
RUN apt-get update && \
    apt-get install cmake -y --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*
RUN git clone ${CPPCHECK_GIT_REPO}
WORKDIR /cppcheck/build
RUN export CXX=g++ && \
    cmake .. -DUSE_MATCHCOMPILER=ON -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build .

FROM base

RUN apt-get update && \
    apt-get install vim clang-format valgrind \
    aspell colordiff gdb -y \
    --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

COPY --from=cppcheck-build /cppcheck/build/bin/cppcheck /usr/bin/
COPY --from=cppcheck-build /cppcheck/cfg /usr/share/Cppcheck/cfg

ARG DIR="/lab0-c"
COPY . ${DIR}
WORKDIR ${DIR}

CMD ["bash"]
