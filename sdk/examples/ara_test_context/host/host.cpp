#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "host/keystone.h"
#include "edge/edge_call.h"
#include "task.h"

using namespace Keystone;

#define UTM_SIZE (1024 * 1024)

static double get_usec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

static double run_enclave(Enclave &enclave, void *utm_buf, const char *name) {
  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals((uintptr_t)utm_buf, UTM_SIZE);
  double start = get_usec();
  Error err = enclave.run();
  double end = get_usec();
  if (err != Error::Success) {
    fprintf(stderr, "[%s] run failed: %d\n", name, (int)err);
    return -1;
  }
  return (end - start);
}

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <fc.pkg> <mm.pkg> <both.pkg> <check.pkg>\n", argv[0]);
    return 1;
  }
  void *utm_buf = malloc(UTM_SIZE);
  for (size_t i = 0; i < UTM_SIZE; i++) ((char*)utm_buf)[i] = 0;
  volatile task_info_t *ti = (volatile task_info_t*)((char*)utm_buf + UTM_OFFSET_TASK);

  Params params;
  params.setFreeMemSize(1024 * 1024);
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, UTM_SIZE);

  fprintf(stderr, "\n=== Phase 1: Baseline (both full) ===\n");
  Enclave both_encl;
  if (both_encl.init(argv[3], argv[3], params) != Error::Success) return 1;
  ti->task_type = TASK_BOTH; ti->start_row = 0; ti->end_row = FCONV2D_M;
  double t_both = run_enclave(both_encl, utm_buf, "both");
  fprintf(stderr, "[BOTH] time=%.0f us\n", t_both);

  char *fc_ref = (char*)utm_buf + UTM_OFFSET_FC_REF;
  char *fc_out = (char*)utm_buf + UTM_OFFSET_FC_OUT;
  for (size_t i = 0; i < FCONV2D_M * FCONV2D_N * 8; i++) fc_ref[i] = fc_out[i];
  char *mm_ref = (char*)utm_buf + UTM_OFFSET_MM_REF;
  char *mm_out = (char*)utm_buf + UTM_OFFSET_MM_OUT;
  for (size_t i = 0; i < FMATMUL_M * FMATMUL_M * 8; i++) mm_ref[i] = mm_out[i];

  fprintf(stderr, "\n=== Phase 2: Interleaved partial ===\n");
  ti->task_type = TASK_FCONV2D; ti->start_row = 0; ti->end_row = FCONV2D_M / 2;
  Enclave fc_encl;
  if (fc_encl.init(argv[1], argv[1], params) != Error::Success) return 1;
  double t_fc0 = run_enclave(fc_encl, utm_buf, "fc0");

  ti->task_type = TASK_FMATMUL; ti->start_row = 0; ti->end_row = FMATMUL_M / 2;
  Enclave mm_encl;
  if (mm_encl.init(argv[2], argv[2], params) != Error::Success) return 1;
  double t_mm0 = run_enclave(mm_encl, utm_buf, "mm0");

  ti->task_type = TASK_FCONV2D; ti->start_row = FCONV2D_M / 2; ti->end_row = FCONV2D_M;
  if (fc_encl.init(argv[1], argv[1], params) != Error::Success) return 1;
  double t_fc1 = run_enclave(fc_encl, utm_buf, "fc1");

  ti->task_type = TASK_FMATMUL; ti->start_row = FMATMUL_M / 2; ti->end_row = FMATMUL_M;
  if (mm_encl.init(argv[2], argv[2], params) != Error::Success) return 1;
  double t_mm1 = run_enclave(mm_encl, utm_buf, "mm1");

  double t_interleaved = t_fc0 + t_mm0 + t_fc1 + t_mm1;
  fprintf(stderr, "[FC0]  time=%.0f us\n", t_fc0);
  fprintf(stderr, "[MM0]  time=%.0f us\n", t_mm0);
  fprintf(stderr, "[FC1]  time=%.0f us\n", t_fc1);
  fprintf(stderr, "[MM1]  time=%.0f us\n", t_mm1);
  fprintf(stderr, "[TOTAL] time=%.0f us\n", t_interleaved);

  fprintf(stderr, "\n=== Phase 3: Verification ===\n");
  ti->task_type = TASK_CHECK; ti->pass = 0;
  Enclave check_encl;
  if (check_encl.init(argv[4], argv[4], params) != Error::Success) return 1;
  double t_check = run_enclave(check_encl, utm_buf, "check");

  int passed = ti->pass;
  fprintf(stderr, "\n=== SUMMARY ===\n");
  fprintf(stderr, "Baseline (both full):  %.0f us\n", t_both);
  fprintf(stderr, "Interleaved (partial): %.0f us\n", t_interleaved);
  fprintf(stderr, "Verification:          %s\n", passed ? "PASS" : "FAIL");
  if (t_both > 0) fprintf(stderr, "Overhead ratio:        %.2fx\n", t_interleaved / t_both);
  return passed ? 0 : 1;
}
