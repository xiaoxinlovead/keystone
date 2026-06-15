//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#ifndef __THREAD_H__
#define __THREAD_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_trap.h>
struct ctx
{
  uintptr_t slot;
  uintptr_t ra;
  uintptr_t sp;
  uintptr_t gp;
  uintptr_t tp;
  uintptr_t t0;
  uintptr_t t1;
  uintptr_t t2;
  uintptr_t s0;
  uintptr_t s1;
  uintptr_t a0;
  uintptr_t a1;
  uintptr_t a2;
  uintptr_t a3;
  uintptr_t a4;
  uintptr_t a5;
  uintptr_t a6;
  uintptr_t a7;
  uintptr_t s2;
  uintptr_t s3;
  uintptr_t s4;
  uintptr_t s5;
  uintptr_t s6;
  uintptr_t s7;
  uintptr_t s8;
  uintptr_t s9;
  uintptr_t s10;
  uintptr_t s11;
  uintptr_t t3;
  uintptr_t t4;
  uintptr_t t5;
  uintptr_t t6;
} __packed;

struct csrs
{
  uintptr_t sstatus;    //Supervisor status register.
  uintptr_t sedeleg;    //Supervisor exception delegation register.
  uintptr_t sideleg;    //Supervisor interrupt delegation register.
  uintptr_t sie;        //Supervisor interrupt-enable register.
  uintptr_t stvec;      //Supervisor trap handler base address.
  uintptr_t scounteren; //Supervisor counter enable

  /*  Supervisor Trap Handling */
  uintptr_t sscratch;   //Scratch register for supervisor trap handlers.
  uintptr_t sepc;       //Supervisor exception program counter.
  uintptr_t scause;     //Supervisor trap cause.
  uintptr_t stval;     //Supervisor bad address (trap value).
  uintptr_t sip;        //Supervisor interrupt pending.

  /*  Supervisor Protection and Translation */
  uintptr_t satp;     //Page-table base register.

};

/* 2 × uint64_t per register for VLEN=128 */
#define VREG_SAVE_WORDS 2

/* enclave thread state */
struct thread_state
{
  int prev_mpp;
  uintptr_t prev_mepc;
  uintptr_t prev_mstatus;
  struct csrs prev_csrs;
  struct ctx prev_state;
  uint64_t prev_vreg[32][VREG_SAVE_WORDS];
};

/* swap previous and current thread states */
void swap_prev_state(struct thread_state* state, struct sbi_trap_regs* regs, int return_on_resume);
void swap_prev_mepc(struct thread_state* state, struct sbi_trap_regs* regs, uintptr_t mepc);
void swap_prev_mstatus(struct thread_state* state, struct sbi_trap_regs* regs, uintptr_t mstatus);
void swap_prev_smode_csrs(struct thread_state* thread);

void switch_vector_enclave();
void switch_vector_host();
extern void trap_vector_enclave();
extern void trap_vector();

/* Vector context save/restore (for mstatus.VS != 0) */
void save_vector_context(uint64_t (*vreg)[VREG_SAVE_WORDS]);
void restore_vector_context(uint64_t (*vreg)[VREG_SAVE_WORDS]);

/* Host ↔ enclave vector context switch */
void switch_to_enclave_vector_context(struct thread_state* thread);
void switch_to_host_vector_context(struct thread_state* thread);

/* Clean state generation */
void clean_state(struct thread_state* state);
void clean_smode_csrs(struct thread_state* state);
#endif /* thread */
