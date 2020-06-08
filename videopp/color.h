#pragma once

#include <cstdint>
#include <functional>
#include "utils.h"

namespace gfx
{
    /// Common 32bit RGBA color structure                                               
    struct color
    {
        uint8_t r = 0, g = 0, b = 0, a = 255;
        
        constexpr color() noexcept = default;
        constexpr color(uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa = 0xff) noexcept 
            : r(rr)
            , g(gg)
            , b(bb)
            , a(aa) {}

        bool operator == (const color &rhs) const noexcept;
        bool operator != (const color &rhs) const noexcept;

        inline operator uint32_t() const noexcept
        {
            return ((uint32_t(a)<< 24) | (uint32_t(b)<<16) | (uint32_t(g)<<8) | (uint32_t(r)<<0));
        }

        // gray/black colors
        static const color& black();
        static const color& state_gray();
        static const color& gray();
        static const color& silver();
        static const color& gainsboro();

        // white colors
        static const color& white();
        static const color& snow();
        static const color& ghost_while();
        static const color& ivory();
        static const color& linen();

        // purple/violet/magenta colors
        static const color& lavender();
        static const color& thistle();
        static const color& violet();
        static const color& orchid();
        static const color& magenta();
        static const color& purple();
        static const color& indigo();

        // blue colors
        static const color& navy();
        static const color& blue();
        static const color& royal_blue();
        static const color& sky_blue();

        // cyan colors
        static const color& teal();
        static const color& turquoise();
        static const color& aquamarine();
        static const color& cyan();

        //green colors
        static const color& green();
        static const color& lime();
        static const color& olive();

        //brown colors
        static const color& maroon();
        static const color& brown();
        static const color& sienna();
        static const color& cholocate();
        static const color& peru();
        static const color& goldenrod();
        static const color& tan();
        static const color& wheat();

        //yellow colors
        static const color& gold();
        static const color& yellow();

        //orange colors
        static const color& orange();
        static const color& coral();
        static const color& tomato();

        //red colors
        static const color& red();
        static const color& crimson();
        static const color& salmon();

        //pink colors
        static const color& pink();

        //transperat color
        static const color& clear();

        color& operator *= (const color& rhs) noexcept;
    };
    
    /// Blending modes                                                                  
    enum blending_mode
    {
        // Disable blending                                                     
        blend_none = 0,
        // RGB = sourceRGB * sourceALPHA + destinationRGB * (1 - sourceALPHA)   
        // ALPHA = sourceALPHA * 1 + destinationALPHA * (1 - sourceALPHA)		
        blend_normal,
        // RGB = sourceRGB * 1 + destinationRGB * 1                             
        // ALPHA = sourceALPHA * 1 + destinationALPHA * 1                       
        blend_add,
        // RGB = max(sourceRGB, destinationRGB)                                 
        // ALPHA = max(sourceALPHA, destinationALPHA)                           
        blend_lighten,
        // RGB = 1 - (1 - sourceRGB) * (1 - destinationRGB)						
        blend_screen,
        // RGBA = r * a, g * a, b * a, a
        pre_multiplication,

        unmultiplied_alpha,
        
        // Leave this at the end                                                
        blending_mode_counter
    };
}

namespace std
{
    template<> struct hash<gfx::color>
    {
        using argument_type = gfx::color;
        using result_type = std::size_t;
        result_type operator()(argument_type const& s) const noexcept
        {
            uint64_t seed{0};
            utils::hash(seed, s.r, s.g, s.b, s.a);
            return seed;
        }
    };
}
