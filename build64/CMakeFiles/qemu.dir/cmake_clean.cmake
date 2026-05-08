file(REMOVE_RECURSE
  "../qemu/build/config-host.mak"
  "../qemu/build/riscv64-softmmu/qemu-system-riscv64"
  "CMakeFiles/qemu"
  "qemu-rom.patch.applied"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/qemu.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
