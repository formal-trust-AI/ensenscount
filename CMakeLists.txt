cmake_minimum_required(VERSION 3.15)
project(ensenscount LANGUAGES CXX)

#-----------------------------------------------------------------------------
# Compiler Configuration
#-----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    # Apple Silicon specific settings
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architectures")
    set(CMAKE_C_COMPILER "/usr/bin/clang")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
    
    # Homebrew paths
    set(HOMEBREW_PREFIX "/opt/homebrew" CACHE PATH "Homebrew installation prefix")
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
    
    # Suppress JSON library warnings
    add_compile_options(-Wno-c++11-compat -Wno-deprecated-declarations)
else()
    # Linux settings
    add_compile_options(-static)
endif()

#-----------------------------------------------------------------------------
# Build Configuration
#-----------------------------------------------------------------------------
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug/Release)")
set(BUILD_SHARED_LIBS OFF)

#-----------------------------------------------------------------------------
# CUDD Library Setup
#-----------------------------------------------------------------------------
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
set(CUDD_DIR ${LIB_DIR}/cudd-3.0.0)

# Extract CUDD if needed
if(NOT EXISTS ${CUDD_DIR})
    find_program(TAR_EXECUTABLE tar REQUIRED)
    execute_process(
        COMMAND ${TAR_EXECUTABLE} -xf lib.tar
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# Build CUDD if needed
if(NOT EXISTS ${CUDD_DIR}/cudd/.libs/libcudd.a)
    execute_process(
        COMMAND ./configure --enable-silent-rules --enable-obj
        WORKING_DIRECTORY ${CUDD_DIR}
        RESULT_VARIABLE config_result
    )
    if(NOT config_result EQUAL 0)
        message(FATAL_ERROR "CUDD configuration failed")
    endif()
    
    execute_process(
        COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
        WORKING_DIRECTORY ${CUDD_DIR}
        RESULT_VARIABLE build_result
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "CUDD build failed")
    endif()
endif()

#-----------------------------------------------------------------------------
# Include Directories
#-----------------------------------------------------------------------------
include_directories(
    ${CUDD_DIR}
    ${CUDD_DIR}/cudd
    ${CUDD_DIR}/epd
    ${CUDD_DIR}/mtr
    ${CUDD_DIR}/st
    ${CUDD_DIR}/cplusplus
    ${CMAKE_SOURCE_DIR}/include
    ${HOMEBREW_PREFIX}/include
)

#-----------------------------------------------------------------------------
# Source Files
#-----------------------------------------------------------------------------
set(SOURCES 
    "src/main.cpp"
    "src/solve_subps.cpp"
    "src/gen_subps.cpp"
    "src/utils.cpp"
    "src/counting_boundary.cpp"
    "src/counting_naive.cpp"
    "src/counting_wrapper.cpp"
    "src/counting_config.cpp"
    "src/pepin_counting.cpp"
    "src/sanity_check.cpp"
)

# Include headers
file(GLOB_RECURSE HEADERS 
    "src/*.hpp"
)

file(GLOB_RECURSE SOURCES_B
    "baseline/*.cpp"
    "baseline/*.hpp"
)

#-----------------------------------------------------------------------------
# Executable Target
#-----------------------------------------------------------------------------
add_executable(ensenscount ${SOURCES})
add_executable(ensenscount_b ${SOURCES_B})
target_compile_definitions(ensenscount PRIVATE TEST_SIMPLE_BDD)
target_compile_definitions(ensenscount_b PRIVATE TEST_SIMPLE_BDD)

#-----------------------------------------------------------------------------
# Link Libraries
#-----------------------------------------------------------------------------
find_package(Threads REQUIRED)

target_link_libraries(ensenscount PRIVATE
    ${CUDD_DIR}/cudd/.libs/libcudd.a
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

target_link_libraries(ensenscount_b PRIVATE
    ${CUDD_DIR}/cudd/.libs/libcudd.a
    ${CMAKE_DL_LIBS}
    Threads::Threads
)


if(APPLE)
    target_link_options(ensenscount PRIVATE
        -L${HOMEBREW_PREFIX}/lib
    )
    target_link_options(ensenscount_b PRIVATE
        -L${HOMEBREW_PREFIX}/lib
    )
endif()

#-----------------------------------------------------------------------------
# Install Configuration
#-----------------------------------------------------------------------------
install(TARGETS ensenscount DESTINATION bin)
install(TARGETS ensenscount_b DESTINATION bin)

#-----------------------------------------------------------------------------
# Testing Support
#-----------------------------------------------------------------------------
enable_testing()
add_test(NAME basic_test COMMAND ensenscount ./models/0020.json 1 1 1 1 0)

#-----------------------------------------------------------------------------
# Post-Build Steps
#-----------------------------------------------------------------------------
add_custom_command(TARGET ensenscount POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:ensenscount>
        ${CMAKE_SOURCE_DIR}
    COMMENT "Copying executable to project root"
)

add_custom_command(TARGET ensenscount_b POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:ensenscount_b>
        ${CMAKE_SOURCE_DIR}
    COMMENT "Copying executable to project root"
)

#-----------------------------------------------------------------------------
# Compiler Sanitizers (Debug only)
#-----------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ensenscount PRIVATE
        -g
        -O0
        -fsanitize=address
        -fno-omit-frame-pointer
        # Enable helpful warnings for catching dead code in debug builds
        -Wall
        -Wextra
        -Wpedantic
        -Wunused
        -Wunused-function
        -Wunused-parameter
        # -Wunused-private-field
        -Wunreachable-code
    )
    target_link_options(ensenscount PRIVATE
        -fsanitize=address
    )
    target_compile_options(ensenscount_b PRIVATE
        -g
        -O0
        -fsanitize=address
        -fno-omit-frame-pointer
        # Enable helpful warnings for catching dead code in debug builds
        -Wall
        -Wextra
        -Wpedantic
        -Wunused
        -Wunused-function
        -Wunused-parameter
        # -Wunused-private-field
        -Wunreachable-code
    )
    target_link_options(ensenscount_b PRIVATE
        -fsanitize=address
    )
endif()
