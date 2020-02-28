#include <ospp/os.h>
#include <videopp/renderer.h>
#include <videopp/ttf_font.h>
#include <videopp/rich_text.h>


#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <regex>

static std::string EN =
R"(style1(FREE SPINS)
style2(3) image(__SCATTER__) symbols anywhere on the style2(2nd), style2(3rd) and style2(4th) reels only trigger style2(__FGCOUNT__) style2(FREE SPINS) + style2(MOVING SYMBOLS).
During style2(FREE SPINS), if symbol appear on the entire style2(1st) reel and on any position on the style2(3rd), style2(4th) or style2(5th) reel, the positions on the row between them will also be filled with that symbol.
In case of retriggering style2(FREE SPINS), the player wins style2(__FGCOUNT__) new style2(FREE SPINS) which are added to the current number of style2(FREE SPINS).
The winnings from image(__SCATTER__) symbols and new style3(FREE SPINS) are won before the expanding of the moving symbols. The style2(FREE SPINS) are played at trigger bet and lines. During style2(FREE SPINS) an alternate set of reels is used.
Some random text with dynamically styled data style2(__CURRENCY__). Some random text after that for no reason at all. Some random text after that for no reason at all.

style1(WILD)
image(__WILD__) subtitutes for all symbols except image(__SCATTER__).)";

static std::string BG =
R"(style1(БЕЗПЛАТНИ СПИНОВЕ)
style2(3) image(__SCATTER__) символи навсякъде на style2(2ра), style2(3та) и style2(4та) ролка задействат само style2(10 БЕЗПЛАТНИ СПИНОВЕ) + style2(ДВИЖЕЩИ СИМВОЛИ).
По време на style2(БЕЗПЛАТНИ СПИНОВЕ), ако символът се появи на цялата style2(1ва) ролка и на всяка позиция на style2(3та), style2(4та) или style2(5та) ролка, позициите на реда между тях също ще бъдат запълнени с този символ.
В случай на повторно задействане на style2(БЕЗПЛАТНИ СПИНОВЕ), играчът печели style2(10) нови style2(БЕЗПЛАТНИ СПИНОВЕ), които се добавят към текущия брой style2(БЕЗПЛАТНИ СПИНОВЕ).
Печалбите от image(__SCATTER__) символи и нови style3(БЕЗПЛАТНИ СПИНОВЕ) се печелят преди разширяването на движещите се символи. style2(БЕЗПЛАТНИ СПИНОВЕ) се играят при залагане на тригер и линии. По време на style2(БЕЗПЛАТНИ СПИНОВЕ) се използва алтернативен набор от макари.

style1(ЖОКЕР)
image(__WILD__) замества за всички символи, с изключение на image(__SCATTER__).)";

static std::string ESP =
R"(style1(GIRAS GRATIS)
Los símbolos style2(3) image(__SCATTER__) en cualquier lugar de los carretes style2(2nd), style2(3rd) y style2(4th) solo activan style2(10 GIRAS GRATIS) + style2(SÍMBOLOS EN MOVIMIENTO).
Durante style2(GIRAS GRATIS), si el símbolo aparece en todo el carrete style2(1st) y en cualquier posición en el carrete style2(3rd), style2(4th) o style2(5th), las posiciones en la fila entre ellos también se llenarán con ese símbolo.
En caso de reactivar style2(GIRAS GRATIS), el jugador gana style2(10) nuevos style2(GIRAS GRATIS) que se agregan al número actual de style2(FREE SPINS).
Las ganancias de los símbolos image(__SCATTER__) y los nuevos style3(GIRAS GRATIS) se ganan antes de la expansión de los símbolos móviles. Los style2(GIRAS GRATIS) se juegan en la apuesta de activación y en las líneas. Durante style2(GIRAS GRATIS) se usa un conjunto alternativo de carretes.

style1(SALVAJE)
image(__WILD__) sustituye a todos los símbolos excepto image(__SCATTER__).)";


static std::vector<std::string> texts{EN, BG, ESP, {}};

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

		auto info1 = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSans-Bold.ttf", builder.get(), 46, 2);
		auto font1 = rend.create_font(std::move(info1));

		auto info2 = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSerif-BoldItalic.ttf", builder.get(), 46, 2);
		auto font2 = rend.create_font(std::move(info2));


		auto image_symbol = rend.create_texture(DATA"symbol.png");
		auto image_background = rend.create_texture(DATA"background.png");



		bool running = true;
        math::transformf tr;


        int currency{100};
        int fg_count{2};
        size_t curr_lang = 0;
		std::string text = texts[curr_lang];
		auto valign = gfx::align::top;
		auto halign = gfx::align::center;
        float scale = 0.5f;


		gfx::rich_text t;
		t.set_font(font);
		t.set_outline_color(gfx::color::black());
		t.set_outline_width(0.1f);
		t.set_shadow_offsets({3, 3});
		t.set_shadow_color(gfx::color::black());

		gfx::rich_config cfg;
		cfg.line_height_scale = 2.0f;

		{
			auto& style = cfg.styles["style1"];
			style.font = font1;
			style.color_top = gfx::color::red();
			style.color_bot = gfx::color::green();
			style.scale = 2.5f;
			style.outline_color_top = gfx::color::red();
            style.outline_color_bot = gfx::color::green();
			style.outline_width = 0.3f;
		}
		{
			auto& style = cfg.styles["style2"];
			style.font = font;
			style.color_top = gfx::color::blue();
			style.color_bot = gfx::color::cyan();
			style.shadow_color_top = gfx::color::blue();
			style.shadow_color_bot = gfx::color::cyan();
			style.shadow_offsets = {-2.0f, -2.0f};
			style.outline_color_top = gfx::color::green();
            style.outline_color_bot = gfx::color::green();
			style.outline_width = 0.2f;
			style.scale = 1.4f;
		}
		{
			auto& style = cfg.styles["style3"];
			style.font = font2;
			style.color_top = gfx::color::red();
			style.color_bot = gfx::color::blue();
			style.outline_color_top = gfx::color::green();
            style.outline_color_bot = gfx::color::green();
			style.outline_width = 0.2f;
		}
		cfg.image_getter = [&](const std::string& content, gfx::image_data& out)
		{
			if(content == "__SCATTER__")
			{
				out.src_rect = image_symbol->get_rect();
				out.image = image_symbol;
			}
			else if(content == "__WILD__")
			{
				out.src_rect = image_symbol->get_rect();
				out.image = image_symbol;
			}
		};
		cfg.text_getter = [&](const std::string& content, gfx::text& out)
		{
			if(content == "__CURRENCY__")
			{
				std::string str_val = std::to_string(currency);
				std::string currency_code = "EUR";

				out.set_utf8_text(str_val + currency_code);

				gfx::text_decorator decorator;
				decorator.scale = 0.7f;
				decorator.script = gfx::script_line::baseline;
				decorator.unicode_range.begin = gfx::text::count_glyphs(str_val);
				decorator.unicode_range.end = decorator.unicode_range.begin + gfx::text::count_glyphs(currency_code);
				out.set_decorators({decorator});
			}
			else if(content == "__FGCOUNT__")
			{
                out.set_utf8_text(std::to_string(fg_count));
            }
		};

		t.set_config(cfg);

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
						if(valign == gfx::align::cap_height_top)
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

                    if(e.key.code == os::key::f8)
                    {
                        currency++;
                        currency %= 101;
                        fg_count += 1;
                        fg_count %= 21;

                        t.clear_lines();
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
            rend.clear(gfx::color::gray());

			float x_percent = 4.0f;
			float y_percent = 13.0f;
			float x_off = x_percent / 100.0f * rend.get_rect().w;
			float y_off = y_percent / 100.0f * rend.get_rect().h;

			gfx::rect area = rend.get_rect();
			area.expand(int(-x_off), int(-y_off));

            gfx::draw_list list;
			list.add_image(image_background, rend.get_rect());
            list.add_rect(area, gfx::color::red(), false);

//			gfx::text_builder b;
//			b.append("some random text");
//			b.append("__WILD__", "image");
//			b.append("__WIN_AMOUNT__", "style3");
//			b.append("some random text:", "style2");
//			b.append_formatted("10");
//			gfx::apply_builder(b, t);

            t.set_alignment(valign | halign);
			t.set_utf8_text(text);

			list.add_text(t, tr);
			//list.add_text(t, gfx::align_and_fit_text(t, tr, area, gfx::size_fit::shrink_to_fit, gfx::dimension_fit::uniform));
			//list.add_text(t, gfx::align_wrap_and_fit_text(t, tr, area, gfx::size_fit::shrink_to_fit, gfx::dimension_fit::uniform));

			//t.draw(list, tr, area, gfx::size_fit::shrink_to_fit, gfx::dimension_fit::uniform);

			//std::cout << list.to_string() << std::endl;

			rend.draw_cmd_list(list);

			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
