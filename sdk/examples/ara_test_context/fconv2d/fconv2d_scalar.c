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

// Scalar (non-vector) fallback implementations of fconv2d

#include "fconv2d.h"

void fconv2d_3x3_scalar(double *o, double *i, double *f,
                        int64_t M, int64_t N, int64_t F) {
  int64_t stride_i = N + F - 1;
  for (int64_t m = 0; m < M; ++m) {
    for (int64_t n = 0; n < N; ++n) {
      double sum = 0.0;
      for (int64_t x = 0; x < F; ++x) {
        for (int64_t y = 0; y < F; ++y) {
          sum += i[(m + x) * stride_i + (n + y)] * f[x * F + y];
        }
      }
      o[m * N + n] = sum;
    }
  }
}

void fconv2d_7x7_scalar(double *o, double *i, double *f,
                        int64_t M, int64_t N, int64_t F) {
  int64_t stride_i = N + F - 1;
  for (int64_t m = 0; m < M; ++m) {
    for (int64_t n = 0; n < N; ++n) {
      double sum = 0.0;
      for (int64_t x = 0; x < F; ++x) {
        for (int64_t y = 0; y < F; ++y) {
          sum += i[(m + x) * stride_i + (n + y)] * f[x * F + y];
        }
      }
      o[m * N + n] = sum;
    }
  }
}

void fconv2d_3x3_scalar_partial(double *o, double *i, double *f,
                                int64_t start_row, int64_t end_row,
                                int64_t M, int64_t N) {
  int64_t F = 3;
  int64_t stride_i = N + F - 1;
  for (int64_t m = start_row; m < end_row; ++m) {
    for (int64_t n = 0; n < N; ++n) {
      double sum = 0.0;
      for (int64_t x = 0; x < F; ++x)
        for (int64_t y = 0; y < F; ++y)
          sum += i[(m + x) * stride_i + (n + y)] * f[x * F + y];
      o[m * N + n] = sum;
    }
  }
}

void fconv2d_7x7_scalar_partial(double *o, double *i, double *f,
                                int64_t start_row, int64_t end_row,
                                int64_t M, int64_t N) {
  int64_t F = 7;
  int64_t stride_i = N + F - 1;
  for (int64_t m = start_row; m < end_row; ++m) {
    for (int64_t n = 0; n < N; ++n) {
      double sum = 0.0;
      for (int64_t x = 0; x < F; ++x)
        for (int64_t y = 0; y < F; ++y)
          sum += i[(m + x) * stride_i + (n + y)] * f[x * F + y];
      o[m * N + n] = sum;
    }
  }
}
