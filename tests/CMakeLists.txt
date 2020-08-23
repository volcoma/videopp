message(STATUS "Enabled tests.")

set(target_name videopp_tests)

file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)

if(MINGW)
	if(NOT ${BUILD_SHARED_LIBS})
		set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-static-libgcc -static-libstdc++ -static")
	else()
		get_filename_component( Mingw_Path ${CMAKE_CXX_COMPILER} PATH )

		file(GLOB MINGW_SYSTEM_RUNTIME_LIBS "${Mingw_Path}/*.dll")

		set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${MINGW_SYSTEM_RUNTIME_LIBS})
	endif()
endif()

add_executable(${target_name} ${libsrc})

target_link_libraries(${target_name} PUBLIC videopp)
target_compile_definitions(${target_name} PUBLIC DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/")

set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include(target_warning_support)
set_warning_level(${target_name} ultra)

add_custom_target(copy_system_runtime_libs ALL
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
