file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_library(litehtml ${libsrc})
target_compile_definitions(litehtml PUBLIC LITEHTML_UTF8)

set_target_properties(litehtml PROPERTIES
    CXX_STANDARD 11
	CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    C_STANDARD 99
	POSITION_INDEPENDENT_CODE ON
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# Export litehtml includes.
target_include_directories(litehtml PUBLIC src)
target_include_directories(litehtml PUBLIC include)

if(MSVC)
	target_include_directories(litehtml PRIVATE src/gumbo/visualc/include)
endif()
