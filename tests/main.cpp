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

struct rich_style
{

};



struct rich_text
{
    struct embedded_image
    {
        size_t line{};
        float x_offset{};
        float y_offset{};
        gfx::texture_weak_ptr image;
    };

	struct embedded_text
	{
		size_t line{};
		float x_offset{};
		float y_offset{};
		gfx::text text;
	};


    void draw(gfx::draw_list& list, math::transformf transform, gfx::rect dst_rect, gfx::size_fit sz_fit = gfx::size_fit::shrink_to_fit,
              gfx::dimension_fit dim_fit = gfx::dimension_fit::uniform)
    {
        auto advance = (max_line_height - text.get_font()->line_height);
        transform.translate(0.0f, advance * 0.5f, 0.0f);
        dst_rect.h -= int(advance * 0.5f);

        auto max_w = dst_rect.w;

        math::transformf world;
        size_t loop{0};
		while(loop < 16)
        {
            text.set_max_width(max_w);
            world = gfx::fit_text(text, transform, dst_rect, sz_fit, dim_fit);
            auto w = int(float(dst_rect.w) / world.get_scale().x);

            if(w == max_w)
            {
                break;
            }

            max_w = w;
            loop++;
        }

        list.add_text(text, world);

        const auto& font = text.get_font();
        auto height = font->ascent - font->descent;
        auto midline = font->descent + height / 2;

        for(const auto& embedded : images)
        {
            auto image = embedded.image.lock();

            if(image)
            {
                auto img_dst_rect = apply_constraints(image->get_rect());

                img_dst_rect.x += int(embedded.x_offset);
                img_dst_rect.y += int(embedded.y_offset);
                img_dst_rect.y -= img_dst_rect.h / 2 + int(midline);
                list.add_image(image, img_dst_rect, world);
            }
        }

		for(const auto& embedded : texts)
		{
			math::transformf tr;
			tr.set_position(embedded.x_offset, embedded.y_offset, 0);
			list.add_text(embedded.text, world * tr);
		}
    }

    void setup_decorators()
    {
        max_line_height = text.get_font()->line_height * 2.0f;
        auto advance = (max_line_height - text.get_font()->line_height);

        text.set_advance({0, advance});

        text.set_align_line_callback([&](size_t line, float align_x)
        {
            for(auto& image : images)
            {
                if(image.line == line)
                {
                    image.x_offset += align_x;
                }
            }

			for(auto& text : texts)
			{
				if(text.line == line)
				{
					text.x_offset += align_x;
				}
			}
        });

        text.set_clear_geometry_callback([&]()
        {
            images.clear();
			texts.clear();
        });

        {
			auto decorators = text.add_decorators("img");

			for(const auto& decorator : decorators)
            {
				decorator->calculate_size = [&](const char* str_begin, const char* str_end)
				{
					std::string utf8_str(str_begin, str_end);
					auto it = textures.find(utf8_str);
                    if(it == std::end(textures))
                    {
                        return 0.0f;
                    }

                    const auto& texture_weak = it->second;
                    auto texture = texture_weak.lock();
                    return float(apply_constraints(texture->get_rect()).w);
                };

				decorator->generate_geometry = [&](float pen_x, float pen_y, size_t line, const char* str_begin, const char* str_end)
				{
					std::string utf8_str(str_begin, str_end);
					auto it = textures.find(utf8_str);
					if(it == std::end(textures))
					{
						return;
					}

					const auto& texture_weak = it->second;

					images.emplace_back();
					auto& image = images.back();

					image.line = line;
					image.x_offset = pen_x;
					image.y_offset = pen_y;
					image.image = texture_weak;
				};
            }
        }

		{
			auto decorators = text.add_decorators("style1");

			for(const auto& decorator : decorators)
			{
				decorator->color_top = gfx::color::green();
				decorator->color_bot = gfx::color::yellow();
				decorator->scale = 2.4f;
			}
		}

        {
			auto decorators = text.add_decorators("style2");

            for(const auto& decorator : decorators)
            {
                decorator->color_top = gfx::color::cyan();
                decorator->color_bot = gfx::color::cyan();
            }
        }


		{
			auto decorators = text.add_decorators("style3");

			for(const auto& decorator : decorators)
			{
				decorator->calculate_size = [&](const char* str_begin, const char* str_end)
				{
					std::string utf8_str(str_begin, str_end);

					gfx::text embedded_text;
					embedded_text.set_font(wierd_font);
					embedded_text.set_vgradient_colors(gfx::color::red(), gfx::color::yellow());
					embedded_text.set_outline_width(0.3f);
					embedded_text.set_outline_color(gfx::color::blue());
					embedded_text.set_shadow_offsets({-2, -2});
					embedded_text.set_shadow_color(gfx::color::white());

					embedded_text.set_alignment(gfx::align::baseline | gfx::align::left);
					embedded_text.set_utf8_text(std::move(utf8_str));
					return embedded_text.get_width();
				};

				decorator->generate_geometry = [&](float pen_x, float pen_y, size_t line, const char* str_begin, const char* str_end)
				{
					texts.emplace_back();
					auto& t = texts.back();

					std::string utf8_str(str_begin, str_end);

					gfx::text embedded_text;
					embedded_text.set_font(wierd_font);
					embedded_text.set_vgradient_colors(gfx::color::red(), gfx::color::yellow());
					embedded_text.set_outline_width(0.3f);
					embedded_text.set_outline_color(gfx::color::blue());
					embedded_text.set_shadow_offsets({-2, -2});
					embedded_text.set_shadow_color(gfx::color::white());

					embedded_text.set_alignment(gfx::align::baseline | gfx::align::left);
					embedded_text.set_utf8_text(std::move(utf8_str));

					t.line = line;
					t.x_offset = pen_x;
					t.y_offset = pen_y;
					t.text = std::move(embedded_text);
				};

			}
		}
    }

    gfx::rect apply_constraints(gfx::rect r) const
    {
        float aspect = float(r.w) / float(r.h);
        auto result = r;
        result.h = int(max_line_height);
        result.w = int(aspect * max_line_height);
        return result;
    }


    gfx::text text;
    float max_line_height{};
	std::vector<embedded_image> images{};
	std::vector<embedded_text> texts{};

    std::map<std::string, gfx::texture_weak_ptr> textures;
};

void print_matches(const std::regex& rx, const std::string& text)
{
    std::vector<std::pair<int, int>> index_matches;

    for(auto it = std::sregex_iterator(text.begin(), text.end(), rx);
        it != std::sregex_iterator();
         ++it)
    {

        index_matches.emplace_back(it->position(), it->length());
    }

    std::cout << "found " << index_matches.size() << " matches!" << std::endl;
    for(const auto& range : index_matches)
    {
        std::cout << std::string(&text[range.first], range.second) << std::endl;
    }
}

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
		auto info1 = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSerif-BoldItalic.ttf", builder.get(), 46, 2);
        auto font = rend.create_font(std::move(info));

		wierd_font = rend.create_font(std::move(info1));
		auto img_symbol = rend.create_texture(DATA"symbol.png");
        auto img_background = rend.create_texture(DATA"background.png");



		bool running = true;
        math::transformf tr;


        size_t curr_lang = 0;
		std::string text = texts[curr_lang];
		auto valign = gfx::align::top;
		auto halign = gfx::align::center;
        float leaning = 0.0f;
        float scale = 0.5f;
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
				}

                if(e.type == os::events::mouse_wheel)
				{
                    //leaning += float(e.wheel.y);
                    tr.scale(1.0f + float(e.wheel.y) * 0.01f, 1.0f + float(e.wheel.y) * 0.01f, 0.0f);
				}
                if(e.type == os::events::text_input)
                {
                    text += e.text.text;
                }
			}


			rend.clear(gfx::color::gray());
			//page.draw(0, 0, rend.get_rect().w);

			gfx::draw_list list;

            gfx::rect section = rend.get_rect();

            float x_percent = 4.0f;
            float y_percent = 13.0f;
            float x_off = x_percent / 100.0f * rend.get_rect().w;
            float y_off = y_percent / 100.0f * rend.get_rect().h;

            section.expand(int(-x_off), int(-y_off));

            list.add_image(img_background, rend.get_rect());
            list.add_rect(section, gfx::color::red(), false);

            rich_text t;
            t.text.set_font(font);
            t.text.set_utf8_text(text);
            t.text.set_alignment(valign | halign);
            t.text.set_leaning(leaning);
            t.text.set_outline_color(gfx::color::black());
            t.text.set_outline_width(0.1f);
			t.text.set_shadow_offsets({3, 3});
            t.text.set_shadow_color(gfx::color::black());
            t.setup_decorators();
			t.textures["scatter"] = img_symbol;
			t.textures["wild"] = img_symbol;

            t.draw(list, tr, section);

            rend.draw_cmd_list(list);


			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
