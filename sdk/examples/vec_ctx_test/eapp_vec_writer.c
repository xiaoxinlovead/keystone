/* Enclave: write vector registers, save to UTM, verify read-back */

#include <stdint.h>
#define UTM_BASE ((volatile uint64_t *)0x41000000)

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void sbi_puthex(unsigned long x) {
  for (int i = 60; i >= 0; i -= 4) {
    int nibble = (int)(x >> i) & 0xf;
    sbi_putchar(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
  }
}
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,1101\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}

/* Store all 32 v registers to UTM using LMUL=m1, VL=1.
 * Each vse64.v stores 1 element (8 bytes), offset by 8 each.
 * base_reg must be "(%[ptr])" — a register indirect operand. */
#define VSET_M1  "vsetivli zero, 1, e64, m1, ta, ma\n\t"
#define STORE_ALL(base_reg) \
  VSET_M1 \
  "vse64.v v0, " base_reg "\n\t" \
  "addi t0, %[ptr], 8\n\t"     "vse64.v v1, (t0)\n\t" \
  "addi t0, %[ptr], 16\n\t"    "vse64.v v2, (t0)\n\t" \
  "addi t0, %[ptr], 24\n\t"    "vse64.v v3, (t0)\n\t" \
  "addi t0, %[ptr], 32\n\t"    "vse64.v v4, (t0)\n\t" \
  "addi t0, %[ptr], 40\n\t"    "vse64.v v5, (t0)\n\t" \
  "addi t0, %[ptr], 48\n\t"    "vse64.v v6, (t0)\n\t" \
  "addi t0, %[ptr], 56\n\t"    "vse64.v v7, (t0)\n\t" \
  "addi t0, %[ptr], 64\n\t"    "vse64.v v8, (t0)\n\t" \
  "addi t0, %[ptr], 72\n\t"    "vse64.v v9, (t0)\n\t" \
  "addi t0, %[ptr], 80\n\t"    "vse64.v v10, (t0)\n\t" \
  "addi t0, %[ptr], 88\n\t"    "vse64.v v11, (t0)\n\t" \
  "addi t0, %[ptr], 96\n\t"    "vse64.v v12, (t0)\n\t" \
  "addi t0, %[ptr], 104\n\t"   "vse64.v v13, (t0)\n\t" \
  "addi t0, %[ptr], 112\n\t"   "vse64.v v14, (t0)\n\t" \
  "addi t0, %[ptr], 120\n\t"   "vse64.v v15, (t0)\n\t" \
  "addi t0, %[ptr], 128\n\t"   "vse64.v v16, (t0)\n\t" \
  "addi t0, %[ptr], 136\n\t"   "vse64.v v17, (t0)\n\t" \
  "addi t0, %[ptr], 144\n\t"   "vse64.v v18, (t0)\n\t" \
  "addi t0, %[ptr], 152\n\t"   "vse64.v v19, (t0)\n\t" \
  "addi t0, %[ptr], 160\n\t"   "vse64.v v20, (t0)\n\t" \
  "addi t0, %[ptr], 168\n\t"   "vse64.v v21, (t0)\n\t" \
  "addi t0, %[ptr], 176\n\t"   "vse64.v v22, (t0)\n\t" \
  "addi t0, %[ptr], 184\n\t"   "vse64.v v23, (t0)\n\t" \
  "addi t0, %[ptr], 192\n\t"   "vse64.v v24, (t0)\n\t" \
  "addi t0, %[ptr], 200\n\t"   "vse64.v v25, (t0)\n\t" \
  "addi t0, %[ptr], 208\n\t"   "vse64.v v26, (t0)\n\t" \
  "addi t0, %[ptr], 216\n\t"   "vse64.v v27, (t0)\n\t" \
  "addi t0, %[ptr], 224\n\t"   "vse64.v v28, (t0)\n\t" \
  "addi t0, %[ptr], 232\n\t"   "vse64.v v29, (t0)\n\t" \
  "addi t0, %[ptr], 240\n\t"   "vse64.v v30, (t0)\n\t" \
  "addi t0, %[ptr], 248\n\t"   "vse64.v v31, (t0)\n\t"

#define LOAD_ALL(base_reg) \
  VSET_M1 \
  "vle64.v v0, " base_reg "\n\t" \
  "addi t0, %[ptr], 8\n\t"     "vle64.v v1, (t0)\n\t" \
  "addi t0, %[ptr], 16\n\t"    "vle64.v v2, (t0)\n\t" \
  "addi t0, %[ptr], 24\n\t"    "vle64.v v3, (t0)\n\t" \
  "addi t0, %[ptr], 32\n\t"    "vle64.v v4, (t0)\n\t" \
  "addi t0, %[ptr], 40\n\t"    "vle64.v v5, (t0)\n\t" \
  "addi t0, %[ptr], 48\n\t"    "vle64.v v6, (t0)\n\t" \
  "addi t0, %[ptr], 56\n\t"    "vle64.v v7, (t0)\n\t" \
  "addi t0, %[ptr], 64\n\t"    "vle64.v v8, (t0)\n\t" \
  "addi t0, %[ptr], 72\n\t"    "vle64.v v9, (t0)\n\t" \
  "addi t0, %[ptr], 80\n\t"    "vle64.v v10, (t0)\n\t" \
  "addi t0, %[ptr], 88\n\t"    "vle64.v v11, (t0)\n\t" \
  "addi t0, %[ptr], 96\n\t"    "vle64.v v12, (t0)\n\t" \
  "addi t0, %[ptr], 104\n\t"   "vle64.v v13, (t0)\n\t" \
  "addi t0, %[ptr], 112\n\t"   "vle64.v v14, (t0)\n\t" \
  "addi t0, %[ptr], 120\n\t"   "vle64.v v15, (t0)\n\t" \
  "addi t0, %[ptr], 128\n\t"   "vle64.v v16, (t0)\n\t" \
  "addi t0, %[ptr], 136\n\t"   "vle64.v v17, (t0)\n\t" \
  "addi t0, %[ptr], 144\n\t"   "vle64.v v18, (t0)\n\t" \
  "addi t0, %[ptr], 152\n\t"   "vle64.v v19, (t0)\n\t" \
  "addi t0, %[ptr], 160\n\t"   "vle64.v v20, (t0)\n\t" \
  "addi t0, %[ptr], 168\n\t"   "vle64.v v21, (t0)\n\t" \
  "addi t0, %[ptr], 176\n\t"   "vle64.v v22, (t0)\n\t" \
  "addi t0, %[ptr], 184\n\t"   "vle64.v v23, (t0)\n\t" \
  "addi t0, %[ptr], 192\n\t"   "vle64.v v24, (t0)\n\t" \
  "addi t0, %[ptr], 200\n\t"   "vle64.v v25, (t0)\n\t" \
  "addi t0, %[ptr], 208\n\t"   "vle64.v v26, (t0)\n\t" \
  "addi t0, %[ptr], 216\n\t"   "vle64.v v27, (t0)\n\t" \
  "addi t0, %[ptr], 224\n\t"   "vle64.v v28, (t0)\n\t" \
  "addi t0, %[ptr], 232\n\t"   "vle64.v v29, (t0)\n\t" \
  "addi t0, %[ptr], 240\n\t"   "vle64.v v30, (t0)\n\t" \
  "addi t0, %[ptr], 248\n\t"   "vle64.v v31, (t0)\n\t"

void _start(void) {
  __asm__ __volatile__ (".option push\n.option norelax\nla gp, __global_pointer$\n.option pop\n");

  /* Write unique pattern to each v register (LMUL=m1, VL=1). */
  __asm__ __volatile__ (
    VSET_M1
    "li t0, 0xDEAD000000000000\n\t" "vmv.v.x v0, t0\n\t"
    "li t0, 0xDEAD000000000001\n\t" "vmv.v.x v1, t0\n\t"
    "li t0, 0xDEAD000000000002\n\t" "vmv.v.x v2, t0\n\t"
    "li t0, 0xDEAD000000000003\n\t" "vmv.v.x v3, t0\n\t"
    "li t0, 0xDEAD000000000004\n\t" "vmv.v.x v4, t0\n\t"
    "li t0, 0xDEAD000000000005\n\t" "vmv.v.x v5, t0\n\t"
    "li t0, 0xDEAD000000000006\n\t" "vmv.v.x v6, t0\n\t"
    "li t0, 0xDEAD000000000007\n\t" "vmv.v.x v7, t0\n\t"
    "li t0, 0xDEAD000000000008\n\t" "vmv.v.x v8, t0\n\t"
    "li t0, 0xDEAD000000000009\n\t" "vmv.v.x v9, t0\n\t"
    "li t0, 0xDEAD00000000000a\n\t" "vmv.v.x v10, t0\n\t"
    "li t0, 0xDEAD00000000000b\n\t" "vmv.v.x v11, t0\n\t"
    "li t0, 0xDEAD00000000000c\n\t" "vmv.v.x v12, t0\n\t"
    "li t0, 0xDEAD00000000000d\n\t" "vmv.v.x v13, t0\n\t"
    "li t0, 0xDEAD00000000000e\n\t" "vmv.v.x v14, t0\n\t"
    "li t0, 0xDEAD00000000000f\n\t" "vmv.v.x v15, t0\n\t"
    "li t0, 0xDEAD000000000010\n\t" "vmv.v.x v16, t0\n\t"
    "li t0, 0xDEAD000000000011\n\t" "vmv.v.x v17, t0\n\t"
    "li t0, 0xDEAD000000000012\n\t" "vmv.v.x v18, t0\n\t"
    "li t0, 0xDEAD000000000013\n\t" "vmv.v.x v19, t0\n\t"
    "li t0, 0xDEAD000000000014\n\t" "vmv.v.x v20, t0\n\t"
    "li t0, 0xDEAD000000000015\n\t" "vmv.v.x v21, t0\n\t"
    "li t0, 0xDEAD000000000016\n\t" "vmv.v.x v22, t0\n\t"
    "li t0, 0xDEAD000000000017\n\t" "vmv.v.x v23, t0\n\t"
    "li t0, 0xDEAD000000000018\n\t" "vmv.v.x v24, t0\n\t"
    "li t0, 0xDEAD000000000019\n\t" "vmv.v.x v25, t0\n\t"
    "li t0, 0xDEAD00000000001a\n\t" "vmv.v.x v26, t0\n\t"
    "li t0, 0xDEAD00000000001b\n\t" "vmv.v.x v27, t0\n\t"
    "li t0, 0xDEAD00000000001c\n\t" "vmv.v.x v28, t0\n\t"
    "li t0, 0xDEAD00000000001d\n\t" "vmv.v.x v29, t0\n\t"
    "li t0, 0xDEAD00000000001e\n\t" "vmv.v.x v30, t0\n\t"
    "li t0, 0xDEAD00000000001f\n\t" "vmv.v.x v31, t0\n\t"
    :
    :
    : "t0"
  );

  sbi_puts("A: v0-v31 set\n");

  /* Save to UTM[0..31] (32 stores, 8 bytes each = 256 bytes) */
  __asm__ __volatile__ (
    STORE_ALL("(%[ptr])")
    :
    : [ptr] "r"(UTM_BASE)
    : "t0", "memory"
  );

  sbi_puts("A: saved, verifying read-back\n");

  /* Zero out all v registers */
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

  /* Read back from UTM into v registers */
  __asm__ __volatile__ (
    LOAD_ALL("(%[ptr])")
    :
    : [ptr] "r"(UTM_BASE)
    : "t0", "memory"
  );

  /* Store read-back values to UTM[32..63] */
  __asm__ __volatile__ (
    STORE_ALL("(%[ptr])")
    :
    : [ptr] "r"(UTM_BASE + 32)
    : "t0", "memory"
  );

  /* Print table */
  sbi_putchar('\n');
  for (int i = 0; i < 32; i++) {
    sbi_puts("  v"); sbi_puthex(i);
    sbi_puts(" 0x"); sbi_puthex(UTM_BASE[i]);
    sbi_puts(" 0x"); sbi_puthex(UTM_BASE[32 + i]);
    if (UTM_BASE[i] != UTM_BASE[32 + i])
      sbi_puts(" MISMATCH");
    sbi_putchar('\n');
  }

  sbi_puts("A: exiting\n");
  sbi_exit(0);
}
