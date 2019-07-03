set(SOURCE_GUMBO
    attribute.c
    char_ref.c
    error.c
    parser.c
    string_buffer.c
    string_piece.c
    tag.c
    tokenizer.c
    utf8.c
    util.c
    vector.c
)

add_library(gumbo STATIC ${SOURCE_GUMBO})

set_target_properties(gumbo PROPERTIES
    C_STANDARD 99
	C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
	POSITION_INDEPENDENT_CODE ON
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if(MSVC)
    target_include_directories(gumbo PRIVATE visualc/include)
endif()

# Export gumbo includes.
target_include_directories(gumbo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
