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
    {

        std::vector<video_window> windows{};

        for(int i = 0; i < 2; ++i)
        {
            windows.emplace_back();
            auto& win = windows.back();
            auto centerd = os::window::centered;
            win.window = std::make_unique<os::window>("win" + std::to_string(i), centerd, centerd, 1366, 768, os::window::resizable);
            win.renderer = std::make_unique<video_ctrl::renderer>(*win.window, true);
        }
        video_ctrl::glyphs_builder builder;
        builder.add(video_ctrl::get_latin_glyph_range());
        //ile:///C:/WINDOWS/Fonts/TT1018M_.TTF
        //file:///C:/WINDOWS/Fonts/DFHEIA1.TTFfile:///D:/Workspace/lato/Lato-Bold.ttf
        auto font_path = R"(D:/Workspace/dejavu-sans-mono/ttf/DejaVuSansMono-Bold.ttf)";
        auto font = windows.at(0).renderer->create_font(video_ctrl::create_font_from_ttf(font_path, builder.get(), 60, 2));

        std::string display_text{"123"};
        video_ctrl::text::alignment align{ video_ctrl::text::alignment::baseline_top};
        math::transformf transform;
        int num = 100;

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
                        if(windows.size() == 1)
                        {
                            font.reset();
                        }

                        windows.erase(std::remove_if(std::begin(windows), std::end(windows),
                                                     [=](const auto& w)
                        {
                            return e.window.window_id == w.window->get_id();
                        }), std::end(windows));
                    }
                }

                if(e.type == os::events::mouse_wheel)
                {
                    float scale = float(e.wheel.y) * 0.1f;
                    transform.scale(1.0f + scale, 1.0f + scale, 1.0f);
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
                            display_text += "\n";
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
                    display_text += e.text.text;
                }
            }

            for(const auto& window : windows)
            {
                const auto& win = *window.window;
                auto& rend = *window.renderer;

                auto pos = os::mouse::get_position(win);
                transform.set_position(pos.x, pos.y, 0.0f);

                using namespace std::chrono_literals;

                rend.clear(video_ctrl::color::gray());

                video_ctrl::draw_list list;

                video_ctrl::text text;
                text.set_font(font);
                text.set_utf8_text(display_text + std::to_string(num));
                text.set_alignment(align);
                //text.set_outline_width(0.2f);
                //list.add_text_superscript(text, text, transform, align);
                list.add_text(text, transform);
                list.add_text_debug_info(text, transform);

                rend.draw_cmd_list(list);

                rend.present();
            }

        }
    }
	os::shutdown();

	return 0;
}
