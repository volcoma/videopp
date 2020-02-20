#pragma once

#include <memory>

namespace gfx
{
struct font_info;
struct font;
using font_ptr = std::shared_ptr<font>;
using font_weak_ptr = std::weak_ptr<font>;

}
