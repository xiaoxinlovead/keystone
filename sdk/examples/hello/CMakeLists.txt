set(eapp_bin hello)
set(eapp_src eapp/hello.c)
set(host_bin hello-runner)
set(host_src host/host.cpp)
set(package_name "hello.ke")
set(package_script "./hello-runner enclave.pkg")
set(pkg_bin enclave.pkg)

if(RISCV32)
  set(eyrie_plugins "freemem io_syscall linux_syscall env_setup rv32")
else()
  set(eyrie_plugins "freemem io_syscall linux_syscall env_setup")
endif()

# eapp

add_executable(${eapp_bin} ${eapp_src})

target_link_libraries(${eapp_bin} "-static")

# host

add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# add target for Eyrie runtime (see keystone.cmake)

set(eyrie_files_to_copy .options_log eyrie-rt)
add_eyrie_runtime(${eapp_bin}-eyrie
  ${eyrie_plugins}
  ${eyrie_files_to_copy})

# flat package: combine eyrie-rt + hello into one .pkg file
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
          ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
          ${CMAKE_CURRENT_BINARY_DIR}/${eapp_bin}
          ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin}
  DEPENDS ${eapp_bin}-eyrie ${eapp_bin}
  COMMENT "Packaging enclave.pkg (flat binary)")
add_custom_target(${eapp_bin}-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${pkg_bin})
add_dependencies(${eapp_bin}-pkg ${eapp_bin}-eyrie ${eapp_bin})

# add target for packaging (see keystone.cmake)

set(pkg_files_to_copy ${pkg_bin})
add_keystone_package(${eapp_bin}-package
  ${package_name}
  ${package_script}
  ${pkg_files_to_copy} ${host_bin})

add_dependencies(${eapp_bin}-package ${eapp_bin}-pkg)

# add package to the top-level target
add_dependencies(examples ${eapp_bin}-package)
