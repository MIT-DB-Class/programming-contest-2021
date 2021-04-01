cmake_minimum_required (VERSION 3.10)
project (DBProgrammingCompetition)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_C_COMPILER /usr/local/bin/gcc)
# set(CMAKE_CXX_COMPILER /usr/local/bin/g++)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -fsanitize=address -Wall")

include_directories(${PROJECT_SOURCE_DIR}/src/include)

# Get the list of all sources.
file(GLOB_RECURSE
	    PROJECT_SRCS
	    CONFIGURE_DEPENDS
	    ${PROJECT_SOURCE_DIR}/src/*.cpp
	    ${PROJECT_SOURCE_DIR}/src/include/*.h
	    )

# Remove non-library files
list(REMOVE_ITEM PROJECT_SRCS ${PROJECT_SOURCE_DIR}/src/main/main.cpp)
list(REMOVE_ITEM PROJECT_SRCS ${PROJECT_SOURCE_DIR}/src/main/harness.cpp)
list(REMOVE_ITEM PROJECT_SRCS ${PROJECT_SOURCE_DIR}/src/main/query2SQL.cpp)

add_library(database ${PROJECT_SRCS})
target_include_directories(database PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

OPTION(FORCE_TESTS "Build tests, regardless of build type." ON)
if (CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]" OR FORCE_TESTS)
    add_subdirectory(test)
endif()

add_executable(driver src/main/main.cpp)
target_link_libraries(driver database)

# Interactive command line tool to translate our query format to SQL queries
add_executable(query2SQL src/main/query2SQL.cpp)
target_link_libraries(query2SQL database)

# Test harness
add_executable(harness src/main/harness.cpp)

ADD_CUSTOM_TARGET(link_target ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/workloads
  ${CMAKE_CURRENT_BINARY_DIR}/workloads)
