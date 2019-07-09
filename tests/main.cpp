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
	video_ctrl::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });

	os::window master_win("master", os::window::centered, os::window::centered, 128, 128, os::window::hidden);
	video_ctrl::renderer master_rend(master_win, true);

	{
		auto centerd = os::window::centered;
		auto flags = os::window::resizable;
		os::window win("win", centerd, centerd, 1366, 768, flags);
		video_ctrl::renderer rend(win, false);

		video_ctrl::html_defaults options;
		options.default_font = "serif";
		options.default_font_size = 16;
		options.default_font_options = video_ctrl::font_flags::use_kerning /*| video_ctrl::font_flags::simulate_all*/;
		options.default_font_families = {
			{
				"monospace",
				{
					DATA"fonts/Free/FreeMono.ttf",
					DATA"fonts/Free/FreeMonoOblique.ttf",
					DATA"fonts/Free/FreeMonoBold.ttf",
					DATA"fonts/Free/FreeMonoBoldOblique.ttf",
				},
			},
			{
				"serif",
                {
					DATA"fonts/Free/FreeSerif.ttf",
					DATA"fonts/Free/FreeSerifItalic.ttf",
					DATA"fonts/Free/FreeSerifBold.ttf",
					DATA"fonts/Free/FreeSerifBoldItalic.ttf",
				},
			},
			{
				"sans-serif",
				{
					DATA"fonts/Free/FreeSans.ttf",
					DATA"fonts/Free/FreeSansOblique.ttf",
					DATA"fonts/Free/FreeSansBold.ttf",
					DATA"fonts/Free/FreeSansBoldOblique.ttf",
				},
			},
			{
				"cursive",
				{
					DATA"fonts/Mali/Mali-Regular.ttf",
					DATA"fonts/Mali/Mali-Italic.ttf",
					DATA"fonts/Mali/Mali-Bold.ttf",
					DATA"fonts/Mali/Mali-BoldItalic.ttf",
				},
			},
			{
				"fantasy",
				{
					DATA"fonts/LobsterTwo/LobsterTwo-Regular.ttf",
					DATA"fonts/LobsterTwo/LobsterTwo-Italic.ttf",
					DATA"fonts/LobsterTwo/LobsterTwo-Bold.ttf",
					DATA"fonts/LobsterTwo/LobsterTwo-BoldItalic.ttf",
				},
			},
		};

		video_ctrl::html_context html_ctx(rend, std::move(options));
		video_ctrl::html_page page(html_ctx);
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
						page.load_from_file(DATA "html/simple_page/index.html");
					}
				}
			}

			using namespace std::chrono_literals;
			auto start = std::chrono::high_resolution_clock::now();

			rend.clear(video_ctrl::color::white());

			page.draw(0, 0, rend.get_rect().w);

			rend.present();

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);

			static decltype(dur) avg_dur{};
			static int count = 0;

			if(count > 100)
			{
				count = 0;
				avg_dur = {};
			}
			count++;
			avg_dur += dur;
			// std::cout << (avg_dur.count() * 1000) / count << std::endl;
		}
	}
	os::shutdown();

	return 0;
}
