#!/bin/bash
# Compile time options
SECTIONS=100
SECTION_SIZE=1000
ITERATIONS=100

export KMP_SETTINGS=true

export OMP_MAX_ACTIVE_LEVELS=2


# Moldable Teams
export KMP_MOLDABLE_LEVELS=2
# export KMP_MOLDABLE_TIME_METHOD=0
# export KMP_MOLDABLE_EXP_AVERAGE=10
# export KMP_MOLDABLE_OVERSUBSCRIPTION_METHOD=0
export KMP_MOLDABLE_WORK_STEALING=1
# export KMP_MOLDABLE_PUSH_TO_OWN_QUEUE=0

# Extrae
source /usr/etc/extrae.sh
export EXTRAE_CONFIG_FILE=extrae.xml

#------------------------------

# Guarantee that we use the correct OpenMP lib
export LD_LIBRARY_PATH=../../openmp/build/runtime/src

CXX_FLAGS="-fopenmp -Wall -Wextra -O2 -g -finstrument-functions -lomptrace -rdynamic"

CXX_ENVS="-DSECTIONS=${SECTIONS} -DSECTION_SIZE=${SECTION_SIZE} -DITERATIONS=${ITERATIONS}"
clang++ sorting_sections.cpp $CXX_FLAGS $CXX_ENVS -o main

# export LD_PRELOAD=${EXTRAE_HOME}/lib/libomptrace.so
export KMP_TOPOLOGY_METHOD=hwloc

./main

