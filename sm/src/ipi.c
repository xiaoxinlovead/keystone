#include <sbi/sbi_fifo.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_hartmask.h>
#include "ipi.h"
#include "pmp.h"

void sbi_pmp_ipi_local_update(struct sbi_tlb_info *__info)
{
  struct sbi_pmp_ipi_info* info = (struct sbi_pmp_ipi_info *) __info;
  if (info->type == SBI_PMP_IPI_TYPE_SET) {
    pmp_set_keystone(info->rid, (uint8_t) info->perm);
  } else {
    pmp_unset(info->rid);
  }
}

void send_and_sync_pmp_ipi(int region_idx, int type, uint8_t perm)
{
  struct sbi_hartmask mask;
  ulong source_hart = current_hartid();
  struct sbi_tlb_info tlb_info;

  SBI_HARTMASK_INIT(&mask);
  sbi_hsm_hart_interruptible_mask(sbi_domain_thishart_ptr(), &mask);

  SBI_TLB_INFO_INIT(&tlb_info, type, 0, region_idx, perm,
      sbi_pmp_ipi_local_update, source_hart);
  sbi_tlb_request(sbi_hartmask_bits(&mask)[0], 0, &tlb_info);
}

