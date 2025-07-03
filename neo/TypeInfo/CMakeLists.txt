SET(SOURCES
  ../framework/CVarSystem.cpp  
  ../framework/CVarSystem.h
  ../framework/CmdSystem.cpp
  ../framework/CmdSystem.h
  ../framework/File.cpp
  ../framework/File.h
  ../framework/FileSystem.cpp
  ../framework/FileSystem.h
  ../framework/Licensee.h
  ../framework/Unzip.cpp
  ../framework/Unzip.h
  main.cpp
  TypeInfoGen.cpp
  TypeInfoGen.h
)

add_definitions(-DID_TYPEINFO -DID_ENABLE_CURL=0)
add_executable(TypeInfo ${SOURCES})
target_link_libraries(TypeInfo idlib)
SET_PROPERTY(TARGET TypeInfo PROPERTY FOLDER exes)
set_cpu_arch(TypeInfo)