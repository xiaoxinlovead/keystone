#include <stdint.h>
#include <string.h>
#include "task.h"
#include "../fconv2d/fconv2d.h"
#include "../fmatmul/kernel/fmatmul.h"
#include "../common/util.h"
#include "../common/runtime.h"

#define UTM_BASE ((volatile char*)0x41000000)

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7", "a0");
}
static void putdec(long x) {
  char buf[24]; int p = 23; buf[23] = 0;
  if (x < 0) { sbi_putchar('-'); x = -x; }
  do { buf[--p] = '0' + (x % 10); x /= 10; } while (x);
  sbi_puts(buf + p);
}

#define FCOUT_ROWS 112
#define FCOUT_COLS 112
#define MMOUT_SIZE 128

extern double fconv2d_i[], fconv2d_f[], fconv2d_o[], fconv2d_golden_o[];
extern uint64_t fmatmul_a[], fmatmul_b[], fmatmul_c[], fmatmul_g[];

static int verify(double *result, double *gold, size_t n, double thresh) {
  for (size_t i = 0; i < n; i++)
    if (!similarity_check(result[i], gold[i], thresh)) return (int)i + 1;
  return 0;
}

void _start(void) {
  volatile task_info_t *ti = (volatile task_info_t*)(UTM_BASE + UTM_OFFSET_TASK);
  double *utm_fc_out = (double*)(UTM_BASE + UTM_OFFSET_FC_OUT);
  double *utm_fc_ref = (double*)(UTM_BASE + UTM_OFFSET_FC_REF);
  double *utm_mm_out = (double*)(UTM_BASE + UTM_OFFSET_MM_OUT);
  double *utm_mm_ref = (double*)(UTM_BASE + UTM_OFFSET_MM_REF);

  int type = ti->task_type;
  int fs = 7;

  if (type == TASK_BOTH) {
    // Full reference: scalar compute → utm_fc_ref/utm_mm_ref
    sbi_puts("[B] fc... ");
    memset(fconv2d_o, 0, FCOUT_ROWS * FCOUT_COLS * 8);
    fconv2d_7x7_scalar(utm_fc_ref, fconv2d_i, fconv2d_f, FCOUT_ROWS, FCOUT_COLS, fs);
    sbi_puts("OK mm... ");
    memset(fmatmul_c, 0, MMOUT_SIZE * MMOUT_SIZE * 8);
    fmatmul_scalar(utm_mm_ref, (double*)fmatmul_a, (double*)fmatmul_b, MMOUT_SIZE, MMOUT_SIZE, MMOUT_SIZE);
    sbi_puts("OK\n");
    ti->pass = 1;
    sbi_exit(0);
  }

  if (type == TASK_FCONV2D) {
    int start = ti->start_row, end = ti->end_row;
    // Vector compute (full), then copy requested rows to utm_fc_out
    memset(fconv2d_o, 0, FCOUT_ROWS * FCOUT_COLS * 8);
    if (fs == 7) fconv2d_7x7(fconv2d_o, fconv2d_i, fconv2d_f, FCOUT_ROWS, FCOUT_COLS, fs);
    else fconv2d_3x3(fconv2d_o, fconv2d_i, fconv2d_f, FCOUT_ROWS, FCOUT_COLS, fs);
    for (int r = start; r < end; r++)
      memcpy((void*)(utm_fc_out + r * FCOUT_COLS), fconv2d_o + r * FCOUT_COLS, FCOUT_COLS * 8);
    sbi_puts("[FC] rows "); putdec(start); sbi_puts("-"); putdec(end-1); sbi_puts("\n");
    ti->pass = 1;
    sbi_exit(0);
  }

  if (type == TASK_FMATMUL) {
    int start = ti->start_row, end = ti->end_row;
    memset(fmatmul_c, 0, MMOUT_SIZE * MMOUT_SIZE * 8);
    fmatmul((double*)fmatmul_c, (double*)fmatmul_a, (double*)fmatmul_b, MMOUT_SIZE, MMOUT_SIZE, MMOUT_SIZE);
    for (int r = start; r < end; r++)
      memcpy((void*)(utm_mm_out + r * MMOUT_SIZE), (double*)fmatmul_c + r * MMOUT_SIZE, MMOUT_SIZE * 8);
    sbi_puts("[MM] rows "); putdec(start); sbi_puts("-"); putdec(end-1); sbi_puts("\n");
    ti->pass = 1;
    sbi_exit(0);
  }

  if (type == TASK_CHECK) {
    // Compare assembled partial results (OUT) with full reference (REF)
    sbi_puts("[CK] fc... ");
    int fc_ok = verify(utm_fc_out, utm_fc_ref, FCOUT_ROWS * FCOUT_COLS, 0.001);
    sbi_puts(fc_ok ? "FAIL " : "OK ");
    sbi_puts("mm... ");
    int mm_ok = verify(utm_mm_out, utm_mm_ref, MMOUT_SIZE * MMOUT_SIZE, 0.001);
    sbi_puts(mm_ok ? "FAIL " : "OK ");
    ti->pass = (fc_ok == 0 && mm_ok == 0);
    sbi_puts(ti->pass ? "PASS\n" : "FAIL\n");
    sbi_exit(0);
  }

  sbi_exit(1);
}
