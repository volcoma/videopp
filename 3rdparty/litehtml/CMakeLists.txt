add_subdirectory(src/gumbo)

set(SOURCE_LITEHTML
    src/background.cpp
    src/box.cpp
    src/context.cpp
    src/css_length.cpp
    src/css_selector.cpp
    src/document.cpp
    src/el_anchor.cpp
    src/el_base.cpp
    src/el_before_after.cpp
    src/el_body.cpp
    src/el_break.cpp
    src/el_cdata.cpp
    src/el_comment.cpp
    src/el_div.cpp
    src/element.cpp
    src/el_font.cpp
    src/el_image.cpp
    src/el_link.cpp
    src/el_para.cpp
    src/el_script.cpp
    src/el_space.cpp
    src/el_style.cpp
    src/el_table.cpp
    src/el_td.cpp
    src/el_text.cpp
    src/el_title.cpp
    src/el_tr.cpp
    src/html.cpp
    src/html_tag.cpp
    src/iterators.cpp
    src/media_query.cpp
    src/style.cpp
    src/stylesheet.cpp
    src/table.cpp
    src/utf8_strings.cpp
    src/web_color.cpp
)

add_library(litehtml ${SOURCE_LITEHTML})
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

# Gumbo
target_link_libraries(litehtml PUBLIC gumbo)
