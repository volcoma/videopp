#pragma once
#include "../draw_cmd.h"

namespace gfx
{


static constexpr const char* glsl_version =
                #if defined(GLX_CONTEXT) || defined(WGL_CONTEXT)
                    "#version 130\n";
                    //"#version 330 core\n";
                #elif defined(EGL_CONTEXT)
                    "#version 100\n";
//                    "#version 300 es\n";
                #else
                R"()";
                #endif

constexpr const char* glsl_derivatives =
                #if defined(GLX_CONTEXT) || defined(WGL_CONTEXT)
                R"(
                    #define HAS_DERIVATIVES
                )";
                #elif defined(EGL_CONTEXT)
                R"(
                    #ifdef GL_OES_standard_derivatives
                        #define HAS_DERIVATIVES
                        #extension GL_OES_standard_derivatives : enable
                    #endif
                )";
                #else
                R"()";
                #endif

static constexpr const char* glsl_precision =
                R"(
                    precision mediump float;
                )";

static constexpr const char* user_defines =
                R"(
                    #define HAS_CROP_RECTS
                )";

static constexpr const char* common_funcs =
                R"(
                    vec4 texture2DArrayIdx(sampler2D textures[32], float tex_index, vec2 tex_coords)
                    {
                        vec4 output_color = vec4(0.0, 1.0, 0.0, 1.0);

                        switch(int(tex_index))
                        {
                            case 0:  output_color = texture2D(textures[0],  tex_coords); break;
                            case 1:  output_color = texture2D(textures[1],  tex_coords); break;
                            case 2:  output_color = texture2D(textures[2],  tex_coords); break;
                            case 3:  output_color = texture2D(textures[3],  tex_coords); break;
                            case 4:  output_color = texture2D(textures[4],  tex_coords); break;
                            case 5:  output_color = texture2D(textures[5],  tex_coords); break;
                            case 6:  output_color = texture2D(textures[6],  tex_coords); break;
                            case 7:  output_color = texture2D(textures[7],  tex_coords); break;
                            case 8:  output_color = texture2D(textures[8],  tex_coords); break;
                            case 9:  output_color = texture2D(textures[9],  tex_coords); break;
                            case 10: output_color = texture2D(textures[10], tex_coords); break;
                            case 11: output_color = texture2D(textures[11], tex_coords); break;
                            case 12: output_color = texture2D(textures[12], tex_coords); break;
                            case 13: output_color = texture2D(textures[13], tex_coords); break;
                            case 14: output_color = texture2D(textures[14], tex_coords); break;
                            case 15: output_color = texture2D(textures[15], tex_coords); break;
                            case 16: output_color = texture2D(textures[16], tex_coords); break;
                            case 17: output_color = texture2D(textures[17], tex_coords); break;
                            case 18: output_color = texture2D(textures[18], tex_coords); break;
                            case 19: output_color = texture2D(textures[19], tex_coords); break;
                            case 20: output_color = texture2D(textures[20], tex_coords); break;
                            case 21: output_color = texture2D(textures[21], tex_coords); break;
                            case 22: output_color = texture2D(textures[22], tex_coords); break;
                            case 23: output_color = texture2D(textures[23], tex_coords); break;
                            case 24: output_color = texture2D(textures[24], tex_coords); break;
                            case 25: output_color = texture2D(textures[25], tex_coords); break;
                            case 26: output_color = texture2D(textures[26], tex_coords); break;
                            case 27: output_color = texture2D(textures[27], tex_coords); break;
                            case 28: output_color = texture2D(textures[28], tex_coords); break;
                            case 29: output_color = texture2D(textures[29], tex_coords); break;
                            case 30: output_color = texture2D(textures[30], tex_coords); break;
                            case 31: output_color = texture2D(textures[31], tex_coords); break;
                        }

                        return output_color;
                    }

                )";

static constexpr const char* vs_simple =
                R"(
                    in vec2 aPosition;
                    in vec2 aTexCoord;
                    in vec4 aColor;
                    in vec4 aExtraColor;
                    in vec2 aExtraData;
                    in float aTexIndex;

                    uniform mat4 uProjection;

                    out vec2 vTexCoord;
                    out vec4 vColor;
                    out vec4 vExtraColor;
                    out vec2 vExtraData;
                    out float vTexIndex;
                    void main()
                    {
                        gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
                        vTexCoord = aTexCoord;
                        vColor = aColor;
                        vExtraColor = aExtraColor;
                        vExtraData = aExtraData;
                        vTexIndex = aTexIndex;
                    }
                )";

static constexpr const char* fs_simple =
                R"(
                    in vec4 vColor;
                    out vec4 FragColor;
                    void main() {
                        FragColor = vColor;
                    }
                )";

static constexpr const char* fs_multi_channel =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in float vTexIndex;

                    out vec4 FragColor;

                    uniform sampler2D uTextures[32];
                #ifdef HAS_CROP_RECTS
                    uniform ivec4 uRects[10];
                    uniform int uRectsCount;
                #endif

                    void main()
                    {
                #ifdef HAS_CROP_RECTS
                        for( int i = 0; i < uRectsCount; ++i)
                        {
                            ivec4 irect = uRects[i];
                            vec4 rect = vec4(irect.x, irect.y, irect.z, irect.w);
                            if(gl_FragCoord.x > rect.x &&
                               gl_FragCoord.x < (rect.x + rect.z) &&
                               gl_FragCoord.y > rect.y &&
                               gl_FragCoord.y < (rect.y + rect.w) )
                            {
                                FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
                        }
                #endif
                        vec4 texcol = texture2DArrayIdx(uTextures, vTexIndex, vTexCoord.xy);
                        FragColor = texcol * vColor;
                    }
                )";



static constexpr const char* fs_single_channel =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in float vTexIndex;
                    out vec4 FragColor;

                    uniform sampler2D uTextures[32];
                #ifdef HAS_CROP_RECTS
                    uniform ivec4 uRects[10];
                    uniform int uRectsCount;
                #endif

                    void main()
                    {

                    #ifdef HAS_CROP_RECTS
                        for( int i = 0; i < uRectsCount; ++i)
                        {
                            ivec4 irect = uRects[i];
                            vec4 rect = vec4(irect.x, irect.y, irect.z, irect.w);
                            if(gl_FragCoord.x > rect.x &&
                               gl_FragCoord.x < (rect.x + rect.z) &&
                               gl_FragCoord.y > rect.y &&
                               gl_FragCoord.y < (rect.y + rect.w) )
                            {
                                FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
                        }
                    #endif

                        vec4 texcol = texture2DArrayIdx(uTextures, vTexIndex, vTexCoord.xy);
                        float alpha = texcol.r;
                        FragColor = vec4(vColor.rgb, vColor.a * alpha);
                    }
                )";


static constexpr const char* fs_raw_alpha =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in float vTexIndex;
                    out vec4 FragColor;
                    uniform sampler2D uTextures[32];

                    void main()
                    {
                        vec4 texcol = texture2DArrayIdx(uTextures, vTexIndex, vTexCoord.xy);

                        FragColor = texcol * vColor;
                        FragColor.rgb /= FragColor.a;
                    }
                )";

static constexpr const char* fs_grayscale =
                R"(
                    in vec2 vTexCoord;
                    in float vTexIndex;
                    out vec4 FragColor;
                    uniform sampler2D uTextures[32];

                    void main()
                    {
                        vec4 texcol = texture2DArrayIdx(uTextures, vTexIndex, vTexCoord.xy);

                        vec4 color = vec4(0.21, 0.72, 0.07, 1) * texcol;
                        float luminosity = color.r + color.g + color.b;
                        FragColor = vec4(luminosity,luminosity,luminosity,color.a);
                    }
                )";

static constexpr const char* fs_alphamix =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;

                    out vec4 FragColor;

                    uniform sampler2D uTextureRGB;
                    uniform sampler2D uTextureAlpha;

                    void main()
                    {
                         vec3 color = texture2D(uTextureRGB, vTexCoord.xy).rgb;
                         float alpha = texture2D(uTextureAlpha, vTexCoord.xy).r;
                         FragColor = vec4(color/alpha, alpha) * vColor;
                    }
                )";

static constexpr const char* fs_valphamix =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in float vTexIndex;

                    out vec4 FragColor;

                    uniform sampler2D uTextures[32];


                    void main()
                    {
                         vec2 coord = vTexCoord.xy;

                         vec3 color = texture2DArrayIdx(uTextures, vTexIndex, coord).rgb;
                         coord.y += 0.5;
                         float alpha = texture2DArrayIdx(uTextures, vTexIndex, coord).r;
                         FragColor = vec4(color/alpha, alpha) * vColor;
                    }
                )";

static constexpr const char* fs_halphamix =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in float vTexIndex;
                    out vec4 FragColor;

                    uniform sampler2D uTextures[32];

                    void main()
                    {
                         vec2 coord = vTexCoord.xy;
                         vec3 color = texture2DArrayIdx(uTextures, vTexIndex, coord).rgb;
                         coord.x += 0.5;
                         float alpha = texture2DArrayIdx(uTextures, vTexIndex, coord).r;
                         FragColor = vec4(color/alpha, alpha) * vColor;
                    }
                )";

static constexpr const char* fs_distance_field =
    R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;
                    in vec4 vExtraColor;
                    in vec2 vExtraData;
                    in float vTexIndex;

                    out vec4 FragColor;

                    uniform sampler2D uTextures[32];

                #ifdef HAS_CROP_RECTS
                    uniform ivec4 uRects[10];
                    uniform int uRectsCount;
                #endif

                #define SUPERSAMPLE
                #define THRESHOLD 0.5
                #define WEIGHT 0.5
                #define SQRT2H 0.70710678118654757
                #define SMOOTHING 32.0

                    float derivative_width(in float dist)
                    {
                #ifdef HAS_DERIVATIVES
                        // fwidth helps keep outlines a constant width irrespective of scaling
                        // Stefan Gustavson's fwidth
                        float width = length(vec2(dFdx(dist), dFdy(dist))) * SQRT2H;
                #else
                        float width = SQRT2H / (SMOOTHING * gl_FragCoord.w);
                #endif
                        return width;
                    }

                    float contour( in float d, in float w )
                    {
                        return smoothstep(THRESHOLD - w, THRESHOLD + w, d);
                    }

                    float supersample( in float alpha, in vec4 box_samples, in float width)
                    {
                        float asum = contour( box_samples.x, width )
                                    + contour( box_samples.y, width )
                                    + contour( box_samples.z, width )
                                    + contour( box_samples.w, width );

                        float weight = WEIGHT;  // scale value to apply to neighbours
                        // weighted average, with 4 extra points
                        return (alpha + weight * asum) / (1.0 + 4.0 * weight);
                    }

                    float aastep_simple(in float dist, in float multiplier)
                    {
                        return (dist - THRESHOLD) * multiplier + THRESHOLD;
                    }

                    float aastep_supersample(in float dist, in vec4 box_samples)
                    {
                        float width = derivative_width(dist);
                        float alpha = contour( dist, width );
                        return supersample(alpha, box_samples, width);
                    }

                    float aastep(in float dist)
                    {
                        float width = derivative_width(dist);
                        float alpha = contour( dist, width );
                        return alpha;
                    }

                    void main()
                    {
                        vec4 master_color = vColor;
                        vec4 outline_color = vExtraColor;
                        float outline_width = clamp(vExtraData.x, 0.0, 1.0) * 0.4;
                        float softness = clamp(vExtraData.y, 0.0, 1.0);
                        vec2 uv = vTexCoord.xy;

                        float dist = texture2DArrayIdx(uTextures, vTexIndex, uv).r;

                        float odist = dist + outline_width;

                #if defined(HAS_DERIVATIVES) && defined(SUPERSAMPLE)
                        // Supersample, 4 extra points
                        float dscale = 0.354; // half of 1/sqrt2; you can play with this
                        vec2 duv = dscale * (dFdx(uv) + dFdy(uv));
                        vec4 box = vec4(uv-duv, uv+duv);
                        vec4 box_distances = vec4(
                            texture2DArrayIdx(uTextures, vTexIndex, box.xy).r,
                            texture2DArrayIdx(uTextures, vTexIndex, box.zw).r,
                            texture2DArrayIdx(uTextures, vTexIndex, box.xw).r,
                            texture2DArrayIdx(uTextures, vTexIndex, box.zy).r
                        );
                        vec4 obox_distances = box_distances + outline_width;

                        float alpha  = aastep_supersample(dist, box_distances);
                        float oalpha = aastep_supersample(odist, obox_distances);

                #else
                        float alpha  = aastep(dist);
                        float oalpha = aastep(odist);
                #endif

                        vec4 color = master_color;
                        vec4 ocolor = outline_color;

                        float glow = pow(pow(dist, 0.75) * 2.0, 2.0);
                        ocolor.a = mix(outline_color.a * oalpha, outline_color.a * glow, softness);

                        // Alpha blend foreground.
                        vec4 rcolor = mix(
                            color,
                            ocolor,
                            1.0 - alpha//clamp(1.0 - alpha, 0.0, 1.0)
                        );

                        // Master alpha.
                        //rcolor.a = clamp(rcolor.a, 0.0, 1.0);

                        // Done!
                        FragColor = rcolor;

                    #ifdef HAS_CROP_RECTS
                        for( int i = 0; i < uRectsCount; ++i)
                        {
                            ivec4 irect = uRects[i];
                            vec4 rect = vec4(irect.x, irect.y, irect.z, irect.w);
                            if(gl_FragCoord.x >= rect.x &&
                               gl_FragCoord.x < (rect.x + rect.z) &&
                               gl_FragCoord.y >= rect.y &&
                               gl_FragCoord.y < (rect.y + rect.w) )
                            {
                                FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
                        }
                    #endif

                    }
                )";

static constexpr const char* fs_blur =
                R"(
                    in vec2 vTexCoord;
                    in vec4 vColor;

                    out vec4 FragColor;

                    uniform sampler2D uTexture;
                    uniform vec2 uTextureSize;
                    uniform vec2 uDirection;

                    vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
                        vec4 color = vec4(0.0);
                        vec2 off1 = vec2(1.3333333333333333) * direction;
                        color += texture2D(image, uv) * 0.29411764705882354;
                        color += texture2D(image, uv + (off1 / resolution)) * 0.35294117647058826;
                        color += texture2D(image, uv - (off1 / resolution)) * 0.35294117647058826;
                        return color;
                    }

                    vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
                        vec4 color = vec4(0.0);
                        vec2 off1 = vec2(1.3846153846) * direction;
                        vec2 off2 = vec2(3.2307692308) * direction;
                        color += texture2D(image, uv) * 0.2270270270;
                        color += texture2D(image, uv + (off1 / resolution)) * 0.3162162162;
                        color += texture2D(image, uv - (off1 / resolution)) * 0.3162162162;
                        color += texture2D(image, uv + (off2 / resolution)) * 0.0702702703;
                        color += texture2D(image, uv - (off2 / resolution)) * 0.0702702703;
                        return color;
                    }

                    vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
                        vec4 color = vec4(0.0);
                        vec2 off1 = vec2(1.411764705882353) * direction;
                        vec2 off2 = vec2(3.2941176470588234) * direction;
                        vec2 off3 = vec2(5.176470588235294) * direction;
                        color += texture2D(image, uv) * 0.1964825501511404;
                        color += texture2D(image, uv + (off1 / resolution)) * 0.2969069646728344;
                        color += texture2D(image, uv - (off1 / resolution)) * 0.2969069646728344;
                        color += texture2D(image, uv + (off2 / resolution)) * 0.09447039785044732;
                        color += texture2D(image, uv - (off2 / resolution)) * 0.09447039785044732;
                        color += texture2D(image, uv + (off3 / resolution)) * 0.010381362401148057;
                        color += texture2D(image, uv - (off3 / resolution)) * 0.010381362401148057;
                        return color;
                    }
                    void main()
                    {
                        FragColor = blur5(uTexture, vTexCoord, uTextureSize, uDirection) * vColor;
                    }
                )";

}
