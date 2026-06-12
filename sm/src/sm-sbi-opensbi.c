#include <sbi/sbi_trap.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_tlb.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_string.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_scratch.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_ecall.h>
#include "sm-sbi-opensbi.h"
#include "pmp.h"
#include "sm-sbi.h"
#include "sm.h"
#include "cpu.h"

static int sbi_ecall_keystone_enclave_handler(unsigned long extid, unsigned long funcid,
                     struct sbi_trap_regs *regs,
                     struct sbi_ecall_return *out)
{
  uintptr_t retval;

  if (funcid <= FID_RANGE_DEPRECATED) { return SBI_ERR_SM_DEPRECATED; }
  else if (funcid <= FID_RANGE_HOST)
  {
    if (cpu_is_enclave_context())
      return SBI_ERR_SM_ENCLAVE_SBI_PROHIBITED;
  }
  else if (funcid <= FID_RANGE_ENCLAVE)
  {
    if (!cpu_is_enclave_context())
      return SBI_ERR_SM_ENCLAVE_SBI_PROHIBITED;
  }

  switch (funcid) {
    case SBI_SM_CREATE_ENCLAVE:
      retval = sbi_sm_create_enclave(&out->value, regs->a0);
      break;
    case SBI_SM_DESTROY_ENCLAVE:
      retval = sbi_sm_destroy_enclave(regs->a0);
      break;
    case SBI_SM_RUN_ENCLAVE:
      retval = sbi_sm_run_enclave(regs, regs->a0, out);
      break;
    case SBI_SM_RESUME_ENCLAVE:
      retval = sbi_sm_resume_enclave(regs, regs->a0, out);
      break;
    case SBI_SM_RANDOM:
      out->value = sbi_sm_random();
      retval = 0;
      break;
    case SBI_SM_ATTEST_ENCLAVE:
      retval = sbi_sm_attest_enclave(regs->a0, regs->a1, regs->a2);
      break;
    case SBI_SM_GET_SEALING_KEY:
      retval = sbi_sm_get_sealing_key(regs->a0, regs->a1, regs->a2);
      break;
    case SBI_SM_STOP_ENCLAVE:
      retval = sbi_sm_stop_enclave(regs, regs->a0, out);
      break;
    case SBI_SM_EXIT_ENCLAVE:
      retval = sbi_sm_exit_enclave(regs, regs->a0, out);
      break;
    case SBI_SM_CALL_PLUGIN:
      retval = sbi_sm_call_plugin(regs->a0, regs->a1, regs->a2, regs->a3);
      break;
    default:
      retval = SBI_ERR_SM_NOT_IMPLEMENTED;
      break;
  }

  return retval;

}

static int keystone_register_extensions(void)
{
  int rc = sbi_ecall_register_extension(&ecall_keystone_enclave);
  sbi_printf("[SM] register_extensions: rc=%d\n", rc);
  return rc;
}

struct sbi_ecall_extension ecall_keystone_enclave = {
  .head = { .next = NULL, .prev = NULL },
  .name = "keystone",
  .extid_start = SBI_EXT_EXPERIMENTAL_KEYSTONE_ENCLAVE,
  .extid_end = SBI_EXT_EXPERIMENTAL_KEYSTONE_ENCLAVE,
  .register_extensions = keystone_register_extensions,
  .handle = sbi_ecall_keystone_enclave_handler,
};
