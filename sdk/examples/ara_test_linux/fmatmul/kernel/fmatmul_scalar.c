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

// Scalar (non-vector) fallback implementation of fmatmul

#include "fmatmul.h"

void fmatmul_scalar(double *c, const double *a, const double *b,
                    const unsigned long int M, const unsigned long int N,
                    const unsigned long int P) {
  for (unsigned long int i = 0; i < M; ++i) {
    for (unsigned long int j = 0; j < P; ++j) {
      double sum = 0.0;
      for (unsigned long int k = 0; k < N; ++k) {
        sum += a[i * N + k] * b[k * P + j];
      }
      c[i * P + j] = sum;
    }
  }
}
