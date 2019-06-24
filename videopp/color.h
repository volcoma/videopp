#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>

namespace utils
{

template <class T>
inline void hash(uint64_t& seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= static_cast<uint64_t>(hasher(v)) + 0x9e3779b967dd4acc + (seed << 6) + (seed >> 2);
}

template <typename Arg0, typename Arg1, typename... Args>
inline void hash(uint64_t& seed, Arg0&& arg0, Arg1&& arg1, Args&& ...args)
{

    hash(seed, std::forward<Arg0>(arg0));
    hash(seed, std::forward<Arg1>(arg1), std::forward<Args>(args)...);
}

}


namespace video_ctrl
{
    struct exception : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
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

        static const color black();
        static const color blue();
        static const color cyan();
        static const color gray();
        static const color green();
        static const color magenta();
        static const color red();
        static const color yellow();
        static const color white();

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
    template<> struct hash<video_ctrl::color>
    {
        using argument_type = video_ctrl::color;
        using result_type = std::size_t;
        result_type operator()(argument_type const& s) const noexcept
        {
            uint64_t seed{0};
            utils::hash(seed, s.r, s.g, s.b, s.a);
            return seed;
        }
    };
}
