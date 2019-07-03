#include <ospp/os.h>
#include <videopp/renderer.h>
#include <videopp/rich_text/rich_text.h>
#include <videopp/ttf_font.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include <litehtml.h>
#include "master.css"

static std::string display_text =
	R"($2 Choosing the optimal platform
The choice of hardware platform has become less important than it used to be. The distinctions between
RISC and CISC processors, between PC's and mainframes, and between simple processors and vector processors are
becoming increasingly blurred as the standard PC processors with CISC instruction sets have got RISC cores, vector
processing instructions, multiple cores, and a processing speed exceeding that of yesterday's big mainframe computers.

Today, the choice of hardware platform for a given task is often determined by considerations such as price, compatibility,
second source, and the availability of good development tools, rather than by the processing power. Connecting several standard
PC's in a network may be both cheaper and more efficient than investing in a big mainframe computer. Big supercomputers with
massively parallel vector processing capabilities still have a niche in scientific computing, but for most purposes the standard
PC processors are preferred because of their superior performance/price ratio.

The CISC instruction set (called x86) of the standard PC processors is not optimal from a technological point of view.
This instruction set is maintained for the sake of backwards compatibility with a lineage of software that dates back to
around 1980 where RAM memory and disk space were scarce resources. However, the CISC instruction set is better than its reputation.
The compactness of the code makes caching more efficient today where cache size is a limited resource. The CISC instruction
set may actually be better than RISC in situations where code caching is critical. The worst problem of the x86 instruction
set is the scarcity of registers. This problem has been alleviated in the 64-bit extension to the x86 instruction set where the
number of registers has been doubled.

Thin clients that depend on network resources are not recommended for critical applications because the response times for network
resources cannot be controlled.

Small hand-held devices are becoming more popular and used for an increasing number of purposes such as email and web browsing that
previously required a PC. Similarly, we are seeing an increasing number of devices and machines with embedded microcontrollers.
I am not making any specific recommendation about which platforms and operating systems are most efficient for such applications,
but it is important to realize that such devices typically have much less memory and computing power than PCs. Therefore, it is
even more important to economize the resource use on such systems than it is on a PC platform. However, with a well optimized
software design, it is possible to get a good performance for many applications even on such small devices, as discussed on page 162.

This manual is based on the standard PC platform with an Intel, AMD or VIA processor and a Windows, Linux, BSD or Mac operating
system running in 32-bit or 64-bit mode. Much of the advice given here may apply to other platforms as well, but the examples have
been tested only on PC platforms. Graphics accelerators The choice of platform is obviously
influenced by the requirements of the task in question. For example, a heavy graphics application is preferably
implemented on a platform with a graphics coprocessor or graphics accelerator card. Some systems also have a dedicated
physics processor for calculating the physical movements of objects in a computer game or animation.

It is possible in some cases to use the high processing power of the processors on a graphics accelerator card
for other purposes than rendering graphics on the screen. However, such applications are highly system dependent and
therefore not recommended if portability is important. This manual does not cover graphics processors.)";

#include <iostream>

using namespace litehtml;

class container_videopp : public document_container
{
	video_ctrl::renderer& rend_;
	std::map<std::string, video_ctrl::font_ptr> fonts_;

public:
	video_ctrl::draw_list list_;
	container_videopp(video_ctrl::renderer& rend)
		: rend_(rend)
	{
	}

	virtual ~container_videopp() = default;

	litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int /*weight*/,
								   litehtml::font_style italic, unsigned int decoration,
								   litehtml::font_metrics* fm) override
	{
        std::string key = faceName + std::to_string(size);
		auto it = fonts_.find(key);
		if(it != fonts_.end())
		{
			return reinterpret_cast<uint_ptr>(&it->second);
		}

		video_ctrl::glyphs_builder builder;
		builder.add(video_ctrl::get_latin_glyph_range());
		builder.add(video_ctrl::get_cyrillic_glyph_range());

        auto font_path = DATA "/" + std::string(faceName) + ".ttf";
		auto font =
			rend_.create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), size, 2, true));

		if(fm)
		{
			// fixme cache this
			fm->ascent = static_cast<int>(font->ascent);
			fm->descent = static_cast<int>(font->descent);
			fm->height = static_cast<int>(font->line_height);
			const auto& x_glyph = font->get_glyph('x');
			fm->x_height = static_cast<int>(x_glyph.y1 - x_glyph.y0);
			fm->draw_spaces = italic == fontStyleItalic || decoration;
		}

		fonts_[key] = font;
		return reinterpret_cast<uint_ptr>(&fonts_[key]);
	}
	void delete_font(litehtml::uint_ptr hFont) override
	{
		auto font = *reinterpret_cast<video_ctrl::font_ptr*>(hFont);
		fonts_.erase(font->face_name);
	}
	int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) override
	{
		auto font = *reinterpret_cast<video_ctrl::font_ptr*>(hFont);
		video_ctrl::text t;
		t.set_font(font);
		t.set_utf8_text(text);
		return static_cast<int>(t.get_width());
	}
	void draw_text(litehtml::uint_ptr /*hdc*/, const litehtml::tchar_t* text, litehtml::uint_ptr hFont,
				   litehtml::web_color color, const litehtml::position& pos) override
	{
		auto font = *reinterpret_cast<video_ctrl::font_ptr*>(hFont);
		video_ctrl::text t;
		t.set_font(font);
		t.set_utf8_text(text);
		t.set_color({color.red, color.green, color.blue, color.alpha});

		math::transformf transform;
		transform.set_position(pos.x, pos.y, 0.0f);
		list_.add_text(t, transform);
	}

	int pt_to_px(int pt) override
	{
		return static_cast<int>(round(pt * 125.0 / 72.0));
	}
	int get_default_font_size() const override
	{
		return 16;
	}
	const litehtml::tchar_t* get_default_font_name() const override
	{
		return _t("FreeSans");
	}
	void draw_list_marker(litehtml::uint_ptr /*hdc*/, const litehtml::list_marker& /*marker*/) override
	{
		int a = 0;
		a++;
	}
	void load_image(const litehtml::tchar_t* /*src*/, const litehtml::tchar_t* /*baseurl*/,
					bool /*redraw_on_ready*/) override
	{
	}
	void get_image_size(const litehtml::tchar_t* /*src*/, const litehtml::tchar_t* /*baseurl*/,
						litehtml::size& /*sz*/) override
	{
	}
	void draw_background(litehtml::uint_ptr /*hdc*/, const litehtml::background_paint& bg) override
	{
		video_ctrl::rect r = {bg.clip_box.x, bg.clip_box.y, bg.clip_box.width, bg.clip_box.height};
		video_ctrl::color col{bg.color.red, bg.color.green, bg.color.blue, bg.color.alpha};
		list_.add_rect(r, col);
	}
	void draw_borders(litehtml::uint_ptr /*hdc*/, const litehtml::borders& /*borders*/,
					  const litehtml::position& /*draw_pos*/, bool /*root*/) override
	{
	}

	void set_caption(const litehtml::tchar_t* /*caption*/) override
	{
	}
	void set_base_url(const litehtml::tchar_t* /*base_url*/) override
	{
	}
	void link(const std::shared_ptr<litehtml::document>& /*doc*/,
			  const litehtml::element::ptr& /*el*/) override
	{
	}
	void on_anchor_click(const litehtml::tchar_t* /*url*/, const litehtml::element::ptr& /*el*/) override
	{
	}
	void set_cursor(const litehtml::tchar_t* /*cursor*/) override
	{
	}
	void transform_text(litehtml::tstring& /*text*/, litehtml::text_transform /*tt*/) override
	{
	}
	void import_css(litehtml::tstring& /*text*/, const litehtml::tstring& /*url*/,
					litehtml::tstring& /*baseurl*/) override
	{
	}
	void set_clip(const litehtml::position& /*pos*/, const litehtml::border_radiuses& /*bdr_radius*/,
				  bool /*valid_x*/, bool /*valid_y*/) override
	{
	}
	void del_clip() override
	{
	}
	void get_client_rect(litehtml::position& client) const override
	{
		const auto& r = rend_.get_rect();
		client.x = r.x;
		client.y = r.y;
		client.width = r.w;
		client.height = r.h;
	}
	std::shared_ptr<litehtml::element>
	create_element(const litehtml::tchar_t* /*tag_name*/, const litehtml::string_map& /*attributes*/,
				   const std::shared_ptr<litehtml::document>& /*doc*/) override
	{
		return {};
	}

	void get_media_features(litehtml::media_features& media) const override
	{
		litehtml::position client;
		get_client_rect(client);
		media.type = litehtml::media_type_screen;
		media.width = client.width;
		media.height = client.height;
		media.device_width = rend_.get_rect().w;
		media.device_height = rend_.get_rect().h;
		media.color = 8;
		media.monochrome = 0;
		media.color_index = 256;
		media.resolution = 96;
	}
	void get_language(litehtml::tstring& language, litehtml::tstring& culture) const override
	{
		language = _t("en");
		culture = _t("");
	}
};

int main()
{

display_text =
R"(
<!DOCTYPE html>
<html>
<body>

<h1 style="color:Tomato;">Hello World</h3>

<p style="color:DodgerBlue;">Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh euismod tincidunt ut laoreet dolore magna aliquam erat volutpat.</p>

<p style="color:MediumSeaGreen;">Ut wisi enim ad minim veniam, quis nostrud exerci tation ullamcorper suscipit lobortis nisl ut aliquip ex ea commodo consequat.</p>

</body>
</html>

)";

	struct video_window
	{
		std::unique_ptr<os::window> window{};
		std::unique_ptr<video_ctrl::renderer> renderer{};
	};

	os::init();
	video_ctrl::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });

	os::window master_win("master", os::window::centered, os::window::centered, 128, 128, os::window::hidden);
	video_ctrl::renderer master_rend(master_win, true);

	litehtml::context html_context;
    html_context.load_master_stylesheet(master_css);
	container_videopp container(master_rend);

	auto doc = litehtml::document::createFromUTF8(display_text.c_str(), &container, &html_context);

	{
		std::vector<video_window> windows{};

		for(int i = 0; i < 1; ++i)
		{
			windows.emplace_back();
			auto& win = windows.back();
			auto centerd = os::window::centered;
			auto flags = os::window::resizable;
			win.window =
				std::make_unique<os::window>("win" + std::to_string(i), centerd, centerd, 1366, 768, flags);
			win.renderer = std::make_unique<video_ctrl::renderer>(*win.window, false);
		}
		video_ctrl::glyphs_builder builder;
		builder.add(video_ctrl::get_latin_glyph_range());
		builder.add(video_ctrl::get_cyrillic_glyph_range());

		auto font_path = DATA "/FreeSansBold.ttf";
		auto font_path2 = DATA "/FreeSans.ttf";

		auto font =
			master_rend.create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), 50, 2, true));
		auto font2 =
			master_rend.create_font(video_ctrl::create_font_from_ttf(font_path2, builder.get(), 50, 2, true));

		auto font_bitmap =
			master_rend.create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), 50, 0, true));

		std::unordered_map<std::string, video_ctrl::font_ptr> rss_mgr = {{"FONT_BOLD_KEY", font},
																		 {"FONT_KEY", font2}};

		auto parser = std::make_shared<video_ctrl::simple_html_parser>();
		parser->register_type("plain", [&](const auto& table, const std::string& text) {
			auto getter = [&](const std::string& key) { return rss_mgr[key]; };

			return std::make_shared<video_ctrl::text_element>(table, text, getter);
		});

		parser->register_type("img", [](const auto& table, const std::string&) {
			return std::make_shared<video_ctrl::image_element>(table, false);
		});
		parser->register_type("body", [](const auto& table, const std::string&) {
			return std::make_shared<video_ctrl::image_element>(table, true);
		});
		video_ctrl::rich_text t;
		t.set_parser(parser);
		t.set_line_gap(4.0f);

		t.set_utf8_text(R"(
        <body>
        <text color=#000000 font=FONT_BOLD_KEY>Your <text color=darkkhaki font=FONT_KEY>winning</text> figure is <img color=#ff00ff width=200 height=100 /> Congratulations.</text>
        <br>
        <text color=#000000 font=FONT_KEY>Твоята <text color=#00ff00 font=FONT_BOLD_KEY>печеливша</text> фигура е <img color=palevioletred width=200 height=100 /> Поздравления.</text>
        <br>
        <text color=#000000 font=FONT_KEY>Your <text color=#ff00ff font=FONT_BOLD_KEY>winning</text> figure is <img color=mediumseagreen width=200 height=100 /> Congratulations.</text>
        <br>
        <text color=salmon font=FONT_KEY>Your <text color=#0000ff font=FONT_BOLD_KEY>winning</text> figure is <img color=#0000ff width=200 height=100 /> Congratulations.</text>
        </body>
        )");

		video_ctrl::text::alignment align{video_ctrl::text::alignment::baseline_top};
		math::transformf transform;
		int num = 100;

		bool running = true;
		bool use_kerning = false;
		float outline_width = 0.0f;
		float target_scale = 1.0f;
		bool use_sdf = true;
		bool debug = false;
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

						windows.erase(std::remove_if(std::begin(windows), std::end(windows),
													 [=](const auto& w) {
														 return e.window.window_id == w.window->get_id();
													 }),
									  std::end(windows));

						if(windows.empty())
						{
							std::cout << "quit (all windows were closed)" << std::endl;
							running = false;
						}
					}
				}

				if(e.type == os::events::mouse_wheel)
				{
					if(os::key::is_pressed(os::key::lctrl))
					{
						float scale = float(e.wheel.y) * 0.01f;
						outline_width += scale;
					}
					else
					{
						float scale = float(e.wheel.y) * 0.1f;
						target_scale += scale;
					}
				}
				if(e.type == os::events::text_input)
				{
					display_text += e.text.text;
				}
				if(e.type == os::events::key_down)
				{
					if(e.key.code == os::key::backspace)
					{
						if(!display_text.empty())
						{
							display_text.pop_back();
						}
						break;
					}

					if(e.key.code == os::key::tab)
					{
						display_text += "\t";
						break;
					}
					if(e.key.code == os::key::lctrl)
					{
						num += 11;
						break;
					}

					if(e.key.code == os::key::enter)
					{
						if(e.key.shift)
						{
							debug = !debug;
						}
						else if(e.key.ctrl)
						{
							use_kerning = !use_kerning;
						}
						else if(e.key.alt)
						{
							use_sdf = !use_sdf;
						}
						else
						{
							auto i = uint32_t(align);
							auto cnt = uint32_t(video_ctrl::text::alignment::count);
							++i;
							i %= cnt;
							align = video_ctrl::text::alignment(i);
						}
					}
				}
			}

			using namespace std::chrono_literals;
			auto start = std::chrono::high_resolution_clock::now();

			for(const auto& window : windows)
			{
				const auto& win = *window.window;
				auto& rend = *window.renderer;
				rend.clear(video_ctrl::color::white());

				auto pos = os::mouse::get_position(win);
				transform.set_position(pos.x, pos.y, 0.0f);

				//                video_ctrl::draw_list list;

				//                list.add_text(t, transform);

				//                video_ctrl::text text;
				//                text.set_font(use_sdf ? font : font_bitmap);
				//                text.set_color(video_ctrl::color::black());
				//                text.set_outline_color(video_ctrl::color::magenta());
				//                text.set_utf8_text(display_text);
				//                text.set_alignment(align);
				//                text.set_outline_width(outline_width);
				//                text.set_kerning(use_kerning);

				//                //for(int i = 0; i < 30; ++i)
				//                {
				////                    list.add_text(text, transform);
				//                }

				//                if(debug)
				//                {
				//                    list.add_text_debug_info(text, transform);
				//                }

				doc->render(rend.get_rect().w);
				doc->draw({}, 0, 0, nullptr);

				rend.draw_cmd_list(container.list_);
				container.list_ = {};
				rend.present();
			}
			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);

			auto scale = transform.get_scale().x;
			scale = std::max(0.05f, math::lerp(scale, target_scale, dur.count()));
			transform.set_scale(scale, scale, 1.0f);

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
