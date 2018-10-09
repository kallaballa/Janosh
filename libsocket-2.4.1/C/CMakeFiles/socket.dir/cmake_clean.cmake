file(REMOVE_RECURSE
  "libsocket.pdb"
  "libsocket.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/socket.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
