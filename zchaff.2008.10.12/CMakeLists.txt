cmake_minimum_required(VERSION 2.8.9)

project(zchaff)

add_custom_command(OUTPUT zchaff_c_wrapper.cpp COMMAND
	sed 's/EXTERN/extern \"C\"/' zchaff_wrapper.wrp > zchaff_c_wrapper.cpp)
add_custom_command(OUTPUT zchaff_cpp_wrapper.cpp COMMAND
	sed 's/EXTERN//' zchaff_wrapper.wrp > zchaff_cpp_wrapper.cpp)

set(LIBSAT_SOURCES zchaff_utils.cpp zchaff_solver.cpp zchaff_base.cpp
                   zchaff_dbase.cpp zchaff_c_wrapper.cpp
                   zchaff_cpp_wrapper.cpp)

add_library(sat SHARED ${LIBSAT_SOURCES})
add_library(sat_static STATIC ${LIBSAT_SOURCES})
