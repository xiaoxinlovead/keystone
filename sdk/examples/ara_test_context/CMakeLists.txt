# ara_test_context: 3 enclaves with interleaved partial computation

set(eapp_src
  eapp/eapp.c
  common/serial.c
  common/printf.c
  common/string.c
  common/util.c
  fconv2d/fconv2d.c
  fconv2d/fconv2d_scalar.c
  fconv2d/fconv2d_3x3.c
  fconv2d/fconv2d_7x7.c
  fconv2d/fconv2d_data.S
  fmatmul/fmatmul.c
  fmatmul/fmatmul_data.S
  fmatmul/kernel/fmatmul.c
  fmatmul/kernel/fmatmul_scalar.c
)

set(EFlags -march=rv64gcv -mabi=lp64d -DNR_LANES=2 -DVLEN=128 -DENABLE_VEC -Wno-error -Wno-unused-variable -Wno-unused-function -Wno-builtin-macro-redefined -Wno-builtin-requires-header)
set(ELink "-static" "-nostartfiles" "-Wl,-e,_start" "-lm" "-march=rv64gcv" "-mabi=lp64d")

# 1. fconv2d enclave (TASK=0)
add_executable(ara_test_fc ${eapp_src})
target_include_directories(ara_test_fc PRIVATE include common)
target_compile_options(ara_test_fc PRIVATE ${EFlags} -DTASK=0)
target_link_libraries(ara_test_fc ${ELink})

# 2. fmatmul enclave (TASK=1)
add_executable(ara_test_mm ${eapp_src})
target_include_directories(ara_test_mm PRIVATE include common)
target_compile_options(ara_test_mm PRIVATE ${EFlags} -DTASK=1)
target_link_libraries(ara_test_mm ${ELink})

# 3. both (baseline) enclave (TASK=2)
add_executable(ara_test_both ${eapp_src})
target_include_directories(ara_test_both PRIVATE include common)
target_compile_options(ara_test_both PRIVATE ${EFlags} -DTASK=2)
target_link_libraries(ara_test_both ${ELink})

# 4. check enclave (TASK=3) — currently skipped (no UTM)
add_executable(ara_test_check ${eapp_src})
target_include_directories(ara_test_check PRIVATE include common)
target_compile_options(ara_test_check PRIVATE ${EFlags} -DTASK=3)
target_link_libraries(ara_test_check ${ELink})

# Shared eyrie runtime
set(eyrie_files_to_copy .options_log eyrie-rt)
add_eyrie_runtime(ara_test_context-eyrie
  "freemem io_syscall linux_syscall env_setup"
  ${eyrie_files_to_copy})

macro(add_pkg name eapp_target)
  set(pkg ${name}.pkg)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pkg}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/pkg-flat.py
            ${CMAKE_CURRENT_BINARY_DIR}/eyrie-rt
            ${CMAKE_CURRENT_BINARY_DIR}/${eapp_target}
            ${CMAKE_CURRENT_BINARY_DIR}/${pkg}
    DEPENDS ${eapp_target} ara_test_context-eyrie
    COMMENT "Packaging ${pkg}")
  add_custom_target(${name}-pkg ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${pkg})
  add_dependencies(${name}-pkg ${eapp_target} ara_test_context-eyrie)
endmacro()

add_pkg(ara_test_fc ara_test_fc)
add_pkg(ara_test_mm ara_test_mm)
add_pkg(ara_test_both ara_test_both)
add_pkg(ara_test_check ara_test_check)

# Host runner (compile only, not used in this version — manual task through S90keystone)
add_executable(ara_test_context-runner host/host.cpp)
target_include_directories(ara_test_context-runner PRIVATE ${KEYSTONE_SDK_DIR}/include include)
target_compile_options(ara_test_context-runner PRIVATE -march=rv64gc -mabi=lp64d)
target_link_libraries(ara_test_context-runner -static ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})
add_dependencies(ara_test_context-runner ara_test_both-pkg)
