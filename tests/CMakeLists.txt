message(STATUS "Enabled tests.")

set(target_name videopp_tests)

file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

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

