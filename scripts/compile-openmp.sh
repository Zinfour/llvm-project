#!/bin/bash

# should be run from llvm-project/

cd openmp/build

cmake -DCMAKE_BUILD_TYPE=Debug -DLIBOMP_MOLDABILITY=ON -DOPENMP_LLVM_TOOLS_DIR=$(pwd)/build/bin ..

make -j $(nproc)

cd ../..


