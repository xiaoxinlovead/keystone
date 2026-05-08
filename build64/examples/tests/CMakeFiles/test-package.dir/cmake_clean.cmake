file(REMOVE_RECURSE
  ".options_log"
  "CMakeFiles/test-package"
  "eyrie-rt"
  "pkg"
  "pkg/.options_log"
  "pkg/attestation"
  "pkg/data-sealing"
  "pkg/eyrie-rt"
  "pkg/fib-bench"
  "pkg/fibonacci"
  "pkg/long-nop"
  "pkg/loop"
  "pkg/malloc"
  "pkg/run-test.sh"
  "pkg/stack"
  "pkg/test-runner"
  "pkg/untrusted"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/test-package.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
