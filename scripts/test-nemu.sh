#!/bin/bash
# test-nemu.sh — Run enclave test in NEMU and verify expected output
# Usage: test-nemu.sh [fw_payload.bin] [timeout_seconds]

set -euo pipefail

NEMU=${NEMU:-/home/yangxin/xs-env/NEMU-2026.03.r3/build/riscv64-nemu-interpreter}
FW=${1:-/home/yangxin/xs-env/keystone/build64-xs/sm.build/platform/nemu_xiangshan/firmware/fw_payload.bin}
TIMEOUT=${2:-180}
INSTRS=${3:-5000000000}

if [ ! -f "$FW" ]; then
  echo "FAIL: firmware not found at $FW"
  echo "Usage: $0 [fw_payload.bin] [timeout_seconds] [instructions]"
  echo ""
  echo "Environment variables:"
  echo "  NEMU  — path to riscv64-nemu-interpreter (default: $NEMU)"
  exit 1
fi

echo "=== NEMU Enclave Test ==="
echo "  Firmware:  $FW"
echo "  NEMU:      $NEMU"
echo "  Timeout:   ${TIMEOUT}s"
echo "  Instr:     $INSTRS"
echo ""

OUTPUT=$(timeout $TIMEOUT $NEMU -b -I $INSTRS $FW 2>&1) || true

# Check results
PAGE_FAULT=$(echo "$OUTPUT" | grep -c "page fault" || true)
NON_HANDLABLE=$(echo "$OUTPUT" | grep -c "non-handlable" || true)
HELLO=$(echo "$OUTPUT" | grep -cE "hello, world|SUCCESS" || true)
EXIT=$(echo "$OUTPUT" | grep -c "exit_enclave" || true)

echo "=== Results ==="
echo "  hello/SUCCESS:  $( [ "$HELLO" -gt 0 ] && echo 'PASS ✓' || echo 'FAIL ✗' )"
echo "  page fault:     $( [ "$PAGE_FAULT" -eq 0 ] && echo 'PASS ✓' || echo "FAIL ✗ (count=$PAGE_FAULT)" )"
echo "  non-handlable:  $( [ "$NON_HANDLABLE" -eq 0 ] && echo 'PASS ✓' || echo "FAIL ✗ (count=$NON_HANDLABLE)" )"
echo "  enclave exit:   $( [ "$EXIT" -gt 0 ] && echo "PASS ✓" || echo "SKIP (no exit, may be expected)" )"
echo ""

# Summary
if [ "$HELLO" -gt 0 ] && [ "$PAGE_FAULT" -eq 0 ] && [ "$NON_HANDLABLE" -eq 0 ]; then
  echo "=== OVERALL: PASS ✓ ==="
  exit 0
else
  echo "=== OVERALL: FAIL ✗ ==="
  exit 1
fi
