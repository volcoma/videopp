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
		page.load_from_utf8(html, DATA);

        gfx::glyphs_builder builder;
        builder.add(gfx::get_default_glyph_range());
        auto info = gfx::create_font_from_ttf(DATA"fonts/dejavu/DejaVuSerif.ttf", builder.get(), 30, 2);
        auto font = rend.create_font(std::move(info));

		bool running = true;
        math::transformf tr;

        std::string text = "1234";
        auto valign = gfx::align::top | gfx::align::right;
        float leaning = 0.0f;
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
				if(e.type == os::events::window)
				{
					if(e.window.type == os::window_event_id::close)
					{

						std::cout << "quit (all windows were closed)" << std::endl;
						running = false;
					}
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
                        else
                        {
                            if(valign == gfx::align::baseline_bottom)
                            {
                                valign = gfx::align::left;
                            }
                            else
                            {
                                uint32_t va = valign;
                                va *= 2;
                                valign = gfx::align(va);
                            }
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

            tr.set_position(pos.x, pos.y, 0);
            //tr.rotate(0, 0, math::radians(1.0f));
            gfx::text t;
            t.set_font(font);
            t.set_utf8_text(text);
            t.set_alignment(valign);
            t.set_leaning(leaning);

            gfx::script_range range{};
            range.begin = 1;
            range.end = range.begin + 2;
            range.type = gfx::script_type::super;
            range.scale = 0.4f;
            t.add_script_range(range);

            range.begin = 6;
            range.end = range.begin + 2;
            range.type = gfx::script_type::normal;
            range.scale = 0.4f;
            t.add_script_range(range);

            range.begin = 10;
            range.end = range.begin + 2;
            range.type = gfx::script_type::sub;
            range.scale = 0.4f;
            t.add_script_range(range);

            //t.set_advance({20, 20});
            //gfx::polyline line;
            //line.line_to({0, 0});
            //line.bezier_curve_to({200, 300}, {400, 0}, {1000, 300});
            //line.ellipse({0, 0}, {200, 100}, 64);
            //t.set_line_path(line);
            //t.set_shadow_offsets({0, 20});
            list.add_text(t, tr);

//            tr.translate(0, t.get_height() * 3, 0);
//            gfx::text t2;
//            t2.set_font(font);
//            t2.set_utf8_text("12");
//            t2.set_alignment(valign);
//            list.add_text_superscript(t2, t2, tr);

            rend.draw_cmd_list(list);


			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
