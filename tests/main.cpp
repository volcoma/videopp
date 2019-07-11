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

			//page.draw(0, 0, rend.get_rect().w);

            std::vector<math::vec2> points
            {
                {10, 250},
                {140, 250},
                {350, 60},
                {600, 510},
                {850, 60},
                {1100, 250},
                {1300, 250}
            };

            float thickness = 20;
#define NORMALIZE2F_OVER_ZERO(VX,VY)     { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = 1.0f / math::sqrt(d2); VX *= inv_len; VY *= inv_len; } }

            std::vector<math::vec2> centers;
            video_ctrl::polyline line;
            for(size_t i = 0; i < points.size() - 2; i++)
            {

                const auto& p0 = points[i + 0];
                if(i == 0)
                    line.line_to(p0);

                const auto& p1 = points[i + 1];
                const auto& p2 = points[i + 2];

                auto angle1 = atan2f(p0.y - p1.y, p0.x-p1.x) - math::radians(90.0f);
                auto angle2 = atan2f(p1.y - p2.y, p1.x-p2.x) - math::radians(90.0f);
                if(angle1 < 0)
                {
                    angle1 += math::radians(360.0f);
                }
                if(angle2 < 0)
                {
                    angle2 += math::radians(360.0f);
                }
                auto deg1 = math::degrees(angle1);
                auto deg2 = math::degrees(angle2);

                float dx = p1.x - p0.x;
                float dy = p1.y - p0.y;
                NORMALIZE2F_OVER_ZERO(dx, dy);
                math::vec2 norm{dy, -dx};

                float arc_radius = thickness * 0.5f;
                centers.push_back(p1 + norm * arc_radius);
                if(p2.y < p1.y || deg2 < deg1)
                {
                    line.arc_to_negative(p1 + norm * arc_radius, arc_radius, angle1, angle2);
                }
                else
                {
                    angle1 += math::radians(180.0f);
                    angle2 += math::radians(180.0f);
                    line.arc_to(p1 + norm * arc_radius, arc_radius, angle1, angle2);

                }

                if(i == points.size() - 3)
                    line.line_to(p2);

            }

            video_ctrl::draw_list list;
            auto c1 = video_ctrl::color::white();
            auto c2 = video_ctrl::color::black();

            list.add_polyline_gradient(line, c1, c2, false, thickness, 1.0f);

            for(const auto& p : centers)
            {
                list.add_rect({int(p.x), int(p.y), 2, 2}, video_ctrl::color::red());
            }
            rend.draw_cmd_list(list);
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
