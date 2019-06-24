#include <ospp/os.h>
#include <videopp/renderer.h>
#include <videopp/ttf_font.h>
#include <videopp/ttf_font.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

int main()
{
    struct video_window
    {
        std::unique_ptr<os::window> window{};
        std::unique_ptr<video_ctrl::renderer> renderer{};
    };

	os::init();
    video_ctrl::set_extern_logger([](const std::string& msg)
    {
        std::cout << msg << std::endl;
    });

    os::window master_win("master", os::window::centered, os::window::centered, 128, 128, os::window::hidden);
    video_ctrl::renderer master_rend(master_win, true);
    {
        std::vector<video_window> windows{};


        for(int i = 0; i < 1; ++i)
        {
            windows.emplace_back();
            auto& win = windows.back();
            auto centerd = os::window::centered;
            auto flags = os::window::resizable;
            win.window = std::make_unique<os::window>("win" + std::to_string(i), centerd, centerd, 1366, 768, flags);
            win.renderer = std::make_unique<video_ctrl::renderer>(*win.window, false);
        }
        video_ctrl::glyphs_builder builder;
        builder.add(video_ctrl::get_latin_glyph_range());

#ifdef _WIN32
        auto font_path = R"(D:/wds052801.ttf)";
        //auto font_path = R"(C:/Windows/Fonts/Arial.ttf)";
#else
        auto font_path = R"(/home/default/Downloads/wds052801.ttf)";
#endif
        auto font = windows.at(0).renderer->create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), 50, 2));
        auto font2 = windows.at(0).renderer->create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), 50));

        std::string display_text =
R"(
$2 Choosing the optimal platform
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
therefore not recommended if portability is important. This manual does not cover graphics processors.
)";
        video_ctrl::text::alignment align{ video_ctrl::text::alignment::baseline_top};
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
                                                     [=](const auto& w)
                        {
                            return e.window.window_id == w.window->get_id();
                        }), std::end(windows));

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

                if(e.type == os::events::text_input)
                {
                    //display_text += e.text.text;
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

                video_ctrl::draw_list list;
                video_ctrl::text text;
                text.set_font(use_sdf ? font : font2);
                text.set_color(video_ctrl::color::black());
                text.set_outline_color(video_ctrl::color::magenta());
                text.set_utf8_text(display_text);
                text.set_alignment(align);
                text.set_outline_width(outline_width);
                text.use_kerning = use_kerning;
                list.add_text(text, transform);
                if(debug)
                {
                    list.add_text_debug_info(text, transform);
                }

                rend.draw_cmd_list(list);
                rend.present();
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);

            auto scale = transform.get_scale().x;
            scale = std::max(0.05f, math::lerp(scale, target_scale, dur.count()));
            transform.set_scale(scale, scale, 1.0f);


            static decltype(dur) avg_dur{};
            static int frame = 0;
            frame++;
            if(frame > 1000)
            {
                static int count = 0;

                if(count > 2000)
                {
                    count = 0;
                    avg_dur = {};
                }
                count++;
                avg_dur += dur;
                std::cout << avg_dur.count() / count << std::endl;
            }
        }
    }
	os::shutdown();

	return 0;
}
