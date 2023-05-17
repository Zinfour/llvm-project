#include <chrono>
#include <omp.h>
#include <thread>

#define ITERATIONS 30
#define TASKS_PER_ITERATIONS 100
#define WORK_PER_TASK 100
#define SLEEP_TIME 1


void task_a() {
  for (int ii = 0; ii < WORK_PER_TASK; ii++) {
    std::this_thread::sleep_for(std::chrono::microseconds(SLEEP_TIME));
  }
}

void task_a_par() {
  #pragma omp parallel for
  for (int ii = 0; ii < WORK_PER_TASK; ii++) {
    std::this_thread::sleep_for(std::chrono::microseconds(SLEEP_TIME));
  }
}

int main()
{
#pragma omp parallel
#pragma omp single
#pragma omp task
  {
    for (int kk=0; kk<ITERATIONS; kk++) {
      for (int kk=0; kk<TASKS_PER_ITERATIONS; kk++) {
        #pragma omp task moldable
        {
          task_a_par();
        }
      }
      #pragma omp taskwait
    }
  }
#pragma omp parallel
#pragma omp single
#pragma omp task
  {
    for (int kk=0; kk<ITERATIONS; kk++) {
      for (int kk=0; kk<TASKS_PER_ITERATIONS; kk++) {
        #pragma omp task
        {
          task_a();
        }
      }
      #pragma omp taskwait
    }
  }
}