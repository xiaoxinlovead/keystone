#include "pmp.h"
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_types.h>
#include <sbi_utils/fdt/fdt_driver.h>

void send_and_sync_pmp_ipi(void) { }

void sbi_trap_handler_keystone_enclave(struct sbi_trap_regs *regs) { }

/* Secure boot key stubs */
unsigned char sanctum_sm_hash[64];
unsigned char sanctum_sm_signature[64];
unsigned char sanctum_sm_public_key[32];
unsigned char sanctum_sm_secret_key[32];
unsigned char sanctum_dev_public_key[32];

/* Platform override modules (v3 fdt_driver format, empty for now) */
const struct fdt_driver *const platform_override_modules[] = { NULL };
