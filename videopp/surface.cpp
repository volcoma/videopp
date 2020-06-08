#include "surface.h"

#include "logger.h"
#include "detail/png_loader.h"

#include <3rdparty/gli/gli/gli.hpp>

namespace gfx
{
    namespace
    {
        struct file_deleter
        {
            void operator()(FILE *fp)
            {
                if (fp != nullptr)
                {
                    fclose(fp);
                }
            }
        };

        namespace dds
        {
            struct surface_format
            {
                uint32_t size {};
                uint32_t flags {};
                uint32_t height {};
                uint32_t width {};
                //uint8_t unused[108] {};
            };

            using magic_array = std::array<uint8_t, 4>;
            constexpr magic_array magic {{0x44, 0x44, 0x53, 0x20}}; // "DDS "
        }

        namespace ktx
        {
            struct surface_format
            {
                uint32_t endianness {};
                uint32_t gl_type {};
                uint32_t gl_type_size {};
                uint32_t gl_format {};
                uint32_t gl_internal_format {};
                uint32_t gl_base_internal_format {};
                uint32_t width {};
                uint32_t height {};
                //uint8_t unused[20] {};
            };

            using magic_array = std::array<uint8_t, 12>;
            constexpr magic_array magic {{0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A}};
        }

        std::tuple<rect, bool> is_png(const std::string& file_name)
        {
            std::unique_ptr<FILE, file_deleter> fp(fopen(file_name.c_str(), "rb"));
            if (!fp)
            {
                return {};
            }
            return detail::is_png(fp.get());
        }

        std::tuple<rect, bool> is_dds(const std::string& file_name)
        {
            std::unique_ptr<FILE, file_deleter> file(fopen(file_name.c_str(), "rb"));
            if (!file)
            {
                return {};
            }

            dds::magic_array read_header {{}};
            auto read_count = ::fread(read_header.data(), read_header.size() * sizeof(dds::magic_array::value_type),
                                      1, file.get());
            if (read_count < 1 || read_header != dds::magic)
            {
                return {};
            }

            dds::surface_format surface_format {};
            read_count = ::fread(&surface_format, sizeof(surface_format), 1, file.get());
            if (read_count < 1)
            {
                return {};
            }

            return {{0, 0, static_cast<int>(surface_format.width), static_cast<int>(surface_format.height)}, true};
        }

        std::tuple<rect, bool> is_ktx(const std::string& file_name)
        {
            std::unique_ptr<FILE, file_deleter> file(fopen(file_name.c_str(), "rb"));
            if (!file)
            {
                return {};
            }

            ktx::magic_array read_header {{}};
            auto read_size = ::fread(read_header.data(), read_header.size() * sizeof(ktx::magic_array::value_type),
                                     1, file.get());
            if (read_size != read_header.size() || read_header != ktx::magic)
            {
                return {};
            }

            ktx::surface_format surface_format {};
            read_size = ::fread(&surface_format, sizeof(surface_format), 1, file.get());
            if (read_size != sizeof(surface_format))
            {
                return {};
            }

            return {{0, 0, static_cast<int>(surface_format.width), static_cast<int>(surface_format.height)}, true};
        }
    }

    std::tuple<rect, bool> surface::is_surface_compatible(const std::string& file_name)
    {
        {
            auto result = is_png(file_name);
            if (std::get<1>(result))
            {
                return result;
            }
        }

        {
            auto result = is_dds(file_name);
            if (std::get<1>(result))
            {
                return result;
            }
        }

        {
            auto result = is_ktx(file_name);
            if (std::get<1>(result))
            {
                return result;
            }
        }

        return {};
    }

    surface::~surface() = default;

    surface::surface(const std::string &file_name)
    {
        if (load_png(file_name))
        {
            return;
        }
        else if (load_dds(file_name))
        {
            return;
        }

        throw gfx::exception("Cannot create surface from file " + file_name);
    }

    surface::surface(const uint8_t* file_buffer, size_t file_buffer_size)
    {
        if (load_png(file_buffer, file_buffer_size))
        {
            return;
        }
    }

    surface::surface(int width, int height, pix_type type)     
          : gli_surface_(std::make_unique<gli::texture>(gli::target::TARGET_2D, detail::gli_pixtype_to_native_format(type),
                                                        gli::texture::extent_type{width, height, 1}, 1, 1, 1))
          , rects_({{0, 0, width, height}})
          , type_(type)
          , had_alpha_pixels_originally_(type != pix_type::rgb)
    {
    }

    surface::surface(const uint8_t *buffer, int width, int height, pix_type type)
        : gli_surface_(std::make_unique<gli::texture>(gli::target::TARGET_2D, detail::gli_pixtype_to_native_format(type),
                                                      gli::texture::extent_type{width, height, 1}, 1, 1, 1))
        , rects_({{0, 0, width, height}})
        , type_(type)
        , had_alpha_pixels_originally_(type != pix_type::rgb)
    {
        ::memcpy(gli_surface_->data(), buffer, gli_surface_->size());
    }

    surface::surface(std::vector<uint8_t>&& buffer, int width, int height, pix_type type)
        : gli_surface_(std::make_unique<gli::texture>(gli::target::TARGET_2D, detail::gli_pixtype_to_native_format(type),
                                                      gli::texture::extent_type{width, height, 1}, 1, 1, 1))
        , rects_({{0, 0, width, height}})
        , type_(type)
        , had_alpha_pixels_originally_(type != pix_type::rgb)
    {
        ::memcpy(gli_surface_->data(), buffer.data(), gli_surface_->size());
    }

    surface::surface(surface&& other) noexcept = default;

    size_t surface::get_layers() const noexcept
    {
        return gli_surface_->layers();
    }

    size_t surface::get_levels() const noexcept
    {
        return gli_surface_->levels();
    }

    size_t surface::get_faces() const noexcept
    {
        return gli_surface_->faces();
    }

    bool surface::save_to_file(const std::string &file_name, size_t level, size_t layer, size_t face)
    {
        if ( gli::save(*gli_surface_, file_name) )
        {
            return true;
        }

        std::unique_ptr<FILE, file_deleter> fp(fopen(file_name.c_str(), "wb"));
        if (!fp)
        {
            return false;
        }

        return detail::save_png(fp.get(), gli_surface_, type_, level, layer, face);
    }

    int surface::get_width(size_t level) const noexcept
    {
        return gli_surface_->extent(level).x;
    }

    int surface::get_height(size_t level) const noexcept
    {
        return gli_surface_->extent(level).y;
    }

    const rect& surface::get_rect(size_t level) const noexcept
    {
        return rects_.at(level);
    }

    int surface::get_bytes_per_pixel() const noexcept
    {
        return bytes_per_pixel(type_);
    }

    pix_type surface::get_type() const noexcept
    {
        return type_;
    }

    std::pair<point, bool> surface::find_pixel(const gfx::color& color, const rect& area, size_t level) const noexcept
    {
        if (compressed_)
        {
            return {{}, false};
        }

        if (level >= rects_.size())
        {
            return {{}, false};
        }

        const auto& rect = rects_[level];
        if (!area.is_inner_of(rect))
        {
            return {{}, false};
        }

        auto pixel = reinterpret_cast<const uint8_t*>(gli_surface_->data(0, 0, level));
        auto bytes_per_pixel = get_bytes_per_pixel();
        auto pitch = bytes_per_pixel * rect.w;
        const uint8_t *search_pixel_start = pixel + area.x * bytes_per_pixel + area.y * pitch;
        auto start = area.get_pos();

        switch (type_)
        {
        case pix_type::gray:
        {
            for (; start.y < area.y + area.h; start.y++)
            {
                for (start.x = area.x; start.x < area.x + area.h; start.x++)
                {

                    if (color.r == *search_pixel_start)
                    {
                        return {start, true};
                    }

                    search_pixel_start++;
                }
            }

            break;
        }

        case pix_type::rgb:
        {
            for (; start.y < area.y + area.h; start.y++)
            {
                for (start.x = area.x; start.x < area.x + area.w; start.x++)
                {
                    if (color == gfx::color{*search_pixel_start, *(search_pixel_start+1), *(search_pixel_start+2)})
                    {
                        return {start, true};
                    }

                    search_pixel_start += bytes_per_pixel;
                }
            }

            break;
        }

        case pix_type::rgba:
        {
            const auto raw_color = reinterpret_cast<const uint32_t*>(&color);
            auto search_pixel_raw = reinterpret_cast<const uint32_t*>(search_pixel_start);

            for (; area.y < area.y + area.h; start.y++)
            {
                for (start.x = area.x; area.x < area.x + rect.w; start.x++)
                {

                    if (*raw_color == *search_pixel_raw)
                    {
                        return {start, true};
                    }

                    search_pixel_raw++;
                }
            }

            break;
        }
        }

        return {{}, false};
    }

    std::pair<point, bool> surface::find_pixel_with_alpha(const rect& area, size_t level) const noexcept
    {
        if (compressed_)
        {
            return {{}, false};
        }

        if (level >= rects_.size())
        {
            return {{}, false};
        }

        const auto& rect = rects_[level];
        if (!area.is_inner_of(rect))
        {
            return {{}, false};
        }

        switch (type_)
        {
        case pix_type::gray:
        case pix_type::rgb:
            return {{area.x, area.y}, true};

        case pix_type::rgba:
        {
            const auto& rect = rects_[level];
            auto pixel = reinterpret_cast<const uint8_t*>(gli_surface_->data(0, 0, level));
            auto bytes_per_pixel = get_bytes_per_pixel();
            auto pitch = bytes_per_pixel * rect.w;
            const uint8_t *search_pixel_start = pixel + area.x * bytes_per_pixel + area.y * pitch;
            auto start = area.get_pos();

            // skil r g and b
            auto search_pixel = search_pixel_start + sizeof(uint8_t) * 3;

            for (; start.y < area.y + area.h; start.y++)
            {
                for (; start.x < area.x + area.w; start.x++)
                {
                    if (*search_pixel != 0x00)
                    {
                        return {start, true};
                    }

                    search_pixel += bytes_per_pixel;
                }
                start.x = area.x;
                search_pixel_start += pitch;
                search_pixel = search_pixel_start + sizeof(uint8_t) * 3;
            }

            break;
        }
        }

        return {{}, false};
    }

    std::pair<point, bool> surface::find_pixel_with_alpha_reverse(const rect& area, size_t level) const noexcept
    {
        if (compressed_)
        {
            return {{}, false};
        }

        if (level >= rects_.size())
        {
            return {{}, false};
        }

        const auto& rect = rects_[level];
        if (!area.is_inner_of(rect))
        {
            return {{}, false};
        }

        switch (type_)
        {
        case pix_type::gray:
        case pix_type::rgb:
            return {{area.x, area.y + area.h - 1}, true};

        case pix_type::rgba:
        {
            gfx::point start {area.x, area.y + area.h - 1};
            const auto& rect = rects_[level];
            auto pixel = reinterpret_cast<const uint8_t*>(gli_surface_->data(0, 0, level));
            auto bytes_per_pixel = get_bytes_per_pixel();
            auto pitch = bytes_per_pixel * rect.w;
            const uint8_t *search_pixel_end = pixel + start.x * bytes_per_pixel + start.y * pitch;

            // move to alpha pixel of last pixel
            auto search_pixel = search_pixel_end + sizeof(uint8_t) * 3;

            for (; start.y > area.y; start.y--)
            {
                for (; start.x < area.x + area.w; start.x++)
                {
                    if (*search_pixel != 0x00)
                    {
                        return {start, true};
                    }

                    search_pixel += bytes_per_pixel;
                }
                start.x = area.x;
                search_pixel_end -= pitch;
                search_pixel = search_pixel_end + sizeof(uint8_t) * 3;
            }

            break;
        }
        }

        return {{}, false};
    }

    bool surface::set_pixel(const point &pos, const gfx::color& color, size_t level, size_t layer,
                            size_t face) noexcept
    {
        if (compressed_)
        {
            return false;
        }
        const auto& size = gli_surface_->extent(level);

        if (pos.x >= size.x || pos.y >= size.y || pos.x < 0 || pos.y < 0)
        {
            return false;
        }

        const auto& rect = rects_[level];
        auto data = reinterpret_cast<uint8_t*>(gli_surface_->data(layer, face, level));
        auto bytes_per_pixel = get_bytes_per_pixel();
        auto pitch = bytes_per_pixel * rect.w;
        uint8_t *pixel_start = data + pos.x * bytes_per_pixel + pos.y * pitch;
        switch (type_)
        {
            case pix_type::gray:
                *pixel_start = static_cast<uint8_t>((color.r + color.g + color.b) / 3);
                break;
            case pix_type::rgb:
                *pixel_start = color.r;
                *(pixel_start+1) = color.g;
                *(pixel_start+2) = color.b;
                break;
            case pix_type::rgba:
                *pixel_start = color.r;
                *(pixel_start+1) = color.g;
                *(pixel_start+2) = color.b;
                *(pixel_start+3) = color.a;
                break;
        }


        return true;
    }

    color surface::get_pixel(const point &point, size_t level) const noexcept
    {
        if (compressed_)
        {
            return {0,0,0,0};
        }

        auto pixel = reinterpret_cast<const uint8_t*>(gli_surface_->data(0, 0, level));
        auto bytes_per_pixel = get_bytes_per_pixel();
        auto pitch = bytes_per_pixel * rects_[level].w;
        const uint8_t *search_pixel_start = pixel + point.x * bytes_per_pixel + point.y * pitch;

        switch (type_)
        {
        case pix_type::gray:
            return {*search_pixel_start, *search_pixel_start, *search_pixel_start};
        case pix_type::rgb:
            return {*search_pixel_start, *(search_pixel_start+1), *(search_pixel_start+2)};
        case pix_type::rgba:
        {
            auto p = reinterpret_cast<const color*>(search_pixel_start);
            return *p;
        }
        }

        return {0,0,0,0};
    }

    void surface::fill(const color &color) noexcept
    {
        gli_surface_->clear(gli::u8vec4{color.r, color.g, color.b, color.a});
    }

    const uint8_t* surface::get_data(size_t level, size_t layer, size_t face) const
    {
        return reinterpret_cast<const uint8_t*>(gli_surface_->data(layer, face, level));
    }

    const std::unique_ptr<gli::texture>& surface::get_native_handle() const noexcept
    {
        return gli_surface_;
    }

    size surface::get_block_extent() const
    {
        auto extent = gli_surface_->storage().block_extent();
        return {extent.x, extent.y};
    }

    std::unique_ptr<surface> surface::create_empty(const size &new_surface_size) const
    {
        auto new_size = [&]()->size
        {
            if (new_surface_size.w != 0 && new_surface_size.h != 0)
            {
                return new_surface_size;
            }

            auto extent = this->gli_surface_->extent();
            return {extent.x, extent.y};
        }();

        auto result = std::make_unique<surface>(surface{});
        result->had_alpha_pixels_originally_ = had_alpha_pixels_originally_;
        result->type_ = type_;

        result->gli_surface_ = std::make_unique<gli::texture>(gli_surface_->target(), gli_surface_->format(),
                                                              gli::texture::extent_type{new_size.w, new_size.h, 1},
                                                              1, 1, 1, gli_surface_->swizzles());
        if (!result->gli_surface_)
        {
            return {};
        }

        result->compressed_ = gli::is_compressed(result->gli_surface_->format());

        for (size_t level = 0; level < result->get_levels(); level++)
        {
            auto extent = result->gli_surface_->extent(level);
            result->rects_.emplace_back(0, 0, extent.x, extent.y);
        }

        return result;
    }

    bool surface::copy_from(const surface &src_surf, const rect &src_rect, const point &dest_point,
                            size_t src_level, size_t src_layer, size_t src_face,
                            size_t dst_level, size_t dst_layer, size_t dst_face)
    {
        if (src_surf.rects_.size() <= src_level)
        {
            gfx::log("ERROR: level not compatible with src surface.");
            return false;
        }

        const auto& src_surf_rect = src_surf.get_rect(src_level);
        if (src_rect.x + src_rect.w > src_surf_rect.w || src_rect.y + src_rect.h > src_surf_rect.h)
        {
            gfx::log("ERROR: src_rect not in src surface.");
            return false;
        }

        if (!compressed_ && !src_surf.compressed_ && src_surf.type_ == type_)
        {
            const auto& dest_surf_rect = get_rect(dst_level);

            auto src_pixels = reinterpret_cast<const uint8_t*>(src_surf.gli_surface_->data());
            auto dest_pixels = reinterpret_cast<uint8_t*>(gli_surface_->data());

            auto src_bpp = src_surf.get_bytes_per_pixel();
            auto dest_bpp = get_bytes_per_pixel();

            auto src_rect_pitch = static_cast<uint32_t> (src_rect.w * src_bpp);
            auto src_pitch = src_surf_rect.w * src_bpp;
            auto dest_pitch = dest_surf_rect.w * dest_bpp;

            src_pixels += src_rect.x * src_bpp + src_rect.y * src_pitch;
            dest_pixels += dest_point.x * dest_bpp + dest_point.y * dest_pitch;

            for (auto i = 0; i < src_rect.h; i++)
            {
                std::memcpy(dest_pixels, src_pixels, src_rect_pitch);
                src_pixels += src_pitch;
                dest_pixels += dest_pitch;
            }

            return true;
        }

        if (rects_.size() <= dst_level)
        {
            gfx::log("ERROR: level not compatible with dest surface.");
            return false;
        }

        const auto& src_gli_surface = src_surf.gli_surface_;
        if (gli_surface_->faces() <= dst_face || src_gli_surface->faces() <= src_face)
        {
            gfx::log("ERROR: face not compatible with src or dest surface.");
            return false;
        }

        if (gli_surface_->layers() <= dst_layer || src_gli_surface->layers() <= src_layer)
        {
            gfx::log("ERROR: layer not compatible with src or dest surface.");
            return false;
        }

        gli_surface_->copy(*src_gli_surface, src_layer, src_face, src_level, {src_rect.x, src_rect.y, 0},
                           dst_layer, dst_face, dst_level, {dest_point.x, dest_point.y, 0},
                           {src_rect.w, src_rect.h, 1});
        return true;
    }

    bool surface::load_png(const std::string &file_name)
    {
        std::unique_ptr<FILE, file_deleter> fp(fopen(file_name.c_str(), "rb"));
        if (!fp)
        {
            return false;
        }

        return load_png(fp.get());
    }

    bool surface::load_png(const uint8_t* file_buffer, size_t file_buffer_size)
    {
//        std::unique_ptr<FILE, file_deleter> fp(fmemopen(const_cast<uint8_t*>(file_buffer), file_buffer_size, "rb"));
//        if (!fp)
//        {
//            return false;
//        }

//        return load_png(fp.get());
        return false;
    }

    bool surface::load_png(FILE* file)
    {
        auto result = detail::load_png(file);
        if (!result.texture)
        {
            return false;
        }

        gli_surface_ = std::move(result.texture);
        type_ = result.type;
        had_alpha_pixels_originally_ = result.had_alpha_pixels_originally;

        auto extend = gli_surface_->extent();
        rects_.emplace_back(0, 0, extend.x, extend.y);

        return true;
    }

    bool surface::load_dds(const std::string& file_name)
    {
        gli_surface_ = std::make_unique<gli::texture> (gli::load(file_name.c_str()));
        if (gli_surface_->empty())
        {
            return false;
        }

        for (size_t level = 0; level < get_levels(); level++)
        {
            auto extent = gli_surface_->extent(level);
            rects_.emplace_back(0, 0, extent.x, extent.y);
        }

        compressed_ = gli::is_compressed(gli_surface_->format());

        //TODO get it properly
        had_alpha_pixels_originally_ = true;

        return true;
    }

    bool surface::load_dds(const uint8_t* file_buffer, size_t file_buffer_size)
    {
        gli_surface_ = std::make_unique<gli::texture> (gli::load(reinterpret_cast<const char*>(file_buffer), file_buffer_size));
        if (gli_surface_->empty())
        {
            return false;
        }

        for (size_t level = 0; level < get_levels(); level++)
        {
            auto extent = gli_surface_->extent(level);
            rects_.emplace_back(0, 0, extent.x, extent.y);
        }

        compressed_ = gli::is_compressed(gli_surface_->format());

        //TODO get it properly
        had_alpha_pixels_originally_ = true;

        return true;
    }
}
