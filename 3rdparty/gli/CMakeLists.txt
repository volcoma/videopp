cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)

#project(gli)
set(GLI_VERSION "0.8.3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

enable_testing()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(".")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# To generate .asm files: /FAs
	add_definitions(/Za)
endif()

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel") AND UNIX))
	add_definitions(-Wall)
endif()

################################
# Add source files

file(GLOB ROOT_SOURCE ./gli/*.cpp)
file(GLOB ROOT_INLINE ./gli/*.inl)
file(GLOB ROOT_HEADER ./gli/*.hpp)
file(GLOB ROOT_TEXT ./*.txt)
file(GLOB ROOT_MD ../*.md)
file(GLOB ROOT_KMG ./doc/spec/*.html)

file(GLOB_RECURSE CORE_SOURCE ./gli/core/*.cpp)
file(GLOB_RECURSE CORE_INLINE ./gli/core/*.inl)
file(GLOB_RECURSE CORE_HEADER ./gli/core/*.hpp)

file(GLOB_RECURSE GTX_SOURCE ./gli/gtx/*.cpp)
file(GLOB_RECURSE GTX_INLINE ./gli/gtx/*.inl)
file(GLOB_RECURSE GTX_HEADER ./gli/gtx/*.hpp)

source_group("KMG Spec" FILES  ${ROOT_KMG})
source_group("Text Files" FILES ${ROOT_TEXT} ${ROOT_MD})
source_group("Core Files" FILES ${CORE_SOURCE})
source_group("Core Files" FILES ${CORE_INLINE})
source_group("Core Files" FILES ${CORE_HEADER})
source_group("GTX Files" FILES ${GTX_SOURCE})
source_group("GTX Files" FILES ${GTX_INLINE})
source_group("GTX Files" FILES ${GTX_HEADER})

################################
# Add package

macro(addExternalPackageGTC NAME DIRECTORY)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/external/${DIRECTORY}/util)
	set(${NAME}_PACKAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/${DIRECTORY})

	find_package(${NAME})
	if(NOT ${NAME}_FOUND)
		message("ERROR: ${NAME} not found")
	endif(NOT ${NAME}_FOUND)
endmacro(addExternalPackageGTC)

add_library(gli INTERFACE)
target_include_directories(gli INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(gli INTERFACE glm)
