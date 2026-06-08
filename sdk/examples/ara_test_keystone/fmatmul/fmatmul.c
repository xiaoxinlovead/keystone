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

#define trace_puts(S) do { const char*_s=(S); while(*_s)trace_putchar(*_s++); } while(0)
static void trace_puthex(unsigned long x) {
  for (int i = 60; i >= 0; i -= 4) {
    int nibble = (int)(x >> i) & 0xf;
    trace_putchar(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
  }
}
static void trace_putdec(long x) {
  char buf[24]; int p = 23; buf[23] = 0;
  if (x < 0) { trace_putchar('-'); x = -x; }
  do { buf[--p] = '0' + (x % 10); x /= 10; } while (x);
  trace_puts(buf + p);
}

#define FMATMUL_M 128
#define FMATMUL_P 128

extern double fmatmul_a[] __attribute__((aligned(32 * NR_LANES)));
extern double fmatmul_b[] __attribute__((aligned(32 * NR_LANES)));
extern double fmatmul_c[] __attribute__((aligned(32 * NR_LANES)));
extern double fmatmul_g[] __attribute__((aligned(32 * NR_LANES)));

#define THRESHOLD 0.001

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

static void run_fmatmul_vec_section(void) {
  trace_puts("\n============================================================\n");
  trace_puts("=  FMATMUL (VEC)  =\n");
  trace_puts("============================================================\n\n");
  for (uint64_t s = 4; s <= FMATMUL_M; s *= 2) {
    trace_puts("--- s="); trace_putdec(s); trace_puts(" ---\n");
    trace_puts("Calculating fmatmul (vector)...\n");
    start_timer();
    start_timer_time();
    fmatmul(fmatmul_c, fmatmul_a, fmatmul_b, s, s, s);
    stop_timer();
    stop_timer_time();
    int64_t runtime = get_timer();
    int64_t runtime_time = get_timer_time();
    trace_puts("The execution took "); trace_putdec(runtime); trace_puts(" time ticks.\n");
    trace_puts("The execution took "); trace_putdec(runtime_time); trace_puts(" timebase-ticks.\n");
    if (s == FMATMUL_M) {
      trace_puts("Verifying result...\n");
      int error = verify_matrix(fmatmul_c, fmatmul_g, s, s, THRESHOLD);
      if (error != 0) {
        trace_puts("Error code "); trace_putdec(error); trace_puts("\n");
        return;
      }
      trace_puts("Passed.\n");
    }
  }
}

static void run_fmatmul_scalar_section(void) {
  trace_puts("\n============================================================\n");
  trace_puts("=  FMATMUL (SCALAR) =\n");
  trace_puts("============================================================\n\n");
  for (uint64_t s = 4; s <= FMATMUL_M; s *= 2) {
    trace_puts("--- s="); trace_putdec(s); trace_puts(" ---\n");
    for (uint64_t i = 0; i < FMATMUL_M * FMATMUL_P; ++i)
      fmatmul_c[i] = 0.0;
    trace_puts("Calculating fmatmul (scalar)...\n");
    start_timer();
    start_timer_time();
    fmatmul_scalar(fmatmul_c, fmatmul_a, fmatmul_b, s, s, s);
    stop_timer();
    stop_timer_time();
    int64_t runtime = get_timer();
    int64_t runtime_time = get_timer_time();
    trace_puts("The execution took "); trace_putdec(runtime); trace_puts(" time ticks.\n");
    trace_puts("The execution took "); trace_putdec(runtime_time); trace_puts(" timebase-ticks.\n");
    if (s == FMATMUL_M) {
      trace_puts("Verifying result...\n");
      int error = verify_matrix(fmatmul_c, fmatmul_g, s, s, THRESHOLD);
      if (error != 0) {
        trace_puts("Error code "); trace_putdec(error); trace_puts("\n");
        return;
      }
      trace_puts("Passed.\n");
    }
  }
}

int run_fmatmul() {
  trace_putchar('F'); trace_putchar('\n');
  run_fmatmul_vec_section();
  run_fmatmul_scalar_section();
  return 0;
}
