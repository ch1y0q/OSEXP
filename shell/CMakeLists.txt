project(seush)
cmake_minimum_required(VERSION 3.16)

if(MSVC)
    message(FATAL_ERROR "can be built only with GCC")
endif()

# Add the source in project root directory
aux_source_directory(. DIRSRCS)
# Add header file include directories
include_directories(./ include)
# Add block directories
#add_subdirectory(hello)
#add_subdirectory(world)
# Target
add_executable(seush ${DIRSRCS})

#find_library(readline rl-lib /usr/share)
target_link_libraries(seush)