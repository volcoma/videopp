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
		options.default_font_options = gfx::font_flags::use_kerning /*| gfx::font_flags::simulate_all*/;
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
		bool running = true;

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
						//page.load_from_file(DATA "html/simple_page/index.html");
                        page.load_from_file(DATA "html/text/pull_quotes2.html");

					}
				}
			}

			rend.clear(gfx::color::white());
			page.draw(0, 0, rend.get_rect().w);
			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
