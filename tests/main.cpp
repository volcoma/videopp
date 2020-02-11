#include <ospp/os.h>
#include <videopp/html/html_page.h>
#include <videopp/renderer.h>
#include <videopp/ttf_font.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

static std::string html =
R"(
<!DOCTYPE html>
<html>
<body>

<h1 style="text-align:center;">Centered Heading</h1>
<p style="text-align:center;">Centered paragraph.</p>
<h1 style="background-color:rgba(255,0,0,0.1);color:Tomato;">Hello World</h3>
<h3 style="background-color:rgba(0,255,0,0.1);color:Tomato;">Hello World</h3>
<p style="background-color:rgba(255,255,0,0.1);color:DodgerBlue;">Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh euismod tincidunt ut laoreet dolore magna aliquam erat volutpat.</p>
<p style="color:MediumSeaGreen;font-size: 32pt">Ut wisi enim ad minim veniam, quis nostrud exerci tation ullamcorper suscipit lobortis nisl ut aliquip ex ea commodo consequat.</p>
<pre>
Text in a pre element
is displayed in a fixed-width
font, and it preserves
both      spaces and
line breaks
</pre>

</body>
</html>
)";


int main()
{
	os::init();
	gfx::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });

    parallel::get_pool();
	{
		os::window win("win", os::window::centered, os::window::centered, 1366, 768, os::window::resizable);
		gfx::renderer rend(win, true);

		gfx::html_defaults options;
		options.default_font = "serif";
		options.default_font_size = 16;
		options.default_font_options = gfx::font_flags::use_kerning | gfx::font_flags::simulate_all;
		options.default_font_families = {
			{
				"monospace",
				{
					DATA"fonts/dejavu/DejaVuSansMono.ttf",
					DATA"fonts/dejavu/DejaVuSansMono-Oblique.ttf",
					DATA"fonts/dejavu/DejaVuSansMono-Bold.ttf",
					DATA"fonts/dejavu/DejaVuSansMono-BoldOblique.ttf",
				},
			},
			{
				"serif",
                {
					DATA"fonts/dejavu/DejaVuSerif.ttf",
					DATA"fonts/dejavu/DejaVuSerif-Italic.ttf",
					DATA"fonts/dejavu/DejaVuSerif-Bold.ttf",
					DATA"fonts/dejavu/DejaVuSerif-BoldItalic.ttf",
				},
			},
			{
				"sans-serif",
				{
					DATA"fonts/dejavu/DejaVuSans.ttf",
					DATA"fonts/dejavu/DejaVuSans-Oblique.ttf",
					DATA"fonts/dejavu/DejaVuSans-Bold.ttf",
					DATA"fonts/dejavu/DejaVuSans-BoldOblique.ttf",
				},
			},
			{
				"cursive",
                {
					DATA"fonts/dejavu/DejaVuSans.ttf",
					DATA"fonts/dejavu/DejaVuSans-Oblique.ttf",
					DATA"fonts/dejavu/DejaVuSans-Bold.ttf",
					DATA"fonts/dejavu/DejaVuSans-BoldOblique.ttf",
				},
			},
			{
				"fantasy",
                {
					DATA"fonts/dejavu/DejaVuSans.ttf",
					DATA"fonts/dejavu/DejaVuSans-Oblique.ttf",
					DATA"fonts/dejavu/DejaVuSans-Bold.ttf",
					DATA"fonts/dejavu/DejaVuSans-BoldOblique.ttf",
				},
			},
		};

		gfx::html_context html_ctx(rend, std::move(options));
		gfx::html_page page(html_ctx);
//		page.load_from_utf8(html, DATA);

        gfx::glyphs_builder builder;
//        builder.add(gfx::get_default_glyph_range());
//        builder.add(gfx::get_cyrillic_glyph_range());
//        builder.add(gfx::get_currency_glyph_range());
//        builder.add(gfx::get_korean_glyph_range());
        builder.add(gfx::get_all_glyph_range());

        auto info = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSerif.ttf", builder.get(), 80, 2);
        //auto info = gfx::create_font_from_ttf(DATA"fonts/wds052801.ttf", builder.get(), 80, 2);
        auto font = rend.create_font(std::move(info));

        auto image = rend.create_texture(DATA"wheel.png");

		bool running = true;
        math::transformf tr;

        std::string text = "1234";
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
						page.load_from_file(DATA "html/simple_page/index.html");
                        //page.load_from_file(DATA "html/text/pull_quotes2.html");

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
                            gfx::draw_list::toggle_debug_draw();
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
                        if(valign == gfx::align::baseline_bottom)
                        {
                            valign = gfx::align::top;
                        }
                        else
                        {
                            uint32_t va = valign;
                            va *= 2;
                            valign = gfx::align(va);
                        }
                    }
                    if(e.key.code == os::key::f4)
                    {
                        if(halign == gfx::align::top)
                        {
                            halign = gfx::align::left;
                        }
                        else
                        {
                            uint32_t va = halign;
                            va *= 2;
                            halign = gfx::align(va);
                        }
                    }
				}

                if(e.type == os::events::mouse_wheel)
				{
                    //leaning += float(e.wheel.y);
                    tr.scale(1.0f + float(e.wheel.y) * 0.1f, 1.0f + float(e.wheel.y) * 0.1f, 0.0f);
				}
                if(e.type == os::events::text_input)
                {
                    text += e.text.text;
                }
			}

            auto pos = os::mouse::get_position(win);
			rend.clear(gfx::color::gray());
			//page.draw(0, 0, rend.get_rect().w);

            gfx::draw_list list;

            tr.set_position(float(pos.x), float(pos.y), 0);

            for(size_t i = 0; i < size_t(gfx::text_line::count); ++i)
            {
                gfx::text t;
                t.set_font(font);
                t.set_utf8_text(text);
                t.set_alignment(valign | halign);
                t.set_leaning(leaning);
                //t.set_color(gfx::color::red());
                //t.set_shadow_offsets({2, 2});
                //t.set_outline_width(0.4f);

                std::vector<gfx::text_decorator> decorators;
                {
                    decorators.emplace_back();
                    auto& decorator = decorators.back();
                    decorator.begin_glyph = 2;
                    decorator.end_glyph = decorator.begin_glyph + 2;
                    decorator.align = gfx::text_line(size_t(gfx::text_line::ascent) + i);
                    decorator.scale = scale;
                }

                {
                    decorators.emplace_back();
                    auto& decorator = decorators.back();
                    decorator.begin_glyph = 2;
                    decorator.end_glyph = decorator.begin_glyph + 2;
                    decorator.align = gfx::text_line(size_t(gfx::text_line::ascent) + i);
                    decorator.scale = scale;
                }
                t.set_decorators(decorators);
                list.add_text(t, tr);

                tr.translate(0.0f, t.get_height() * tr.get_scale().y, 0.0f);
            }

            rend.draw_cmd_list(list);


			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
