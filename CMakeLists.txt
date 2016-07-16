cmake_minimum_required(VERSION 2.8.9)
project(bcsat)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(bcsat_parser parser.y ${CMAKE_BINARY_DIR}/parser.cc
             COMPILE_FLAGS "-b parser -p bcp_ -d")
BISON_TARGET(bcsat_parser11 parser11.y ${CMAKE_BINARY_DIR}/parser11.cc
             COMPILE_FLAGS "-b parser11 -p bcp11_ -d")
FLEX_TARGET(bcsat_lexer lexer.lex ${CMAKE_BINARY_DIR}/lexer.cc
            COMPILE_FLAGS "-L -Pbcp_")
FLEX_TARGET(bcsat_lexer11 lexer11.lex ${CMAKE_BINARY_DIR}/lexer11.cc
            COMPILE_FLAGS "-L -Pbcp11_")
ADD_FLEX_BISON_DEPENDENCY(bcsat_lexer bcsat_parser)

set(SOURCES defs.cc bc.cc gate.cc gatehash.cc handle.cc timer.cc heap.cc
            defs.hh bc.hh gate.hh gatehash.hh handle.hh timer.hh heap.hh
            hashset.hh
            ${BISON_bcsat_parser_OUTPUTS}
            ${BISON_bcsat_parser11_OUTPUTS}
            ${FLEX_bcsat_lexer_OUTPUTS}
            ${FLEX_bcsat_lexer11_OUTPUTS})

add_definitions(-std=c++11)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(bc2cnf bc2cnf.cc ${SOURCES})
add_executable(bc2edimacs bc2edimacs.cc ${SOURCES})
add_executable(edimacs2bc edimacs2bc.cc ${SOURCES})
add_executable(bc2iscas89 bc2iscas89.cc ${SOURCES})

add_subdirectory(zchaff.2008.10.12)
add_executable(bczchaff bczchaff.cc bczchaff_solve.cc ${SOURCES})
set_target_properties(bczchaff PROPERTIES
	COMPILE_DEFINITIONS "BC_HAS_ZCHAFF"
	INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR};${zchaff_SOURCE_DIR}")
target_link_libraries(bczchaff sat)

add_subdirectory(minisat-2.2.0)
add_executable(bcminisat2core bcminisat.cc bcminisat220_solve.cc ${SOURCES})
set_target_properties(bcminisat2core PROPERTIES
	COMPILE_DEFINITIONS "BC_HAS_MINISAT;MINISAT220CORE"
	INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR};${minisat_SOURCE_DIR};${minisat_SOURCE_DIR}/minisat/core")
target_link_libraries(bcminisat2core minisat-lib-shared)

add_executable(bcminisat2simp bcminisat.cc bcminisat220_solve.cc ${SOURCES})
set_target_properties(bcminisat2simp PROPERTIES
	COMPILE_DEFINITIONS "BC_HAS_MINISAT;MINISAT220SIMP"
	INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR};${minisat_SOURCE_DIR};${minisat_SOURCE_DIR}/minisat/simp")
target_link_libraries(bcminisat2simp minisat-lib-shared)
