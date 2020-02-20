#include <ospp/os.h>
#include <videopp/html/html_page.h>
#include <videopp/renderer.h>
#include <videopp/ttf_font.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <regex>

static std::string EN =
R"(style1(FREE SPINS)
style2(3) img(scatter) symbols anywhere on the style2(2nd), style2(3rd) and style2(4th) reels only trigger style2(10 FREE SPINS) + style2(MOVING SYMBOLS).
During style2(FREE SPINS), if symbol appear on the entire style2(1st) reel and on any position on the style2(3rd), style2(4th) or style2(5th) reel, the positions on the row between them will also be filled with that symbol.
In case of retriggering style2(FREE SPINS), the player wins style2(10) new style2(FREE SPINS) which are added to the current number of style2(FREE SPINS).
The winnings from img(scatter) symbols and new style3(FREE SPINS) are won before the expanding of the moving symbols. The style2(FREE SPINS) are played at trigger bet and lines. During style2(FREE SPINS) an alternate set of reels is used.

style1(WILD)
img(wild) subtitutes for all symbols except img(scatter).)";

static std::string BG =
R"(style1(БЕЗПЛАТНИ СПИНОВЕ)
style2(3) img(scatter) символи навсякъде на style2(2ра), style2(3та) и style2(4та) ролка задействат само style2(10 БЕЗПЛАТНИ СПИНОВЕ) + style2(ДВИЖЕЩИ СИМВОЛИ).
По време на style2(БЕЗПЛАТНИ СПИНОВЕ), ако символът се появи на цялата style2(1ва) ролка и на всяка позиция на style2(3та), style2(4та) или style2(5та) ролка, позициите на реда между тях също ще бъдат запълнени с този символ.
В случай на повторно задействане на style2(БЕЗПЛАТНИ СПИНОВЕ), играчът печели style2(10) нови style2(БЕЗПЛАТНИ СПИНОВЕ), които се добавят към текущия брой style2(БЕЗПЛАТНИ СПИНОВЕ).
Печалбите от img(scatter) символи и нови style3(БЕЗПЛАТНИ СПИНОВЕ) се печелят преди разширяването на движещите се символи. style2(БЕЗПЛАТНИ СПИНОВЕ) се играят при залагане на тригер и линии. По време на style2(БЕЗПЛАТНИ СПИНОВЕ) се използва алтернативен набор от макари.

style1(ДИВ)
img(wild) замества за всички символи, с изключение на img(scatter).)";

static std::string ESP =
R"(style1(GIRAS GRATIS)
Los símbolos style2(3) img(scatter) en cualquier lugar de los carretes style2(2nd), style2(3rd) y style2(4th) solo activan style2(10 GIRAS GRATIS) + style2(SÍMBOLOS EN MOVIMIENTO).
Durante style2(GIRAS GRATIS), si el símbolo aparece en todo el carrete style2(1st) y en cualquier posición en el carrete style2(3rd), style2(4th) o style2(5th), las posiciones en la fila entre ellos también se llenarán con ese símbolo.
En caso de reactivar style2(GIRAS GRATIS), el jugador gana style2(10) nuevos style2(GIRAS GRATIS) que se agregan al número actual de style2(FREE SPINS).
Las ganancias de los símbolos img(scatter) y los nuevos style3(GIRAS GRATIS) se ganan antes de la expansión de los símbolos móviles. Los style2(GIRAS GRATIS) se juegan en la apuesta de activación y en las líneas. Durante style2(GIRAS GRATIS) se usa un conjunto alternativo de carretes.

style1(SALVAJE)
img(wild) sustituye a todos los símbolos excepto img(scatter).)";


static std::vector<std::string> texts{EN, BG, ESP};

static gfx::font_ptr wierd_font;

struct line_element
{
    size_t line{};
    float x_offset{};
    float y_offset{};
};

struct image_data
{
    gfx::rect src_rect{};
    gfx::texture_weak_ptr image{};
};

struct embedded_image
{
    line_element element{};
    image_data data{};
};

struct embedded_text
{
    line_element element{};
    gfx::text text{};
};


struct rich_config
{
    using image_getter_t = std::function<image_data(const std::string&)>;

    image_getter_t image_getter;
    std::map<std::string, gfx::text_style> styles{};

    float line_height_scale = 2.0f;
};

struct rich_text : gfx::text
{
    void draw(gfx::draw_list& list, math::transformf transform, gfx::rect dst_rect, gfx::size_fit sz_fit = gfx::size_fit::shrink_to_fit,
              gfx::dimension_fit dim_fit = gfx::dimension_fit::uniform)
    {
        const auto& style = get_style();
        auto advance = (calculated_line_height_ - style.font->line_height);
        transform.translate(0.0f, advance * 0.25f, 0.0f);
        dst_rect.h -= int(advance * 0.25f);

        auto max_w = dst_rect.w;

        math::transformf world;
        size_t loop{0};
		while(loop < 16)
        {
            set_max_width(max_w);
            world = gfx::align_and_fit_item(get_alignment(), get_width(), get_height(), transform, dst_rect, sz_fit, dim_fit);
            auto w = int(float(dst_rect.w) / world.get_scale().x);

            if(w == max_w)
            {
                break;
            }

            max_w = w;
            loop++;
        }

        list.add_text(*this, world);

        for(const auto& kvp : embedded_images_)
        {
            const auto& embedded = kvp.second;
            const auto& element = embedded.element;
            auto image = embedded.data.image.lock();

            const auto& img_src_rect = embedded.data.src_rect;
            gfx::rect img_dst_rect = {0, 0, img_src_rect.w, img_src_rect.h};
            img_dst_rect = apply_constraints(img_dst_rect);

            img_dst_rect.x += int(element.x_offset);
            img_dst_rect.y += int(element.y_offset);
            img_dst_rect.y -= img_dst_rect.h / 2;

            list.add_image(image, img_src_rect, img_dst_rect, world);

        }

        for(const auto& kvp : embedded_texts_)
        {
            const auto& embedded = kvp.second;
            const auto& element = embedded.element;
            const auto& text = embedded.text;

			math::transformf tr;
			tr.set_position(element.x_offset, element.y_offset, 0);
			list.add_text(text, world * tr);
		}
    }

    void set_config(const rich_config& cfg)
    {
        cfg_ = cfg;
        embedded_images_.clear();
        embedded_texts_.clear();

        const auto& style = get_style();
        calculated_line_height_ = style.font->line_height * cfg_.line_height_scale;
        auto advance = (calculated_line_height_ - style.font->line_height);

        set_advance({0, advance});

        set_align_line_callback([&](size_t line, float align_x)
        {
            for(auto& kvp : embedded_images_)
            {
                auto& element = kvp.second.element;
                if(element.line == line)
                {
                    element.x_offset += align_x;
                }
            }

            for(auto& kvp : embedded_texts_)
            {
                auto& element = kvp.second.element;
                if(element.line == line)
                {
                    element.x_offset += align_x;
                }
            }
        });

        set_clear_geometry_callback([&]()
        {
            embedded_images_.clear();
			embedded_texts_.clear();
        });

        {
			auto decorators = add_decorators("img");

			for(const auto& decorator : decorators)
            {
				decorator->get_size_on_line = [&](const gfx::text_decorator& decorator, const char* str_begin, const char* str_end) -> float
				{
                    key_t key{decorator.visual_range.begin, decorator.visual_range.end};

                    auto it = embedded_images_.find(key);
                    if(it == std::end(embedded_images_))
                    {
                        if(!cfg_.image_getter)
                        {
                            return 0.0f;
                        }

                        auto& embedded = embedded_images_[key];

                        std::string utf8_str(str_begin, str_end);
                        embedded.data = cfg_.image_getter(utf8_str);

                        auto texture = embedded.data.image.lock();
                        auto src_rect = embedded.data.src_rect;
                        gfx::rect dst_rect = {0, 0, src_rect.w, src_rect.h};
                        return float(apply_constraints(dst_rect).w);
                    }

                    auto& embedded = it->second;
                    auto image = embedded.data.image;
                    auto src_rect = embedded.data.src_rect;
                    gfx::rect dst_rect = {0, 0, src_rect.w, src_rect.h};

                    return float(apply_constraints(dst_rect).w);
                };

				decorator->set_position_on_line = [&](const gfx::text_decorator& decorator,
                        float line_offset_x,
                        size_t line,
                        const gfx::line_metrics& metrics,
                        const char* /*str_begin*/, const char* /*str_end*/)
				{
                    key_t key{decorator.visual_range.begin, decorator.visual_range.end};

                    auto& embedded = embedded_images_[key];
                    auto& element = embedded.element;
					element.line = line;
					element.x_offset = line_offset_x;
                    element.y_offset = metrics.median;
				};

            }
        }

        for(const auto& kvp : cfg_.styles)
		{
            const auto& style_id = kvp.first;
            const auto& style = kvp.second;
			auto decorators = add_decorators(style_id);

			for(const auto& decorator : decorators)
			{
                decorator->get_size_on_line = [&](const gfx::text_decorator& decorator, const char* str_begin, const char* str_end) ->float
				{
                    key_t key{decorator.visual_range.begin, decorator.visual_range.end};

                    auto it = embedded_texts_.find(key);
                    if(it == std::end(embedded_texts_))
                    {
                        auto& embedded = embedded_texts_[key];
                        embedded.text.set_style(style);
                        embedded.text.set_alignment(gfx::align::baseline | gfx::align::left);
                        embedded.text.set_utf8_text({str_begin, str_end});
                        return embedded.text.get_width();
                    }

                    auto& embedded = it->second;
                    auto& text = embedded.text;
					return text.get_width();
				};

				decorator->set_position_on_line = [&](const gfx::text_decorator& decorator,
                        float line_offset_x,
                        size_t line,
                        const gfx::line_metrics& metrics,
                        const char* /*str_begin*/, const char* /*str_end*/)
				{
                    key_t key{decorator.visual_range.begin, decorator.visual_range.end};

                    auto& embedded = embedded_texts_[key];
                    auto& element = embedded.element;
					element.line = line;
					element.x_offset = line_offset_x;
					element.y_offset = metrics.baseline;
				};

			}
		}
    }

    gfx::rect apply_constraints(gfx::rect r) const
    {
        float aspect = float(r.w) / float(r.h);
        auto result = r;
        result.h = int(calculated_line_height_);
        result.w = int(aspect * calculated_line_height_);
        return result;
    }


    float calculated_line_height_{};

    using key_t = std::pair<size_t, size_t>;
	std::map<key_t, embedded_image> embedded_images_{};
	std::map<key_t, embedded_text> embedded_texts_{};

    rich_config cfg_;
};

int main()
{

	os::init();
	gfx::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });


	{
		os::window win("win", os::window::centered, os::window::centered, 1366, 768, os::window::resizable);
		gfx::renderer rend(win, true);


        gfx::glyphs_builder builder;
        builder.add(gfx::get_all_glyph_range());

		auto info = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSans-Bold.ttf", builder.get(), 30, 2);
        auto font = rend.create_font(std::move(info));
        auto info1 = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSerif-BoldItalic.ttf", builder.get(), 46, 2);
        auto font1 = rend.create_font(std::move(info1));

        auto info2 = gfx::create_font_from_ttf(DATA"fonts/wds052801.ttf", builder.get(), 46, 2);
        auto font2 = rend.create_font(std::move(info2));



		auto img_symbol = rend.create_texture(DATA"symbol.png");
        auto img_background = rend.create_texture(DATA"background.png");



		bool running = true;
        math::transformf tr;


        size_t curr_lang = 0;
		std::string text = texts[curr_lang];
		auto valign = gfx::align::top;
		auto halign = gfx::align::center;
        float scale = 0.5f;

        float line_scale = 2.0f;
		while(running)
		{
			os::event e{};
			while(os::poll_event(e))
			{
				if(e.type == os::events::quit)
				{
					std::cout << "quit (all windows were closed)" << std::endl;
					running = false;
					break;
				}
				if(e.type == os::events::key_down)
				{
					if(e.key.code == os::key::f5)
					{
                        curr_lang++;
                        curr_lang %= texts.size();
                        text = texts[curr_lang];
					}
                    if(e.key.code == os::key::backspace)
					{
						if(!text.empty())
                        {
                            text.pop_back();
                        }
					}
                    if(e.key.code == os::key::enter)
					{
                        if(e.key.shift)
                        {
                            text += "\n";
                        }
                        else if(e.key.ctrl)
                        {
                            gfx::toggle_debug_draw();
                        }
					}
                    if(e.key.code == os::key::f1)
                    {
                        scale += 0.01f;
                    }
                    if(e.key.code == os::key::f2)
                    {
                        scale -= 0.01f;
                    }
                    if(e.key.code == os::key::f3)
                    {
                        {
                            uint32_t va = valign;
                            va *= 2;
                            valign = gfx::align(va);
                        }
                        if(valign == gfx::align::baseline_bottom)
                        {
                            valign = gfx::align::top;
                        }
                    }
                    if(e.key.code == os::key::f4)
                    {
                        {
                            uint32_t va = halign;
                            va *= 2;
                            halign = gfx::align(va);
                        }
                        if(halign == gfx::align::top)
                        {
                            halign = gfx::align::left;
                        }
                    }

                    if(e.key.code == os::key::f6)
                    {
                        line_scale += 0.01f;
                    }


                    if(e.key.code == os::key::f7)
                    {
                        line_scale -= 0.01f;
                    }
				}

                if(e.type == os::events::mouse_wheel)
				{
                    tr.scale(1.0f + float(e.wheel.y) * 0.01f, 1.0f + float(e.wheel.y) * 0.01f, 0.0f);
				}
                if(e.type == os::events::text_input)
                {
                    text += e.text.text;
                }
			}

            float x_percent = 4.0f;
            float y_percent = 13.0f;
            float x_off = x_percent / 100.0f * rend.get_rect().w;
            float y_off = y_percent / 100.0f * rend.get_rect().h;

            gfx::rect area = rend.get_rect();
            area.expand(int(-x_off), int(-y_off));

			rend.clear(gfx::color::gray());

			gfx::draw_list list;

            list.add_image(img_background, rend.get_rect());
            list.add_rect(area, gfx::color::red(), false);

            rich_text t;
            t.set_font(font);
            t.set_outline_color(gfx::color::black());
            t.set_outline_width(0.1f);
			t.set_shadow_offsets({3, 3});
            t.set_shadow_color(gfx::color::black());

            t.set_utf8_text(text);
            t.set_alignment(valign | halign);

            rich_config cfg;
            cfg.line_height_scale = line_scale;

            {
                auto& style = cfg.styles["style1"];
                style.font = font2;
                style.color_top = gfx::color::green();
                style.color_bot = gfx::color::yellow();
                style.scale = 2.5f;
                style.outline_color = gfx::color::red();
                style.outline_width = 0.3f;
            }

            {
                auto& style = cfg.styles["style2"];
                style.font = font;
                style.color_top = gfx::color::cyan();
                style.color_bot = gfx::color::cyan();
                style.scale = 1.4f;
            }

            {
                auto& style = cfg.styles["style3"];
                style.font = font1;
                style.color_top = gfx::color::red();
                style.color_bot = gfx::color::blue();

            }

            cfg.image_getter = [&](const std::string& tag) -> image_data
            {
                if(tag == "scatter")
                {
                    return {img_symbol->get_rect(), img_symbol};
                }

                if(tag == "wild")
                {
                    return {img_symbol->get_rect(), img_symbol};
                }

                return {{0, 0, 256, 256},{}};
            };

            t.set_config(cfg);

            t.draw(list, tr, area);

            rend.draw_cmd_list(list);

			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
