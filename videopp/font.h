#pragma once

#include "rect.h"
#include "surface.h"
#include "font_info.h"

#include <map>
#include <memory>
#include <vector>

namespace video_ctrl
{
    class texture;
    using texture_ptr = std::shared_ptr<texture>;

    struct font : public font_info
    {
        texture_ptr texture;
    };
}

#include "font_ptr.h"
