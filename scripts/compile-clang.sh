#!/bin/bash

# should be run from /llvm-project

cmake -S llvm -B build -G Ninja -DLLVM_ENABLE_PROJECTS='clang' -DLLVM_PARALLEL_LINK_JOBS=8 -DCMAKE_BUILD_TYPE=Release

cd ./build

ninja

cd ..

