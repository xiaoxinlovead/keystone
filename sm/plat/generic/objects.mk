ifdef PLATFORM
  platform-genflags-y += "-DTARGET_PLATFORM_HEADER=\"platform/$(PLATFORM)/platform.h\""
else
	PLATFORM = "generic"
  platform-genflags-y += "-DTARGET_PLATFORM_HEADER=\"platform/generic/platform.h\""
endif

platform-cflags-y += -I../src

# Keystone SM source files
platform-objs-y += ../../src/attest.o
platform-objs-y += ../../src/cpu.o
platform-objs-y += ../../src/crypto.o
platform-objs-y += ../../src/enclave.o
platform-objs-y += ../../src/pmp.o
platform-objs-y += ../../src/sm.o
platform-objs-y += ../../src/sm-sbi.o
platform-objs-y += ../../src/sm-sbi-opensbi.o
platform-objs-y += ../../src/thread.o
platform-objs-y += ../../src/mprv.o
#platform-objs-y += ../../src/sbi_trap_hack.o  # TODO: port to v3
platform-objs-y += ../../src/trap.o
#platform-objs-y += ../../src/ipi.o  # TODO: port to v3 (sbi_tlb_info API changed)

# Platform object (required for 'platform' symbol)
platform-objs-y += platform.o

# Stubs for disabled SM functions
platform-objs-y += ../../src/stubs.o

# Platform override modules - provided by stubs.c for now (pending v3 fdt_driver port)

platform-objs-y += ../../src/sha3/sha3.o
platform-objs-y += ../../src/ed25519/fe.o
platform-objs-y += ../../src/ed25519/ge.o
platform-objs-y += ../../src/ed25519/keypair.o
platform-objs-y += ../../src/ed25519/sc.o
platform-objs-y += ../../src/ed25519/sign.o

platform-objs-y += ../../src/hkdf_sha3_512/hkdf_sha3_512.o
platform-objs-y += ../../src/hmac_sha3/hmac_sha3.o

platform-objs-y += ../../src/platform/$(PLATFORM)/platform.o
platform-objs-y += ../../src/plugins/plugins.o

# Register keystone SBI extension
carray-sbi_ecall_exts-y += ecall_keystone_enclave
