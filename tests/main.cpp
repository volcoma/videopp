#include <ospp/os.h>
#include <videopp/renderer.h>
#include <videopp/html/html_page.h>
#include <videopp/ttf_font.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
#include <iostream>

namespace detail
{

template <typename Container = std::string, typename CharT = char, typename Traits = std::char_traits<char>>
auto read_stream(std::basic_istream<CharT, Traits>& in, Container& container) -> bool
{
	static_assert(
		// Allow only strings...
		std::is_same<Container,
					 std::basic_string<CharT, Traits, typename Container::allocator_type>>::value ||
			// ... and vectors of the plain, signed, and
			// unsigned flavours of CharT.
			std::is_same<Container, std::vector<CharT, typename Container::allocator_type>>::value ||
			std::is_same<Container, std::vector<std::make_unsigned_t<CharT>,
												typename Container::allocator_type>>::value ||
			std::is_same<Container,
						 std::vector<std::make_signed_t<CharT>, typename Container::allocator_type>>::value,
		"only strings and vectors of ((un)signed) CharT allowed");

	auto const start_pos = in.tellg();
	if(std::streamsize(-1) == start_pos)
	{
		return false;
	};

	if(!in.seekg(0, std::ios_base::end))
	{
		return false;
	};

	auto const end_pos = in.tellg();

	if(std::streamsize(-1) == end_pos)
	{
		return false;
	};

	auto const char_count = end_pos - start_pos;

	if(!in.seekg(start_pos))
	{
		return false;
	};

	container.resize(static_cast<std::size_t>(char_count));

	if(!container.empty())
	{
		if(!in.read(reinterpret_cast<CharT*>(&container[0]), char_count))
		{
			return false;
		};
	}

	return true;
}
} // namespace detail

auto load_file(const std::string& path, std::string& buffer) -> bool
{
	std::ifstream stream(path, std::ios::in | std::ios::binary);

	if(!stream.is_open())
	{
		return false;
	}

	return detail::read_stream(stream, buffer);
}

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
        options.fonts_dir = DATA;
        options.default_font = "FreeSerif";
        options.default_monospace_font = "FreeMono";
        options.default_font_size = 16;

        video_ctrl::html_context html_ctx(rend, std::move(options));
        video_ctrl::html_page page(html_ctx);
        page.load(html);
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
                    if(e.key.code == os::key::r)
                    {
                        if(load_file(DATA"/html/example.html", html))
                        {
                            page.load(html);
                        }
                    }
                }
			}

            auto mouse_pos = os::mouse::get_position(win);

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
			std::cout << (avg_dur.count() * 1000) / count << std::endl;
		}
	}
	os::shutdown();

	return 0;
}
