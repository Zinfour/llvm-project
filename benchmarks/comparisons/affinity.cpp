#include <chrono>
#include <omp.h>
#include <thread>
#include <iostream>

void print_affinity() {
  kmp_affinity_mask_t mask;
  kmp_create_affinity_mask(&mask);
  kmp_get_affinity(&mask);
  #pragma omp critical
  {
    for (int i = 0; i < 8; i++)
    {
      if (kmp_get_affinity_mask_proc(i, &mask)) {
        std::cout << "1";
      } else {
        std::cout << "0";
      }
    }
    std::cout << std::endl;
  }
}

void a() {
  #pragma omp parallel for
  for (int i = 0; i < 10; i++) {
    print_affinity();
  }
}

int main()
{
#pragma omp parallel
#pragma omp single
  {
    for (int i = 0; i < 20; i++) {
      #pragma omp task moldable
      a();
    }
  }
}