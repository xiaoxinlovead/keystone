set(eapp_bin llama_keystone)
set(eapp_src_c
  eapp/main.c
  common/serial.c
  common/printf.c
  common/string.c
)
set(eapp_src_model /tmp/model.o)
set(pkg_bin enclave.pkg)

add_executable(${eapp_bin} ${eapp_src_c} ${eapp_src_model})

target_include_directories(${eapp_bin} PRIVATE
  common
  ${CMAKE_CURRENT_SOURCE_DIR}/../llama.cpp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../llama.cpp/ggml/include
)
target_compile_options(${eapp_bin} PRIVATE
  -Wno-format -Wno-format-security -Wno-format-extra-args)

target_link_libraries(${eapp_bin}
  "/tmp/llama-build2/src/libllama.a"
  "/tmp/llama-build2/ggml/src/libggml.a"
  "/tmp/llama-build2/ggml/src/libggml-cpu.a"
  "/tmp/llama-build2/ggml/src/libggml-base.a"
  "-static" "-nostartfiles" "-Wl,-e,_start"
  "-Wl,--allow-multiple-definition"
  "-lm" "-lstdc++" "-lc" "-lgcc"
)

# host runner
set(host_bin llama_keystone-runner)
add_executable(${host_bin} host/host.cpp)
target_include_directories(${host_bin} PRIVATE ${KEYSTONE_SDK_DIR}/include)
target_link_libraries(${host_bin} -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# Eyrie runtime with full libc support
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

# Register the model object as a dependency so rebuilds happen
set_source_files_properties(/tmp/model.o PROPERTIES GENERATED FALSE)
set_source_files_properties(/tmp/model.o PROPERTIES EXTERNAL_OBJECT TRUE)
