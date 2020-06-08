#include <ospp/os.h>
#include <videopp/renderer.h>
#include <videopp/ttf_font.h>

#include <iostream>

int main()
{

	os::init();
	gfx::set_extern_logger([](const std::string& msg) { std::cout << msg << std::endl; });


	{
		os::window win("win", os::window::centered, os::window::centered, 1366, 768, os::window::resizable);
		gfx::renderer rend(win, true);


        gfx::glyphs_builder builder;
        builder.add(gfx::get_all_glyph_range());
		auto info = gfx::create_font_from_ttf(DATA"fonts/wds052801.ttf", builder.get(), 100);
		auto font = rend.create_font(std::move(info));


        int size = int(font->size);
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
				if(e.type == os::events::mouse_wheel)
				{
                    size += int(e.wheel.y) * 5;
				}

			}

            rend.clear(gfx::color::gray());

            gfx::text text{};
            text.set_font(font, size);
            text.set_utf8_text("Hello World!");
            text.set_outline_width(0.3f);
            text.set_outline_color(gfx::color::red());
            text.set_alignment(gfx::align::center | gfx::align::middle);

            rend.get_list().add_text(text, gfx::align_wrap_and_fit_text(text, {}, rend.get_rect()));

			rend.present();
		}
	}
	os::shutdown();

	return 0;
}
