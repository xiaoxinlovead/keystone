/* Enclave B: Corrupt vector registers (set all to zero) */

#include <stdint.h>
#define UTM_BASE ((volatile uint64_t *)0x41000000)

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void sbi_exit(int code) {
  __asm__ __volatile__ (
    "li a7, 1101\n\t"
    "mv a0, %0\n\t"
    "ecall\n\t"
    :
    : "r"((unsigned long)code)
    : "a7", "a0"
  );
}

#define VSET_M1 "vsetivli zero, 1, e64, m1, ta, ma\n\t"
#define VSET_M8 "vsetivli zero, 1, e64, m8, ta, ma\n\t"

void _start(void) {
  __asm__ __volatile__ (".option push\n.option norelax\nla gp, __global_pointer$\n.option pop\n");

  sbi_puts("B: zeroing v0-v31\n");

  /* LMUL=m1 allows all 32 registers as vm.v.i destinations */
  __asm__ __volatile__ (
    VSET_M1
    "vmv.v.i v0, 0\n\t"  "vmv.v.i v1, 0\n\t"
    "vmv.v.i v2, 0\n\t"  "vmv.v.i v3, 0\n\t"
    "vmv.v.i v4, 0\n\t"  "vmv.v.i v5, 0\n\t"
    "vmv.v.i v6, 0\n\t"  "vmv.v.i v7, 0\n\t"
    "vmv.v.i v8, 0\n\t"  "vmv.v.i v9, 0\n\t"
    "vmv.v.i v10, 0\n\t" "vmv.v.i v11, 0\n\t"
    "vmv.v.i v12, 0\n\t" "vmv.v.i v13, 0\n\t"
    "vmv.v.i v14, 0\n\t" "vmv.v.i v15, 0\n\t"
    "vmv.v.i v16, 0\n\t" "vmv.v.i v17, 0\n\t"
    "vmv.v.i v18, 0\n\t" "vmv.v.i v19, 0\n\t"
    "vmv.v.i v20, 0\n\t" "vmv.v.i v21, 0\n\t"
    "vmv.v.i v22, 0\n\t" "vmv.v.i v23, 0\n\t"
    "vmv.v.i v24, 0\n\t" "vmv.v.i v25, 0\n\t"
    "vmv.v.i v26, 0\n\t" "vmv.v.i v27, 0\n\t"
    "vmv.v.i v28, 0\n\t" "vmv.v.i v29, 0\n\t"
    "vmv.v.i v30, 0\n\t" "vmv.v.i v31, 0\n\t"
  );

  /* Store first 8 regs to UTM as proof */
  __asm__ __volatile__ (
    VSET_M8
    "vse64.v v0, (%0)\n\t"
    :
    : "r"(UTM_BASE)
    : "memory"
  );

  sbi_puts("B: done, exiting\n");
  sbi_exit(0);
}
