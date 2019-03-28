set(LLVM_LINK_COMPONENTS
  BitReader
  IRReader
  Core
  Support
  Object
  Linker
  Passes
  AggressiveInstCombine
  Coroutines
  IPO
  InstCombine
  Instrumentation
  ScalarOpts
  TransformUtils
  Vectorize
  ObjCARCOpts
  Obfuscation
)
add_llvm_tool(Noctilucence
  Noctilucence.cpp
  DEPENDS
  intrinsics_gen
)
#[[find_package(PkgConfig REQUIRED)
find_package(LibArchive)
if(NOT LibArchive_FOUND)
      message(FATAL_ERROR "LibArchive is not found!")
endif()

target_include_directories(Noctilucence SYSTEM PRIVATE ${LibArchive_INCLUDE_DIRS})
target_link_libraries(Noctilucence PRIVATE ${LibArchive_LIBRARIES})]]


#[[target_include_directories(Noctilucence SYSTEM PRIVATE "/usr/local/opt/libarchive/include/")
target_include_directories(Noctilucence SYSTEM PRIVATE "/usr/local/opt/expat/include/")
target_link_libraries(Noctilucence  PRIVATE "/usr/local/opt/libarchive/lib/libarchive.a")
target_link_libraries(Noctilucence  PRIVATE "/usr/local/lib/liblzma.a")
target_link_libraries(Noctilucence  PRIVATE "/usr/lib/libiconv.dylib")
target_link_libraries(Noctilucence  PRIVATE "/usr/lib/libbz2.dylib")
target_link_libraries(Noctilucence  PRIVATE "/usr/lib/libxml2.dylib")
target_link_libraries(Noctilucence  PRIVATE "/usr/local/opt/expat/lib/libexpat.a")]]

target_link_libraries(Noctilucence  PRIVATE "/usr/lib/libxar.dylib")
