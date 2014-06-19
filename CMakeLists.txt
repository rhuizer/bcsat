cmake_minimum_required(VERSION 2.8)
project(bcsat)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(bcsat_parser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
             COMPILE_FLAGS "-b parser -p bcp_ -d")
FLEX_TARGET(bcsat_lexer lexer.lex ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
            COMPILE_FLAGS "-L -Pbcp_")
ADD_FLEX_BISON_DEPENDENCY(bcsat_lexer bcsat_parser)

set(SOURCES defs.cc bc.cc gate.cc gatehash.cc handle.cc timer.cc heap.cc
            ${BISON_bcsat_parser_OUTPUTS}
            ${FLEX_bcsat_lexer_OUTPUTS})
add_executable(bc2cnf bc2cnf.cc ${SOURCES})
add_executable(bc2edimacs bc2edimacs.cc ${SOURCES})
add_executable(edimacs2bc edimacs2bc.cc ${SOURCES})
add_executable(bc2iscas89 bc2iscas89.cc ${SOURCES})
