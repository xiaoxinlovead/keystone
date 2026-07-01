set(eapp_bin tls_test)
set(eapp_src eapp/main.c)
set(pkg_bin enclave.pkg)

add_executable(${eapp_bin} ${eapp_src})
target_link_libraries(${eapp_bin} "-static" "-nostartfiles" "-Wl,-e,_start" "-Wl,--allow-multiple-definition" "-lc" "-lgcc")

# host runner
set(host_bin tls_test-runner)
add_executable(${host_bin} host/host.cpp)
target_include_directories(${host_bin} PRIVATE ${KEYSTONE_SDK_DIR}/include)
target_link_libraries(${host_bin} -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# Eyrie runtime
set(eyrie_files_to_copy .options_log eyrie-rt)
add_eyrie_runtime(${eapp_bin}-eyrie
  "freemem env_setup io_syscall linux_syscall"
  ${eyrie_files_to_copy})

# flat package
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
          ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
          ${CMAKE_CURRENT_BINARY_DIR}/${eapp_bin}
          ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  DEPENDS ${eapp_bin}-eyrie ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt ${eapp_bin}
  COMMENT "Packaging enclave.pkg (flat binary)")
add_custom_target(${eapp_bin}-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin})
add_dependencies(${eapp_bin}-pkg ${eapp_bin}-eyrie ${eapp_bin})
add_dependencies(examples ${eapp_bin}-pkg)
