# vec_ctx_test: multi-enclave vector context switch test
#   Enclave A: writes vector regs, yields, checks on resume
#   Enclave B: trashes all vector regs to zero
#   Host: A → stop → B → resume A → compare

set(VEC_FLAGS "-march=rv64gcv" "-mabi=lp64d" "-Wno-unused-variable" "-Wno-unused-function")

# --- Enclave A: vector writer/checker ---
add_executable(vec_writer eapp_vec_writer.c)
target_compile_options(vec_writer PRIVATE ${VEC_FLAGS})
target_link_libraries(vec_writer "-static" "-nostartfiles" "-Wl,-e,_start")

# --- Enclave B: vector trash ---
add_executable(vec_trash eapp_vec_trash.c)
target_compile_options(vec_trash PRIVATE ${VEC_FLAGS})
target_link_libraries(vec_trash "-static" "-nostartfiles" "-Wl,-e,_start")

# --- Host runner ---
add_executable(vec_ctx_test-runner host/host.cpp)
target_link_libraries(vec_ctx_test-runner -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# --- Eyrie runtime (shared) ---
set(eyrie_files_to_copy .options_log eyrie-rt)
add_eyrie_runtime(vec_ctx-eyrie
  "freemem io_syscall linux_syscall env_setup"
  ${eyrie_files_to_copy})

# --- Package enclave A ---
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vec_writer.pkg
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
          ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
          ${CMAKE_CURRENT_BINARY_DIR}/vec_writer
          ${CMAKE_CURRENT_BINARY_DIR}/vec_writer.pkg
  DEPENDS vec_writer vec_ctx-eyrie
  COMMENT "Packaging vec_writer.pkg")
add_custom_target(vec_writer-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vec_writer.pkg)
add_dependencies(vec_writer-pkg vec_writer vec_ctx-eyrie)

# --- Package enclave B ---
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vec_trash.pkg
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
          ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
          ${CMAKE_CURRENT_BINARY_DIR}/vec_trash
          ${CMAKE_CURRENT_BINARY_DIR}/vec_trash.pkg
  DEPENDS vec_trash vec_ctx-eyrie
  COMMENT "Packaging vec_trash.pkg")
add_custom_target(vec_trash-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vec_trash.pkg)
add_dependencies(vec_trash-pkg vec_trash vec_ctx-eyrie)

# --- Test runner depends on both packages ---
add_custom_target(vec_ctx_test ALL
  DEPENDS vec_ctx_test-runner vec_writer-pkg vec_trash-pkg)

add_dependencies(examples vec_ctx_test)
