#include <asm/csr.h>

#include "util/printf.h"
#include "sys/interrupt.h"
#include "call/syscall.h"
#include "mm/vm.h"
#include "util/string.h"
#include "call/sbi.h"
#include "mm/freemem.h"
#include "mm/mm.h"
#include "sys/env.h"
#include "mm/paging.h"

/* defined in vm.h */
extern uintptr_t shared_buffer;
extern uintptr_t shared_buffer_size;

/* initial memory layout */
uintptr_t utm_base;
size_t utm_size;

/* defined in entry.S */
extern void* encl_trap_handler;

#ifdef USE_FREEMEM


/* map entire enclave physical memory so that
 * we can access the old page table and free memory */
/* remap runtime kernel to a new root page table */
void
map_physical_memory(uintptr_t dram_base,
                    uintptr_t dram_size)
{
  uintptr_t ptr = EYRIE_LOAD_START;
  /* load address should not override kernel address */
  assert(RISCV_GET_PT_INDEX(ptr, 1) != RISCV_GET_PT_INDEX(runtime_va_start, 1));
  assert(dram_size <= EYRIE_LOAD_SIZE_MAX);

  map_with_reserved_page_table(dram_base, dram_size, ptr,
      load_l1_page_table, 0, 0);
}

void
remap_kernel_space(uintptr_t runtime_base,
                   uintptr_t runtime_size)
{
  /* eyrie runtime is mapped with reserved bootstrap page tables. */

  #if __riscv_xlen == 64
  assert(runtime_size <= RISCV_GET_LVL_PGSIZE(2));
  #elif __riscv_xlen == 32
  assert(runtime_size <= RISCV_GET_LVL_PGSIZE(1));
  #endif 

  map_with_reserved_page_table(runtime_base, runtime_size,
     runtime_va_start, kernel_l1_page_table, kernel_l2_page_table,
     kernel_l3_page_table);
}

static int pte_is_leaf(pte entry);
static pte* pte_to_va(pte entry);
static void merge_page_table(pte* dst, pte* src, int level);

void
copy_root_page_table()
{
  /* the old table lives in the first page */
  pte* old_root_page_table = (pte*) EYRIE_LOAD_START;
  merge_page_table(root_page_table, old_root_page_table, RISCV_PT_LEVELS - 1);
  __asm__ volatile("fence rw, rw\nsfence.vma" ::: "memory");
}

static int
pte_is_leaf(pte entry)
{
  return entry & (PTE_R | PTE_W | PTE_X);
}

static pte*
pte_to_va(pte entry)
{
  return (pte*) __va(pte_ppn(entry) << RISCV_PAGE_BITS);
}

static void
merge_page_table(pte* dst, pte* src, int level)
{
  for (int i = 0; i < BIT(RISCV_PT_INDEX_BITS); i++) {
    pte src_entry = src[i];
    if (!(src_entry & PTE_V)) {
      continue;
    }

    pte dst_entry = dst[i];
    if (!(dst_entry & PTE_V)) {
      dst[i] = src_entry;
      continue;
    }

    if (level == 0 || pte_is_leaf(src_entry) || pte_is_leaf(dst_entry)) {
      continue;
    }

    merge_page_table(pte_to_va(dst_entry), pte_to_va(src_entry), level - 1);
  }
}

/* initialize free memory with a simple page allocator*/
void
init_freemem()
{
  spa_init(freemem_va_start, freemem_size);
}

#endif // USE_FREEMEM

/* initialize user stack */
void
init_user_stack_and_env(ELF(Ehdr) *hdr)
{
  void* user_sp = (void*) EYRIE_USER_STACK_START;

#ifdef USE_FREEMEM
  size_t count;
  uintptr_t stack_end = EYRIE_USER_STACK_END;
  size_t stack_count = EYRIE_USER_STACK_SIZE >> RISCV_PAGE_BITS;


  // allocated stack pages right below the runtime
  count = alloc_pages(vpn(stack_end), stack_count,
      PTE_R | PTE_W | PTE_D | PTE_A | PTE_U);

  assert(count == stack_count);

#endif // USE_FREEMEM

  // setup user stack env/aux
  user_sp = setup_start(user_sp, hdr);

  // prepare user sp
  csr_write(sscratch, user_sp);
}

void
eyrie_boot(uintptr_t dummy, // $a0 contains the return value from the SBI
           uintptr_t dram_base,
           uintptr_t dram_size,
           uintptr_t runtime_paddr,
           uintptr_t user_paddr,
           uintptr_t free_paddr,
           uintptr_t utm_vaddr,
           uintptr_t utm_size)
{
  /* Debug output disabled */
  /* sbi_putchar('R'); */

  /* set initial values */
  load_pa_start = dram_base;
  shared_buffer = utm_vaddr;
  shared_buffer_size = utm_size;
  runtime_va_start = (uintptr_t) &rt_base;
  kernel_offset = runtime_va_start - runtime_paddr;

  debug("UTM : 0x%lx-0x%lx (%u KB)", utm_vaddr, utm_vaddr+utm_size, utm_size/1024);
  debug("DRAM: 0x%lx-0x%lx (%u KB)", dram_base, dram_base + dram_size, dram_size/1024);
#ifdef USE_FREEMEM
  freemem_va_start = __va(free_paddr);
  freemem_size = dram_base + dram_size - free_paddr;

  debug("FREE: 0x%lx-0x%lx (%u KB), va 0x%lx", free_paddr, dram_base + dram_size, freemem_size/1024, freemem_va_start);

  /* remap kernel VA */
  remap_kernel_space(runtime_paddr, user_paddr - runtime_paddr);

  map_physical_memory(dram_base, dram_size);

  /* switch to the new page table */
  csr_write(satp, satp_new(kernel_va_to_pa(root_page_table)));
  __asm__ volatile("sfence.vma" ::: "memory");

  /* copy valid entries from the old page table */
  copy_root_page_table();

  /* initialize free memory */
  init_freemem();

  //TODO: This should be set by walking the userspace vm and finding
  //highest used addr. Instead we start partway through the anon space
  set_program_break(EYRIE_ANON_REGION_START + (1024 * 1024 * 1024));

  #ifdef USE_PAGING
  init_paging(user_paddr, free_paddr);
  #endif /* USE_PAGING */
#endif /* USE_FREEMEM */

  /* initialize user stack */
  init_user_stack_and_env((ELF(Ehdr) *) __va(user_paddr));

  /* set trap vector */
  csr_write(stvec, &encl_trap_handler);

  /* prepare edge & system calls */
  init_edge_internals();

  /* set timer */
  init_timer();

  /* Enable FPU and allow S-mode runtime access to UTM/user mappings. */
  csr_write(sstatus, csr_read(sstatus) | SR_FS | SR_SUM);

  debug("eyrie boot finished. drop to the user land ...");
  /* booting all finished, droping to the user land */
  return;
}
