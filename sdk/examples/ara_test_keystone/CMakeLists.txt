set(eapp_bin ara_test_keystone)
set(eapp_src
  main.c
  fconv2d/fconv2d.c
  fconv2d/fconv2d_scalar.c
  fconv2d/fconv2d_3x3.c
  fconv2d/fconv2d_7x7.c
  fconv2d/fconv2d_data.S
  fmatmul/fmatmul.c
  fmatmul/fmatmul_data.S
  fmatmul/kernel/fmatmul.c
  fmatmul/kernel/fmatmul_scalar.c
  common/serial.c
  common/printf.c
  common/string.c
  common/util.c
)
set(pkg_bin enclave.pkg)

# common files (no V extension — they use printf which may be auto-vectorized)
set(common_src
  common/serial.c
  common/printf.c
  common/string.c
  common/util.c
)
set_source_files_properties(${common_src} PROPERTIES COMPILE_FLAGS "-march=rv64gc -mabi=lp64d")
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS "-march=rv64gc -mabi=lp64d")
set_source_files_properties(fconv2d/fconv2d_scalar.c PROPERTIES COMPILE_FLAGS "-march=rv64gc -mabi=lp64d")
set_source_files_properties(fmatmul/kernel/fmatmul_scalar.c PROPERTIES COMPILE_FLAGS "-march=rv64gc -mabi=lp64d")

add_executable(${eapp_bin} ${eapp_src})
target_include_directories(${eapp_bin} PRIVATE common)
target_compile_options(${eapp_bin} PRIVATE -march=rv64gcv -mabi=lp64d -DNR_LANES=2 -DVLEN=128 -DENABLE_VEC -Wno-error -Wno-unused-variable -Wno-unused-function -Wno-builtin-macro-redefined -Wno-builtin-requires-header -DNR_LANES=2 -DVLEN=128)
target_link_libraries(${eapp_bin} "-static" "-nostartfiles" "-Wl,-e,_start" "-lm" "-march=rv64gcv" "-mabi=lp64d")

# host runner (reuse hello host)
set(host_bin ara_test_keystone-runner)
add_executable(${host_bin} ${CMAKE_CURRENT_SOURCE_DIR}/../hello/host/host.cpp)
target_include_directories(${host_bin} PRIVATE ${KEYSTONE_SDK_DIR}/include)
target_link_libraries(${host_bin} -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# Eyrie runtime
set(eyrie_files_to_copy .options_log eyrie-rt)
add_eyrie_runtime(${eapp_bin}-eyrie
  "freemem io_syscall linux_syscall env_setup"
  ${eyrie_files_to_copy})

# flat package
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
          ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
          ${CMAKE_CURRENT_BINARY_DIR}/${eapp_bin}
          ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  DEPENDS ${eapp_bin}-eyrie ${eapp_bin}
  COMMENT "Packaging enclave.pkg (flat binary)")
add_custom_target(${eapp_bin}-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin})
add_dependencies(${eapp_bin}-pkg ${eapp_bin}-eyrie ${eapp_bin})
