#include "html_container.h"

namespace video_ctrl
{

html_container::html_container(renderer& rend, const html_defaults& options)
    : options_(options)
    , rend_(rend)
{
}

void html_container::invalidate_draw_list()
{
    list_ = {};

}

void html_container::present()
{
    rend_.draw_cmd_list(list_);
}


litehtml::uint_ptr html_container::create_font(const litehtml::tchar_t* face_name, int size, int weight,
                                               litehtml::font_style style, unsigned int decoration,
                                               litehtml::font_metrics* fm)
{
    auto extract_font_metrics = [&](const font_face& face, litehtml::font_metrics* fm)
    {
        if(fm)
        {
            fm->ascent = static_cast<int>(face.font->ascent * face.scale);
            fm->descent = static_cast<int>(-face.font->descent * face.scale) ;
            fm->height = static_cast<int>(face.font->line_height * face.scale);
            const auto& x_glyph = face.font->get_glyph('x');
            fm->x_height = static_cast<int>((x_glyph.y1 - x_glyph.y0) * face.scale);
            fm->draw_spaces = style == litehtml::fontStyleItalic || decoration;
        }
    };


    std::string key = face_name + std::to_string(size);
    auto it = fonts_faces_.find(key);
    if(it != fonts_faces_.end())
    {
        extract_font_metrics(it->second, fm);
        return reinterpret_cast<litehtml::uint_ptr>(&it->second);
    }

    font_face opts;
    int bold_weight = 500;
    if(weight > bold_weight)
    {
        float step = (weight - bold_weight) / 100.0f;
        opts.boldness = step * 0.05f;
    }
    opts.key = key;


    try
    {
        auto font_path = options_.fonts_dir + "/" + std::string(face_name) + ".ttf";

        auto find_it = fonts_.find(font_path);
        if(find_it != std::end(fonts_))
        {
            opts.font = find_it->second;
        }
        else
        {
            glyphs_builder builder;
            builder.add(get_latin_glyph_range());
            builder.add(get_cyrillic_glyph_range());
            opts.font = rend_.create_font(create_font_from_ttf(font_path, builder.get(), 50, 2));
            fonts_[font_path] = opts.font;
        }
    }
    catch(...)
    {
        opts.font = rend_.create_font(create_default_font(50, 2));
    }

    opts.scale = size / float(opts.font->size);

    fonts_faces_[opts.key] = opts;

    extract_font_metrics(opts, fm);
    return reinterpret_cast<litehtml::uint_ptr>(&fonts_faces_[opts.key]);
}

void html_container::delete_font(litehtml::uint_ptr)
{

}

int html_container::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont)
{
    auto& face = *reinterpret_cast<font_face*>(hFont);

    video_ctrl::text t;
    t.set_font(face.font);
    t.set_utf8_text(text);
    if(face.boldness > 0.0f)
    {
        t.set_outline_width(face.boldness);
    }

    return static_cast<int>(t.get_width() * face.scale);
}

void html_container::draw_text(litehtml::uint_ptr, const litehtml::tchar_t* text, litehtml::uint_ptr hFont,
                               litehtml::web_color color, const litehtml::position& pos)
{
    auto& face = *reinterpret_cast<font_face*>(hFont);
    video_ctrl::color col = {color.red, color.green, color.blue, color.alpha};
    video_ctrl::text t;
    t.set_font(face.font);
    t.set_utf8_text(text);
    t.set_alignment(video_ctrl::text::alignment::top_left);
    t.set_color(col);
    if(face.boldness > 0.0f)
    {
        t.set_outline_color(col);
        t.set_outline_width(face.boldness);
    }
    math::transformf transform;
    transform.set_scale(face.scale, face.scale, 1.0f);
    transform.set_position(pos.x, pos.y, 0.0f);
    list_.add_text(t, transform);
}

int html_container::pt_to_px(int pt)
{
    return static_cast<int>(round(pt * 125.0 / 72.0));
}

int html_container::get_default_font_size() const
{
    return options_.default_font_size;
}

const litehtml::tchar_t* html_container::get_default_font_name() const
{
    return options_.default_font.c_str();
}

void html_container::draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker&)
{
}

void html_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* /*baseurl*/,
                                bool /*redraw_on_ready*/)
{
    auto it = textures_.find(src);
    if(it != textures_.end())
    {
        return;
    }

    textures_[src] = video_ctrl::rect(0, 0, 1024, 512);
}

void html_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* /*baseurl*/,
                                    litehtml::size& sz)
{

    auto it = textures_.find(src);
    if(it != textures_.end())
    {
        const auto& tex = it->second;
        sz.width = tex.w;
        sz.height = tex.h;
    }
}

void html_container::draw_background(litehtml::uint_ptr, const litehtml::background_paint& bg)
{
    if(bg.image.empty())
    {
        video_ctrl::color col{bg.color.red, bg.color.green, bg.color.blue, bg.color.alpha};
        video_ctrl::rect fill_rect = {bg.clip_box.x, bg.clip_box.y, bg.clip_box.width, bg.clip_box.height};
        if(fill_rect)
        {
            list_.add_rect(fill_rect, col);
        }
    }
    else
    {
        video_ctrl::rect fill_rect = {bg.clip_box.x, bg.clip_box.y, bg.image_size.width, bg.image_size.height};

        auto it = textures_.find(bg.image);
        if(it != textures_.end())
        {
            list_.add_rect(fill_rect);
        }
    }
}

void html_container::draw_borders(litehtml::uint_ptr /*hdc*/, const litehtml::borders& borders,
                                  const litehtml::position& draw_pos, bool /*root*/)
{
    if (borders.top.width != 0 && borders.top.style > litehtml::border_style_hidden)
    {
        video_ctrl::rect hollow_rect = { draw_pos.x, draw_pos.y, draw_pos.width, draw_pos.height };
        video_ctrl::color col{borders.top.color.red, borders.top.color.green, borders.top.color.blue, borders.top.color.alpha};

        if(hollow_rect)
        {
            list_.add_rect(hollow_rect, col, false, 1.0f);
        }
    }

}

void html_container::set_caption(const litehtml::tchar_t*)
{
}

void html_container::set_base_url(const litehtml::tchar_t*)
{
}

void html_container::link(const std::shared_ptr<litehtml::document>&, const litehtml::element::ptr&)
{
}

void html_container::on_anchor_click(const litehtml::tchar_t*, const litehtml::element::ptr&)
{
}

void html_container::set_cursor(const litehtml::tchar_t*)
{
}

void html_container::transform_text(litehtml::tstring&, litehtml::text_transform)
{
}

void html_container::import_css(litehtml::tstring&, const litehtml::tstring&, litehtml::tstring&)
{
}

void html_container::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius,
                              bool valid_x, bool valid_y)
{
    clip_rect_ = {pos.x, pos.y, pos.width, pos.height};
}

void html_container::del_clip()
{
    clip_rect_ = {};
}

void html_container::get_client_rect(litehtml::position& client) const
{
    const auto& r = rend_.get_rect();
    client.x = r.x;
    client.y = r.y;
    client.width = r.w;
    client.height = r.h;
}

std::shared_ptr<litehtml::element> html_container::create_element(const litehtml::tchar_t*,
                                                                  const litehtml::string_map&,
                                                                  const std::shared_ptr<litehtml::document>&)
{
    return {};
}

void html_container::get_media_features(litehtml::media_features& media) const
{
    litehtml::position client;
    get_client_rect(client);
    media.type = litehtml::media_type_screen;
    media.width = client.width;
    media.height = client.height;
    media.device_width = rend_.get_rect().w;
    media.device_height = rend_.get_rect().h;
    media.color = 8;
    media.monochrome = 0;
    media.color_index = 256;
    media.resolution = 96;
}

void html_container::get_language(litehtml::tstring& language, litehtml::tstring& culture) const
{
    language = _t("en");
    culture = _t("");
}
}
