FROM ubuntu as base

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get autoremove -y

RUN apt-get install git -y

FROM base as cppcheck-build

RUN apt-get install g++ cmake -y
RUN git clone https://github.com/danmar/cppcheck.git
RUN export CXX=g++ && \
    mkdir cppcheck/build && \
    cd cppcheck/build && \
    cmake .. -DUSE_MATCHCOMPILER=ON && \
    cmake --build .

FROM base

RUN apt-get install gcc build-essential \
    vim clang-format \
    aspell colordiff python3 -y

COPY --from=cppcheck-build /cppcheck/build/bin/cppcheck /usr/bin/
COPY --from=cppcheck-build /cppcheck/cfg /usr/local/share/Cppcheck/cfg
COPY . /linux2020/lab0-c

CMD ["bash"]
