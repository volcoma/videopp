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
R"(^FREE SPINS^
$3$ #scatter# symbols anywhere on the $2nd$, $3rd$ and $4th$ reels only trigger $10 FREE SPINS$ + $MOVING SYMBOLS$.
During $FREE SPINS$, if symbol appear on the entire $1st$ reel and on any position on the $3rd$, $4th$ or $5th$ reel, the positions on the row between them will also be filled with that symbol.
In case of retriggering $FREE SPINS$, the player wins $10$ new $FREE SPINS$ which are added to the current number of $FREE SPINS$.
The winnings from #scatter# symbols and new @FREE SPINS@ are won before the expanding of the moving symbols. The $FREE SPINS$ are played at trigger bet and lines. During $FREE SPINS$ an alternate set of reels is used.

^WILD^
#wild# subtitutes for all symbols except #scatter#.)";

static std::string BG =
R"(^БЕЗПЛАТНИ СПИНОВЕ^
$3$ #scatter# символи навсякъде на $2ра$, $3та$ и $4та$ ролка задействат само $10 БЕЗПЛАТНИ СПИНОВЕ$ + $ДВИЖЕЩИ СИМВОЛИ$.
По време на $БЕЗПЛАТНИ СПИНОВЕ$, ако символът се появи на цялата $1ва$ ролка и на всяка позиция на $3та$, $4та$ или $5та$ ролка, позициите на реда между тях също ще бъдат запълнени с този символ.
В случай на повторно задействане на $БЕЗПЛАТНИ СПИНОВЕ$, играчът печели $10$ нови $БЕЗПЛАТНИ СПИНОВЕ$, които се добавят към текущия брой $БЕЗПЛАТНИ СПИНОВЕ$.
Печалбите от #scatter# символи и нови @БЕЗПЛАТНИ СПИНОВЕ@ се печелят преди разширяването на движещите се символи. $БЕЗПЛАТНИ СПИНОВЕ$ се играят при залагане на тригер и линии. По време на $БЕЗПЛАТНИ СПИНОВЕ$ се използва алтернативен набор от макари.

^ДИВ^
#wild# замества за всички символи, с изключение на #scatter#.)";

static std::string ESP =
R"(^GIRAS GRATIS^
Los símbolos $3$ #scatter# en cualquier lugar de los carretes $2nd$, $3rd$ y $4th$ solo activan $10 GIRAS GRATIS$ + $SÍMBOLOS EN MOVIMIENTO$.
Durante $GIRAS GRATIS$, si el símbolo aparece en todo el carrete $ 1st $ y en cualquier posición en el carrete $3rd$, $4th$ o $5th$, las posiciones en la fila entre ellos también se llenarán con ese símbolo.
En caso de reactivar $GIRAS GRATIS$, el jugador gana $10$ nuevos $GIRAS GRATIS$ que se agregan al número actual de $FREE SPINS$.
Las ganancias de los símbolos #scatter# y los nuevos @GIRAS GRATIS@ se ganan antes de la expansión de los símbolos móviles. Los $GIRAS GRATIS$ se juegan en la apuesta de activación y en las líneas. Durante $GIRAS GRATIS$ se usa un conjunto alternativo de carretes.

^SALVAJE^
#wild# sustituye a todos los símbolos excepto #scatter#.)";


static std::array<std::string, 3> texts{EN, BG, ESP};

struct rich_text
{
    struct embedded_image
    {
        size_t line{};
        float x_offset{};
        float y_offset{};
        gfx::texture_weak_ptr image;
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
        });

        text.set_clear_geometry_callback([&]()
        {
            images.clear();
        });

        {
			static const std::regex rx(R"(#scatter#)"); // --> #scatter#
            auto decorators = text.add_decorators(rx);
            for(const auto& decorator : decorators)
            {
                decorator->callback = [&](bool add, float pen_x, float pen_y, size_t line)
                {
                    auto it = textures.find("#scatter#");
                    if(it == std::end(textures))
                    {
                        return 0.0f;
                    }

                    const auto& texture_weak = it->second;
                    if(add)
                    {
                        images.emplace_back();
                        auto& image = images.back();

                        image.line = line;
                        image.x_offset = pen_x;
                        image.y_offset = pen_y;
                        image.image = texture_weak;
                    }

                    auto texture = texture_weak.lock();

                    return float(apply_constraints(texture->get_rect()).w);
                };
            }
        }

        {
			static const std::regex rx(R"(#wild#)"); // --> #wild#
            auto decorators = text.add_decorators(rx);
            for(const auto& decorator : decorators)
            {
                decorator->callback = [&](bool add, float pen_x, float pen_y, size_t line)
                {
                    auto it = textures.find("#wild#");
                    if(it == std::end(textures))
                    {
                        return 0.0f;
                    }

                    const auto& texture_weak = it->second;
                    if(add)
                    {
                        images.emplace_back();
                        auto& image = images.back();

                        image.line = line;
                        image.x_offset = pen_x;
                        image.y_offset = pen_y;
                        image.image = texture_weak;
                    }

                    auto texture = texture_weak.lock();

                    return float(apply_constraints(texture->get_rect()).w);
                };
            }
        }


        {
            static const std::regex rx(R"(\$([\s\S]*?)\$)"); // --> $some_text$
            auto decorators = text.add_decorators(rx);

            for(const auto& decorator : decorators)
            {
                decorator->color_top = gfx::color::cyan();
                decorator->color_bot = gfx::color::cyan();
            }
        }

        {
            static const std::regex rx(R"(\@([\s\S]*?)\@)"); // --> @some_text@
            auto decorators = text.add_decorators(rx);

            for(const auto& decorator : decorators)
            {
                decorator->color_top = gfx::color::red();
                decorator->color_bot = gfx::color::yellow();
                decorator->scale = 1.4f;
                decorator->leaning = 12.0f;
            }
        }

        {
            static const std::regex rx(R"(\^([\s\S]*?)\^)"); // --> ^some_text^
            auto decorators = text.add_decorators(rx);

            for(const auto& decorator : decorators)
            {
                decorator->color_top = gfx::color::green();
                decorator->color_bot = gfx::color::yellow();
                decorator->scale = 2.4f;
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
    std::regex rx(R"(\$([\s\S]*?)\$)");

    std::cout << "PRINT EN" << std::endl;
    print_matches(rx, EN);

    std::cout << "PRINT BG" << std::endl;
    print_matches(rx, BG);

    std::cout << "PRINT ESP" << std::endl;
    print_matches(rx, ESP);


	os::init();
	gfx::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });


	{
		os::window win("win", os::window::centered, os::window::centered, 1366, 768, os::window::resizable);
		gfx::renderer rend(win, true);


        gfx::glyphs_builder builder;
        builder.add(gfx::get_all_glyph_range());

		auto info = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSans-Bold.ttf", builder.get(), 30, 2);
//        auto info = gfx::create_font_from_ttf(DATA"fonts/wds052801.ttf", builder.get(), 46, 2);
        auto font = rend.create_font(std::move(info));

		auto img_symbol = rend.create_texture(DATA"symbol.png");
        auto img_background = rend.create_texture(DATA"background.png");



		bool running = true;
        math::transformf tr;


        size_t curr_lang = 0;
        std::string text = "";//texts[curr_lang];
        auto valign = gfx::align::top;
        auto halign = gfx::align::left;
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
            t.textures["#scatter#"] = img_symbol;
            t.textures["#wild#"] = img_symbol;

            t.draw(list, tr, section);

            rend.draw_cmd_list(list);


			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
