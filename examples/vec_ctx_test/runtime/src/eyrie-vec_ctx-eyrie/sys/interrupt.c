//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "util/regs.h"
#include "call/sbi.h"
#include "sys/timex.h"
#include "sys/interrupt.h"
#include "util/printf.h"
#include <asm/csr.h>

#define DEFAULT_CLOCK_DELAY 10000

static void rt_trace(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}

void init_timer(void)
{
  sbi_set_timer(get_cycles64() + DEFAULT_CLOCK_DELAY);
  csr_set(sstatus, SR_SPIE);
  csr_set(sie, SIE_STIE | SIE_SSIE);
}

void handle_timer_interrupt()
{
  rt_trace('T');
  rt_trace('0');
  sbi_stop_enclave(0);
  /* Clobber v1 to prove SM doesn't save vector context */
  __asm__ __volatile__ (
    ".4byte 0x0d007057\n"  /* vsetvli zero,zero,e32,m1,ta,ma */
    ".4byte 0x14d00293\n"  /* li t0, 333 */
    ".4byte 0x5e02c0d7\n"  /* vmv.v.x v1,t0 */
  );
  rt_trace('C');
  rt_trace('3');
  unsigned long next_cycle = get_cycles64() + DEFAULT_CLOCK_DELAY;
  sbi_set_timer(next_cycle);
  csr_set(sstatus, SR_SPIE);
  return;
}

void handle_interrupts(struct encl_ctx* regs)
{
  unsigned long cause = regs->scause;

  switch(cause) {
    case INTERRUPT_CAUSE_TIMER:
      handle_timer_interrupt();
      break;
    case INTERRUPT_CAUSE_SOFTWARE:
    case INTERRUPT_CAUSE_EXTERNAL:
    default:
      sbi_stop_enclave(0);
      return;
  }
}
