set(eapp_bin vector)
set(eapp_src eapp/vector.c)
set(host_bin vector-runner)
set(host_src host/host.cpp)
set(pkg_bin enclave.pkg)

# eapp — must use rv64gcv to enable vector extension
add_executable(${eapp_bin} ${eapp_src})
target_compile_options(${eapp_bin} PRIVATE -march=rv64gcv -mabi=lp64d)
target_link_libraries(${eapp_bin} "-static" "-nostartfiles" "-Wl,-e,_start" "-march=rv64gcv" "-mabi=lp64d")

# host
add_executable(${host_bin} ${host_src})
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
