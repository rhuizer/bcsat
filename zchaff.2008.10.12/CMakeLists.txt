cmake_minimum_required(VERSION 2.8.7)

project(zchaff)

include_directories(${PROJECT_SOURCE_DIR})

add_custom_command(OUTPUT zchaff_c_wrapper.cpp COMMAND
	sed 's/EXTERN/extern \"C\"/' ${PROJECT_SOURCE_DIR}/zchaff_wrapper.wrp > ${CMAKE_CURRENT_BINARY_DIR}/zchaff_c_wrapper.cpp)
add_custom_command(OUTPUT zchaff_cpp_wrapper.cpp COMMAND
	sed 's/EXTERN//' ${PROJECT_SOURCE_DIR}/zchaff_wrapper.wrp > ${CMAKE_CURRENT_BINARY_DIR}/zchaff_cpp_wrapper.cpp)

set(LIBSAT_SOURCES zchaff_utils.cpp zchaff_solver.cpp zchaff_base.cpp
                   zchaff_dbase.cpp zchaff_c_wrapper.cpp
                   zchaff_cpp_wrapper.cpp)

add_library(sat SHARED ${LIBSAT_SOURCES})
add_library(sat_static STATIC ${LIBSAT_SOURCES})
