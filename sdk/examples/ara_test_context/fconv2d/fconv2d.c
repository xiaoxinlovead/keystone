#include "util.h"
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

// Author: Matteo Perotti

#include <stdint.h>
#include <string.h>

#include "fconv2d.h"
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
// o = i ° f, with i=[MxN], f=[FxF], o=[MxN]
// The filter is a square matrix, and F is odd

// Matrices defined in data.S
extern double fconv2d_i[] __attribute__((
    aligned(4 * NR_LANES))); // [ (M+floor(F/2)) * (N+floor(F/2)) ]
extern double fconv2d_f[] __attribute__((aligned(4 * NR_LANES)));        // [ F*F ]
extern double fconv2d_o[] __attribute__((aligned(4 * NR_LANES)));        // [ M*N ]
extern double fconv2d_golden_o[] __attribute__((aligned(4 * NR_LANES))); // [ M*N ]
// M, N, F from data.S (hardcoded to avoid FEP external var loading bug)
#define FCONV2D_M 112
#define FCONV2D_N 112
#define FCONV2D_F 7

// Verify the matrices
int verify_matrix(double *matrix, double *golden_matrix, int64_t R, int64_t C,
                  double threshold) {
  for (int r = 0; r < R; ++r)
    for (int c = 0; c < C; ++c)
      if (!similarity_check(matrix[c + C * r], golden_matrix[c + C * r],
                            threshold)) {
        printf("Error: o[%d][%d] = %lf, instead of %lf\n", r, c,
               matrix[c + C * r], golden_matrix[c + C * r]);
        return 1;
      }
  return 0;
}

void print_matrix(double const *matrix, uint64_t num_rows,
                  uint64_t num_columns) {
  printf("0x%8X\n", (uint64_t)matrix);
  for (uint64_t i = 0; i < num_rows; ++i) {
    for (uint64_t j = 0; j < num_columns; ++j) {
      printf("%10f ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int run_fconv2d() {
  printf("\n");
  printf("===================\n");
  printf("=  FCONV2D (VEC)  =\n");
  printf("===================\n");
  printf("\n");
  printf("\n");

  // Call the main kernel, and measure cycles
  start_timer();
  trace_putchar('v');
  start_timer_time();
  if (FCONV2D_F == 3)
    fconv2d_3x3(fconv2d_o, fconv2d_i, fconv2d_f, FCONV2D_M, FCONV2D_N, FCONV2D_F);
  else if (FCONV2D_F == 7)
    fconv2d_7x7(fconv2d_o, fconv2d_i, fconv2d_f, FCONV2D_M, FCONV2D_N, FCONV2D_F);
  else
    printf("Error: the filter size is different from 3 or 5 or 7.\n");
  stop_timer();
  stop_timer_time();

  // Performance metrics
  int64_t runtime = get_timer();
  int64_t runtime_time = get_timer_time();
  float performance = 2.0 * FCONV2D_F * FCONV2D_F * FCONV2D_M * FCONV2D_N / runtime;
  float utilization = 100 * performance / (2.0 * NR_LANES);
  float performance_time = 2.0 * FCONV2D_F * FCONV2D_F * FCONV2D_M * FCONV2D_N / runtime_time;
  float utilization_time = 100 * performance_time / (2.0 * NR_LANES);

  printf("The execution took %d time ticks.\n", runtime);
  printf("The performance is %f DPFLOP/tick (%f%% utilization).\n",
         performance, utilization);
  printf("The execution took %d timebase-ticks.\n", runtime_time);
  printf("The performance is %f DPFLOP/tick (%f%% utilization).\n",
         performance_time, utilization_time);

  // Verify correctness
  printf("Verifying result...\n");
  int error = verify_matrix(fconv2d_o, fconv2d_golden_o, FCONV2D_M, FCONV2D_N, THRESHOLD);
  if (error != 0) {
    printf("Fail.\n");
  } else {
  trace_putchar('V');
    printf("Passed.\n");
  }

  printf("\n");
  printf("=======================\n");
  printf("=  FCONV2D (SCALAR)  =\n");
  printf("=======================\n");
  printf("\n");
  printf("\n");

  // Zero output buffer
  for (int64_t i = 0; i < FCONV2D_M * FCONV2D_N; ++i)
    fconv2d_o[i] = 0.0;

  trace_putchar('s');
  start_timer();
  start_timer_time();
  if (FCONV2D_F == 3)
    fconv2d_3x3_scalar(fconv2d_o, fconv2d_i, fconv2d_f, FCONV2D_M, FCONV2D_N, FCONV2D_F);
  else if (FCONV2D_F == 7)
    fconv2d_7x7_scalar(fconv2d_o, fconv2d_i, fconv2d_f, FCONV2D_M, FCONV2D_N, FCONV2D_F);
  else
    printf("Error: the filter size is different from 3 or 5 or 7.\n");
  stop_timer();
  stop_timer_time();

  runtime = get_timer();
  runtime_time = get_timer_time();
  performance = 2.0 * FCONV2D_F * FCONV2D_F * FCONV2D_M * FCONV2D_N / runtime;
  performance_time = 2.0 * FCONV2D_F * FCONV2D_F * FCONV2D_M * FCONV2D_N / runtime_time;

  printf("The execution took %d time ticks.\n", runtime);
  printf("The performance is %f DPFLOP/tick.\n", performance);
  printf("The execution took %d timebase-ticks.\n", runtime_time);
  printf("The performance is %f DPFLOP/tick.\n", performance_time);

  // Verify correctness
  printf("Verifying result...\n");
  error = verify_matrix(fconv2d_o, fconv2d_golden_o, FCONV2D_M, FCONV2D_N, THRESHOLD);
  if (error != 0) {
  trace_putchar('S');
    printf("Fail.\n");
  } else {
    printf("Passed.\n");
  }

  return error;
}
