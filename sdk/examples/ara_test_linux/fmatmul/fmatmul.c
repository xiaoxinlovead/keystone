// Copyright 2020 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matheus Cavalcante, ETH Zurich
//         Samuel Riedel, ETH Zurich
//         Matteo Perotti, ETH Zurich

#include <string.h>

#include "kernel/fmatmul.h"
#include "runtime.h"
#include "util.h"

#ifdef SPIKE
#include <stdio.h>
#elif defined ARA_LINUX
#include <stdio.h>
#else
#include "printf.h"
#endif

// Define Matrix dimensions:
// C = AB with A=[MxN], B=[NxP], C=[MxP]
extern uint64_t fmatmul_M;
extern uint64_t fmatmul_N;
extern uint64_t fmatmul_P;

extern double fmatmul_a[] __attribute__((aligned(32 * NR_LANES)));
extern double fmatmul_b[] __attribute__((aligned(32 * NR_LANES)));
extern double fmatmul_c[] __attribute__((aligned(32 * NR_LANES)));
// Gold results
extern double fmatmul_g[] __attribute__((aligned(32 * NR_LANES)));

#define THRESHOLD 0.001

// Verify the matrix
static int verify_matrix(double *result, double *gold, size_t R, size_t C,
                  double threshold) {
  for (uint64_t i = 0; i < R; ++i) {
    for (uint64_t j = 0; j < C; ++j) {
      uint64_t idx = i * C + j;
      if (!similarity_check(result[idx], gold[idx], threshold)) {
        return (i + j) == 0 ? -1 : idx;
      }
    }
  }
  return 0;
}

int run_fmatmul() {
  printf("\n");
  printf("===================\n");
  printf("=  FMATMUL (VEC)  =\n");
  printf("===================\n");
  printf("\n");
  printf("\n");

#ifdef VCD_DUMP
  // Measure only the full-size matmul
  for (uint64_t s = M; s <= M; s *= 2) {
#else
  for (uint64_t s = 4; s <= fmatmul_M; s *= 2) {
#endif
    printf("\n");
    printf("------------------------------------------------------------\n");
    printf("Calculating a (%d x %d) x (%d x %d) matrix multiplication...\n", s,
           s, s, s);
    printf("------------------------------------------------------------\n");
    printf("\n");

    // Matrices are initialized --> Start calculating
    printf("Calculating fmatmul (vector)...\n");
    start_timer();
    start_timer_time();
    fmatmul(fmatmul_c, fmatmul_a, fmatmul_b, s, s, s);
    stop_timer();
    stop_timer_time();

    // Metrics
    int64_t runtime = get_timer();
    int64_t runtime_time = get_timer_time();
    float performance = 2.0 * s * s * s / runtime;
    float utilization = 100 * performance / (2.0 * NR_LANES);
    float performance_time = 2.0 * s * s * s / runtime_time;
    float utilization_time = 100 * performance_time / (2.0 * NR_LANES);

    printf("The execution took %d cycles.\n", runtime);
    printf("The performance is %f FLOP/cycle (%f%% utilization).\n",
           performance, utilization);
    printf("The execution took %d timebase-ticks.\n", runtime_time);
    printf("The performance is %f FLOP/tick (%f%% utilization).\n",
           performance_time, utilization_time);

    // Verify the result only for s == M (to keep it simple)
    if (s == fmatmul_M) {
      printf("Verifying result...\n");
      int error = verify_matrix(fmatmul_c, fmatmul_g, s, s, THRESHOLD);
      if (error != 0) {
        printf("Error code %d\n", error);
        printf("c[%d]=%d\n", error, fmatmul_c[error]);
        return error;
      } else {
        printf("Passed.\n");
      }
    }
  }

  printf("\n");
  printf("=====================\n");
  printf("=  FMATMUL (SCALAR) =\n");
  printf("=====================\n");
  printf("\n");
  printf("\n");

  for (uint64_t s = 4; s <= fmatmul_M; s *= 2) {
    printf("\n");
    printf("------------------------------------------------------------\n");
    printf("Calculating a (%d x %d) x (%d x %d) matrix multiplication...\n", s,
           s, s, s);
    printf("------------------------------------------------------------\n");
    printf("\n");

    // Zero output buffer
    for (uint64_t i = 0; i < fmatmul_M * fmatmul_P; ++i)
      fmatmul_c[i] = 0.0;

    printf("Calculating fmatmul (scalar)...\n");
    start_timer();
    start_timer_time();
    fmatmul_scalar(fmatmul_c, fmatmul_a, fmatmul_b, s, s, s);
    stop_timer();
    stop_timer_time();

    // Metrics
    int64_t runtime = get_timer();
    int64_t runtime_time = get_timer_time();
    float performance = 2.0 * s * s * s / runtime;
    float performance_time = 2.0 * s * s * s / runtime_time;

    printf("The execution took %d cycles.\n", runtime);
    printf("The performance is %f FLOP/cycle.\n", performance);
    printf("The execution took %d timebase-ticks.\n", runtime_time);
    printf("The performance is %f FLOP/tick.\n", performance_time);

    // Verify the result only for s == M (to keep it simple)
    if (s == fmatmul_M) {
      printf("Verifying result...\n");
      int error = verify_matrix(fmatmul_c, fmatmul_g, s, s, THRESHOLD);
      if (error != 0) {
        printf("Error code %d\n", error);
        printf("c[%d]=%d\n", error, fmatmul_c[error]);
        return error;
      } else {
        printf("Passed.\n");
      }
    }
  }

  return 0;
}
