#pragma once

#include <memory>

namespace gfx
{
struct font_info;
struct font;
using font_ptr = std::shared_ptr<font>;
using font_weak_ptr = std::weak_ptr<font>;


/// Regular weight font
inline font_ptr& font_regular() noexcept
{
    static font_ptr font;
    return font;
}

/// Bold weight font - thicker than regular
inline font_ptr& font_bold() noexcept
{
    static font_ptr font;
    return font;
}

/// Black weight font - thicker than bold
inline font_ptr& font_black() noexcept
{
    static font_ptr font;
    return font;
}

/// Monospaced font useful for digits and animations
/// as spaces between glyphs is uniform.
inline font_ptr& font_monospace() noexcept
{
    static font_ptr font;
    return font;
}

/// ascii only font, useful for debug
inline font_ptr& font_default() noexcept
{
    static font_ptr font;
    return font;
}

}
