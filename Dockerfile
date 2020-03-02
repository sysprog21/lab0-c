FROM ubuntu:18.04 as base

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get autoremove -y

RUN apt-get install git -y

FROM base as cppcheck-build

ARG CPPCHECK_GIT_REPO="https://github.com/danmar/cppcheck.git"
RUN apt-get install g++ cmake python3 -y
RUN git clone ${CPPCHECK_GIT_REPO}
RUN export CXX=g++ && \
    mkdir cppcheck/build && \
    cd cppcheck/build && \
    cmake .. -DUSE_MATCHCOMPILER=ON -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build .

FROM base

RUN apt-get update --fix-missing && \
    apt-get install gcc build-essential \
    vim clang-format valgrind \
    aspell colordiff python3 -y \
    --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

COPY --from=cppcheck-build /cppcheck/build/bin/cppcheck /usr/bin/
COPY --from=cppcheck-build /cppcheck/cfg /usr/share/Cppcheck/cfg

ARG DIR="/lab0-c"
COPY . ${DIR}
WORKDIR ${DIR}

CMD ["bash"]
