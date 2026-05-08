file(REMOVE_RECURSE
  "CMakeFiles/buildroot"
  "buildroot.build"
  "buildroot.build/.config"
  "overlay"
  "overlay/root"
  "overlay/root/.ssh/authorized_keys"
  "overlay/root/.ssh/id_rsa"
  "overlay/root/.ssh/id_rsa.pub"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/buildroot.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
