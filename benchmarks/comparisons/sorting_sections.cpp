// Simple program to compare different implementations
// Example:
// clang++ sorting_sections.cpp -fopenmp -Wall -Wextra -O2 -g -o main && OMP_MAX_ACTIVE_LEVELS=2 KMP_TOPOLOGY_METHOD=hwloc KMP_MOLDABLE_LEVELS=2 ./main
#include <algorithm>
#include <iostream>
#include <random>
#include <cassert>
#include <chrono>
#include <omp.h>

#ifndef SECTIONS
#define SECTIONS 100
#endif
#ifndef SECTION_SIZE
#define SECTION_SIZE 10000
#endif
#ifndef ITERATIONS
#define ITERATIONS 100
#endif

const int SIZE = SECTIONS * SECTION_SIZE;

enum Loop {
    Serial,
    Parallel,
    SplitParallel,
};

void a(Loop loop, int *overlay) {
    // First we create the list 0...SIZE
    int* list = (int*) malloc(SIZE * sizeof(int));
    for (int i = 0; i < SIZE; i++) {
        list[i] = i + overlay[i];
    }

    // Then we shuffle it
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(list, list+SIZE, g);

    // Then we sort each section individually
    switch (loop) {
        case Serial:
            for (int i = 0; i < SECTIONS; i++) {
                int* beg = list + SECTION_SIZE*i;
                int* end = list + SECTION_SIZE*(i+1);
                std::sort(beg, end);
            }
            break;
        case Parallel:
            #pragma omp parallel for
            for (int i = 0; i < SECTIONS; i++) {
                int* beg = list + SECTION_SIZE*i;
                int* end = list + SECTION_SIZE*(i+1);
                std::sort(beg, end);
            }
            break;
        case SplitParallel:
            int p = omp_get_place_num();
            int t = omp_get_place_num_procs(p) / 2;
            #pragma omp parallel for num_threads(t) proc_bind(master)
            for (int i = 0; i < SECTIONS; i++) {
                int* beg = list + SECTION_SIZE*i;
                int* end = list + SECTION_SIZE*(i+1);
                std::sort(beg, end);
            }
            break;
    }

    // Then we assert that each section is ordered
    for (int i = 0; i < SECTIONS; i++) {
        for (int j = 0; j < SECTION_SIZE-1; j++) {
            int a = list[SECTION_SIZE*i + j];
            int b = list[SECTION_SIZE*i + j + 1];
            assert(a <= b);
        }
    }
    free(list);
}

void moldable(bool wait) {
    int *overlay1 = (int*) malloc(SIZE * sizeof(int));
    for (int i = 0; i < SIZE; i++) {
        overlay1[i] = i;
    }
    int *overlay2 = (int*) malloc(SIZE * sizeof(int));
    for (int i = 0; i < SIZE; i++) {
        overlay2[i] = SIZE-i;
    }
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task
        {
            for (int i = 0; i < ITERATIONS; i++) {
                #pragma omp task moldable
                a(Parallel, overlay1);

                #pragma omp task moldable
                a(Parallel, overlay2);
                if (wait) {
                    #pragma omp taskwait
                }
            }
            // #pragma omp taskwait
        }
    }
    free(overlay1);
    free(overlay2);
}

void serial_tasks(bool wait) {
    int *overlay1 = (int*) malloc(SIZE * sizeof(int));
    for (int i = 0; i < SIZE; i++) {
        overlay1[i] = i;
    }
    int *overlay2 = (int*) malloc(SIZE * sizeof(int));
    for (int i = 0; i < SIZE; i++) {
        overlay2[i] = SIZE-i;
    }
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task
        {
            for (int i = 0; i < ITERATIONS; i++) {
                #pragma omp task
                a(Serial, overlay1);

                #pragma omp task
                a(Serial, overlay2);
                if (wait) {
                    #pragma omp taskwait
                }
            }
            // #pragma omp taskwait
        }
    }
    free(overlay1);
    free(overlay2);
}

// void manual_moldable(bool wait) {
//     #pragma omp parallel
//     #pragma omp single
//     for (int i = 0; i < ITERATIONS; i++) {
//         #pragma omp task
//         a(SplitParallel);

//         #pragma omp task
//         a(SplitParallel);
//         if (wait) {
//             #pragma omp taskwait
//         }
//     }
// }

// void serial() {
//     for (int i = 0; i < ITERATIONS; i++) {
//         a(Serial);
//         a(Serial);
//     }
// }

#define TIME(f, s)                             \
    before = std::chrono::system_clock::now(); \
    f;                                         \
    after = std::chrono::system_clock::now();  \
    std::cout << s << (after - before) / 1ms << "ms" << std::endl

int main() {
    using namespace std::literals;

    std::chrono::time_point<std::chrono::system_clock> before;
    std::chrono::time_point<std::chrono::system_clock> after;
    std::cout << "SECTIONS     : " << SECTIONS << std::endl;
    std::cout << "SECTION_SIZE : " << SECTION_SIZE << std::endl;
    std::cout << "ITERATIONS   : " << ITERATIONS << std::endl;
    for (int ii = 0; ii < 20; ii++) {
        TIME(moldable(false),        "Moldable               : ");
        TIME(serial_tasks(false),    "Serial Tasks           : ");
    }
    // TIME(manual_moldable(false), "Manual Moldable        : ");
    // TIME(moldable(true),         "Moldable        (wait) : ");
    // TIME(serial_tasks(true),     "Serial Tasks    (wait) : ");
    // TIME(manual_moldable(true),  "Manual Moldable (wait) : ");
    // TIME(serial(),               "Serial                 : ");
    return 0;
}
