//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include "thread.h"

/* Host vector register save area (save/restore around enclave entry) */
static uint64_t host_vreg[32][VREG_SAVE_WORDS] __attribute__((aligned(128)));

/* Called when entering enclave: save host vectors, restore enclave vectors */
void switch_to_enclave_vector_context(struct thread_state* thread) {
  unsigned long old_mie = csr_read(mstatus) & MSTATUS_MIE;
  csr_clear(mstatus, MSTATUS_MIE);
  csr_set(mstatus, MSTATUS_VS);
  save_vector_context(host_vreg);
  restore_vector_context(thread->prev_vreg);
  if (old_mie) csr_set(mstatus, MSTATUS_MIE);
}

/* Called when exiting enclave: save enclave vectors, restore host vectors */
void switch_to_host_vector_context(struct thread_state* thread) {
  unsigned long old_mie = csr_read(mstatus) & MSTATUS_MIE;
  csr_clear(mstatus, MSTATUS_MIE);
  csr_set(mstatus, MSTATUS_VS);
  save_vector_context(thread->prev_vreg);
  restore_vector_context(host_vreg);
  if (old_mie) csr_set(mstatus, MSTATUS_MIE);
}

void switch_vector_enclave(){
  extern void trap_vector_enclave();
  csr_write(mtvec, &trap_vector_enclave);
}

void switch_vector_host(){
  extern void _trap_handler();
  csr_write(mtvec, &_trap_handler);
}

void swap_prev_mstatus(struct thread_state* thread, struct sbi_trap_regs* regs, uintptr_t current_mstatus) {
  //Time interrupts can occur in either user mode or supervisor mode
  uintptr_t mstatus_mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP |
                            MSTATUS_MPP | MSTATUS_FS | MSTATUS_SUM |
                            MSTATUS_MXR | MSTATUS_VS;

  uintptr_t tmp = thread->prev_mstatus;
  thread->prev_mstatus = (current_mstatus & ~mstatus_mask) | (current_mstatus & mstatus_mask);
  regs->mstatus = (current_mstatus & ~mstatus_mask) | tmp;
}

/* Swaps the entire s-mode visible state, general registers and then csrs */
void swap_prev_state(struct thread_state* thread, struct sbi_trap_regs* regs, int return_on_resume)
{
  int i;

  uintptr_t* prev = (uintptr_t*) &thread->prev_state;
  for(i=0; i<32; i++)
  {
    /* swap state */
    uintptr_t tmp = prev[i];
    prev[i] = ((unsigned long *)regs)[i];
    ((unsigned long *)regs)[i] = tmp;
  }

  prev[0] = !return_on_resume;

  swap_prev_smode_csrs(thread);

  return;
}

/* Swaps all s-mode csrs defined in 1.10 standard */
/* TODO: Right now we are only handling the ones that our test
   platforms support. Realistically we should have these behind
   defines for extensions (ex: N extension)*/
void swap_prev_smode_csrs(struct thread_state*
thread){

  uintptr_t tmp;

#define LOCAL_SWAP_CSR(csrname) \
  tmp = thread->prev_csrs.csrname;                 \
  thread->prev_csrs.csrname = csr_read(csrname);   \
  csr_write(csrname, tmp);

  LOCAL_SWAP_CSR(sstatus);
  // These only exist with N extension.
  //LOCAL_SWAP_CSR(sedeleg);
  //LOCAL_SWAP_CSR(sideleg);
  LOCAL_SWAP_CSR(sie);
  LOCAL_SWAP_CSR(stvec);
  LOCAL_SWAP_CSR(scounteren);
  LOCAL_SWAP_CSR(sscratch);
  LOCAL_SWAP_CSR(sepc);
  LOCAL_SWAP_CSR(scause);
  LOCAL_SWAP_CSR(stval);
  LOCAL_SWAP_CSR(sip);
  LOCAL_SWAP_CSR(satp);

#undef LOCAL_SWAP_CSR
}

void swap_prev_mepc(struct thread_state* thread, struct sbi_trap_regs* regs, uintptr_t current_mepc)
{
  uintptr_t tmp = thread->prev_mepc;
  thread->prev_mepc = current_mepc;
  regs->mepc = tmp;
}


/* Save v0-v31 to the given save area (512 bytes).
 * Clobbers a0 for address advancement.
 * Caller must have set csr_set(mstatus, MSTATUS_VS) before calling. */
void save_vector_context(uint64_t (*vreg)[VREG_SAVE_WORDS]) {
  register uintptr_t a0 asm("a0") = (uintptr_t)vreg;
  (void)a0;
  __asm__ volatile (
    ".option arch, +v\n"
    "li t0, 16\n"
    "vsetvli zero, t0, e64, m8, ta, ma\n"
    "vse64.v v0, (a0)\n"
    "addi a0, a0, 128\n"
    "vse64.v v8, (a0)\n"
    "addi a0, a0, 128\n"
    "vse64.v v16, (a0)\n"
    "addi a0, a0, 128\n"
    "vse64.v v24, (a0)\n"
    :
    :
    : "a0", "t0", "memory"
  );
}

/* Restore v0-v31 from the given save area (512 bytes).
 * Clobbers a0 for address advancement.
 * Caller must have set csr_set(mstatus, MSTATUS_VS) before calling. */
void restore_vector_context(uint64_t (*vreg)[VREG_SAVE_WORDS]) {
  register uintptr_t a0 asm("a0") = (uintptr_t)vreg;
  (void)a0;
  __asm__ volatile (
    ".option arch, +v\n"
    "li t0, 16\n"
    "vsetvli zero, t0, e64, m8, ta, ma\n"
    "vle64.v v0, (a0)\n"
    "addi a0, a0, 128\n"
    "vle64.v v8, (a0)\n"
    "addi a0, a0, 128\n"
    "vle64.v v16, (a0)\n"
    "addi a0, a0, 128\n"
    "vle64.v v24, (a0)\n"
    :
    :
    : "a0", "t0", "memory"
  );
}

void clean_vector_state(struct thread_state* state) {
  int r, w;
  for (r = 0; r < 32; r++)
    for (w = 0; w < VREG_SAVE_WORDS; w++)
      state->prev_vreg[r][w] = 0;
}

void clean_state(struct thread_state* state){
  int i;
  uintptr_t* prev = (uintptr_t*) &state->prev_state;
  for(i=1; i<32; i++)
  {
    prev[i] = 0;
  }

  state->prev_mpp = -1; // 0x800;
  clean_smode_csrs(state);
  clean_vector_state(state);
}

void clean_smode_csrs(struct thread_state* state){

  state->prev_csrs.sstatus = 0;

  // We can't read these or set these from M-mode?
  state->prev_csrs.sedeleg = 0;
  state->prev_csrs.sideleg = 0;

  state->prev_csrs.sie = 0;
  state->prev_csrs.stvec = 0;
  // For now we take whatever the OS was doing
  state->prev_csrs.scounteren = csr_read(scounteren);
  state->prev_csrs.sscratch = 0;
  state->prev_csrs.sepc = 0;
  state->prev_csrs.scause = 0;
  state->prev_csrs.stval = 0;
  state->prev_csrs.sip = 0;
  state->prev_csrs.satp = 0;

}
