/* Enclave B: Corrupt vector registers with a known pattern */
#include <stdint.h>

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,1101\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}

void _start(void) {
  __asm__ __volatile__ (".option push\n.option norelax\nla gp, __global_pointer$\n.option pop\n");

  sbi_puts("B: zeroing v0-v31\n");

  __asm__ __volatile__ (
    "vsetivli zero, 1, e64, m1, ta, ma\n\t"
    "vxor.vv v0, v0, v0\n\t"   "vxor.vv v1, v1, v1\n\t"
    "vxor.vv v2, v2, v2\n\t"   "vxor.vv v3, v3, v3\n\t"
    "vxor.vv v4, v4, v4\n\t"   "vxor.vv v5, v5, v5\n\t"
    "vxor.vv v6, v6, v6\n\t"   "vxor.vv v7, v7, v7\n\t"
    "vxor.vv v8, v8, v8\n\t"   "vxor.vv v9, v9, v9\n\t"
    "vxor.vv v10, v10, v10\n\t" "vxor.vv v11, v11, v11\n\t"
    "vxor.vv v12, v12, v12\n\t" "vxor.vv v13, v13, v13\n\t"
    "vxor.vv v14, v14, v14\n\t" "vxor.vv v15, v15, v15\n\t"
    "vxor.vv v16, v16, v16\n\t" "vxor.vv v17, v17, v17\n\t"
    "vxor.vv v18, v18, v18\n\t" "vxor.vv v19, v19, v19\n\t"
    "vxor.vv v20, v20, v20\n\t" "vxor.vv v21, v21, v21\n\t"
    "vxor.vv v22, v22, v22\n\t" "vxor.vv v23, v23, v23\n\t"
    "vxor.vv v24, v24, v24\n\t" "vxor.vv v25, v25, v25\n\t"
    "vxor.vv v26, v26, v26\n\t" "vxor.vv v27, v27, v27\n\t"
    "vxor.vv v28, v28, v28\n\t" "vxor.vv v29, v29, v29\n\t"
    "vxor.vv v30, v30, v30\n\t" "vxor.vv v31, v31, v31\n\t"
  );

  sbi_puts("B: done, exiting\n");
  sbi_exit(0);
}
