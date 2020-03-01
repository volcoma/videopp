#pragma once
#include "../draw_cmd.h"

namespace gfx
{


static constexpr const char* version =
                #if defined(GLX_CONTEXT) || defined(WGL_CONTEXT)
                R"(
                    #version 130
                )";
                #elif defined(EGL_CONTEXT)
                R"(
                    #version 100
                )";
                #endif

static constexpr const char* fs_derivatives =
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
                #endif


static constexpr const char* user_defines =
                R"(
                    #define HAS_CROP_RECTS;
                )";

static constexpr const char* vs_simple =
                R"(
                    attribute vec2 aPosition;
                    attribute vec2 aTexCoord;
                    attribute vec4 aColor;
                    attribute vec4 aExtraColor;
                    attribute vec2 aExtraData;

					uniform mat4 uProjection;

                    varying vec2 vTexCoord;
                    varying vec4 vColor;
                    varying vec4 vExtraColor;
                    varying vec2 vExtraData;

                    void main() {
						gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
                        vTexCoord = aTexCoord;
                        vColor = aColor;
                        vExtraColor = aExtraColor;
                        vExtraData = aExtraData;
                    }
                )";

static constexpr const char* fs_simple =
                R"(
                    precision mediump float;
                    varying vec4 vColor;

                    void main()
                    {
                        gl_FragColor = vColor;
                    }
                )";

static constexpr const char* fs_multi_channel =
                R"(
                    precision mediump float;
                    varying vec2 vTexCoord;
                    varying vec4 vColor;
                    uniform sampler2D uTexture;
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
                               gl_FragCoord.x < float(rect.x + rect.z) &&
                               gl_FragCoord.y > rect.y &&
                               gl_FragCoord.y < (rect.y + rect.w) )
                            {
                                gl_FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
						}
                #endif
                        gl_FragColor = texture2D(uTexture, vTexCoord.xy) * vColor;
                    }
                )";


static constexpr const char* fs_single_channel =
                R"(
                    precision mediump float;
                    varying vec2 vTexCoord;
                    varying vec4 vColor;

                    uniform sampler2D uTexture;
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
                                gl_FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
                        }
                    #endif

                        float alpha = texture2D(uTexture, vTexCoord.xy).r;
                        gl_FragColor = vec4(vColor.rgb, vColor.a * alpha);
                    }
                )";


static constexpr const char* fs_distance_field =
                R"(
                    precision mediump float;
                    varying vec2 vTexCoord;
                    varying vec4 vColor;

                    varying vec4 vExtraColor;
                    varying vec2 vExtraData;

                    uniform sampler2D uTexture;

                #ifdef HAS_CROP_RECTS
                    uniform ivec4 uRects[10];
                    uniform int uRectsCount;
                #endif

                #ifndef HAS_DERIVATIVES
                    uniform float uDFMultiplier;
                #else
                    float contour( in float d, in float w )
                    {
                        return smoothstep(0.5 - w, 0.5 + w, d);
                    }

                    float supersample( in float alpha, in vec4 box_samples, in float width)
                    {
                         float asum = contour( box_samples.x, width )
                                    + contour( box_samples.y, width )
                                    + contour( box_samples.z, width )
                                    + contour( box_samples.w, width );

//                         float weight = 0.5;  // scale value to apply to neighbours
//                         // weighted average, with 4 extra points
//                         return (alpha + weight * asum) / (1.0 + 4.0 * weight);

                        // Correct would dividing by 5 for calculate average, but
                        // when multisampling brightness is lost. Therefore divide by 3.
                        return (alpha + asum) * 0.3333;
                    }

                    float aastep(in float dist, in vec4 box_samples)
                    {
                        // fwidth helps keep outlines a constant width irrespective of scaling
                        // Stefan Gustavson's fwidth
                        float width = 0.7 * length(vec2(dFdx(dist), dFdy(dist)));
                        float alpha = contour( dist, width );
                        alpha = supersample(alpha, box_samples, width);
                        return alpha;
                    }
                #endif

                    void main()
                    {


                        vec4 master_color = vColor;
                        float master_alpha = master_color.a;
                        vec4 outline_color = vExtraColor;
                        float outline_width = clamp(vExtraData.x, 0.0, 0.5);
                        vec2 uv = vTexCoord.xy;
                        float dist = texture2D(uTexture, uv).r;

                    #ifdef HAS_DERIVATIVES

                        // Supersample, 4 extra points
						float dscale = 0.354; // half of 1/sqrt2; you can play with this
                        vec2 duv = dscale * (dFdx(uv) + dFdy(uv));
						vec4 box = vec4(uv-duv, uv+duv);
                        vec4 box_distances = vec4(
                            texture2D(uTexture, box.xy).r,
                            texture2D(uTexture, box.zw).r,
                            texture2D(uTexture, box.xw).r,
                            texture2D(uTexture, box.zy).r
                        );
                        float alpha = aastep(dist, box_distances);
                        vec4 color = vec4(master_color.rgb, alpha);

                        vec4 obox_distances = box_distances + outline_width;
                        float odist = dist + outline_width;
                        float oalpha = aastep(odist, obox_distances);
                        vec4 ocolor = vec4(outline_color.rgb, oalpha * outline_color.a);
                    #else
                        float multiplier = uDFMultiplier;
                        float alpha = (dist - 0.5) * multiplier + 0.5;
                        vec4 color = vec4(master_color.rgb, alpha);
                        float odist = dist + outline_width;
                        float oalpha = (odist - 0.5) * multiplier + 0.5;
                        vec4 ocolor = vec4(outline_color.rgb, oalpha * outline_color.a);
                    #endif
                        // Alpha blend foreground.
                        vec4 rcolor = mix(
                            color,
                            ocolor,
                            clamp(1.0 - alpha, 0.0, 1.0) * outline_color.a
                        );

                        // Master alpha.
                        rcolor.a = clamp(rcolor.a, 0.0, 1.0) * master_color.a;

                        // Done!
                        gl_FragColor = rcolor;

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
                                gl_FragColor = vec4(0.0,0.0,0.0,0.0);
                                return;
                            }
                        }
                    #endif

                    }
                )";

static constexpr const char* fs_blur =
                R"(
                    precision mediump float;
                    varying vec2 vTexCoord;
                    varying vec4 vColor;

                    uniform sampler2D uTexture;
                    uniform vec2 uTextureSize;
                    uniform vec2 uDirection;

                    vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
                    {
                        vec4 color = vec4(0.0);
                        vec2 off1 = vec2(1.3333333333333333) * direction;
                        color += texture2D(image, uv) * 0.29411764705882354;
                        color += texture2D(image, uv + (off1 / resolution)) * 0.35294117647058826;
                        color += texture2D(image, uv - (off1 / resolution)) * 0.35294117647058826;
                        return color;
                    }

                    vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
                    {
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

                    vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
                    {
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
                        gl_FragColor = blur5(uTexture, vTexCoord, uTextureSize, uDirection) * vColor;
                    }
                )";


static constexpr const char* fs_fxaa =
                R"(
                    precision mediump float;
                    varying vec2 vTexCoord;
                    varying vec4 vColor;

                    uniform sampler2D uTexture;
                    uniform vec2 uTextureSize;

                    /*
                    FXAA_PRESET - Choose compile-in knob preset 0-5.
                    ------------------------------------------------------------------------------
                    FXAA_EDGE_THRESHOLD - The minimum amount of local contrast required
                                         to apply algorithm.
                                         1.0/3.0  - too little
                                         1.0/4.0  - good start
                                         1.0/8.0  - applies to more edges
                                         1.0/16.0 - overkill
                    ------------------------------------------------------------------------------
                    FXAA_EDGE_THRESHOLD_MIN - Trims the algorithm from processing darks.
                                             Perf optimization.
                                             1.0/32.0 - visible limit (smaller isn't visible)
                                             1.0/16.0 - good compromise
                                             1.0/12.0 - upper limit (seeing artifacts)
                    ------------------------------------------------------------------------------
                    FXAA_SEARCH_STEPS - Maximum number of search steps for end of span.
                    ------------------------------------------------------------------------------
                    FXAA_SEARCH_THRESHOLD - Controls when to stop searching.
                                           1.0/4.0 - seems to be the best quality wise
                    ------------------------------------------------------------------------------
                    FXAA_SUBPIX_TRIM - Controls sub-pixel aliasing removal.
                                      1.0/2.0 - low removal
                                      1.0/3.0 - medium removal
                                      1.0/4.0 - default removal
                                      1.0/8.0 - high removal
                                      0.0 - complete removal
                    ------------------------------------------------------------------------------
                    FXAA_SUBPIX_CAP - Insures fine detail is not completely removed.
                                     This is important for the transition of sub-pixel detail,
                                     like fences and wires.
                                     3.0/4.0 - default (medium amount of filtering)
                                     7.0/8.0 - high amount of filtering
                                     1.0 - no capping of sub-pixel aliasing removal
                    */

                    #ifndef FXAA_PRESET
                       #define FXAA_PRESET 5
                    #endif
                    #if (FXAA_PRESET == 3)
                       #define FXAA_EDGE_THRESHOLD      (1.0/8.0)
                       #define FXAA_EDGE_THRESHOLD_MIN  (1.0/16.0)
                       #define FXAA_SEARCH_STEPS        16
                       #define FXAA_SEARCH_THRESHOLD    (1.0/4.0)
                       #define FXAA_SUBPIX_CAP          (3.0/4.0)
                       #define FXAA_SUBPIX_TRIM         (1.0/4.0)
                    #endif
                    #if (FXAA_PRESET == 4)
                       #define FXAA_EDGE_THRESHOLD      (1.0/8.0)
                       #define FXAA_EDGE_THRESHOLD_MIN  (1.0/24.0)
                       #define FXAA_SEARCH_STEPS        24
                       #define FXAA_SEARCH_THRESHOLD    (1.0/4.0)
                       #define FXAA_SUBPIX_CAP          (3.0/4.0)
                       #define FXAA_SUBPIX_TRIM         (1.0/4.0)
                    #endif
                    #if (FXAA_PRESET == 5)
                       #define FXAA_EDGE_THRESHOLD      (1.0/8.0)
                       #define FXAA_EDGE_THRESHOLD_MIN  (1.0/24.0)
                       #define FXAA_SEARCH_STEPS        32
                       #define FXAA_SEARCH_THRESHOLD    (1.0/4.0)
                       #define FXAA_SUBPIX_CAP          (3.0/4.0)
                       #define FXAA_SUBPIX_TRIM         (1.0/4.0)
                    #endif

                    #define FXAA_SUBPIX_TRIM_SCALE (1.0/(1.0 - FXAA_SUBPIX_TRIM))

                    // Return the luma, the estimation of luminance from rgb inputs.
                    // This approximates luma using one FMA instruction,
                    // skipping normalization and tossing out blue.
                    // FxaaLuma() will range 0.0 to 2.963210702.
                    float FxaaLuma(vec3 rgb) {
                       return rgb.y * (0.587/0.299) + rgb.x;
                    }

                    vec3 FxaaLerp3(vec3 a, vec3 b, float amountOfA) {
                       return (vec3(-amountOfA) * b) + ((a * vec3(amountOfA)) + b);
                    }

                    vec4 FxaaTexOff(sampler2D tex, vec2 pos, ivec2 off, vec2 rcpFrame) {
                       float x = pos.x + float(off.x) * rcpFrame.x;
                       float y = pos.y + float(off.y) * rcpFrame.y;
                       return texture2D(tex, vec2(x, y));
                    }

                    // pos is the output of FxaaVertexShader interpolated across screen.
                    // xy -> actual texture position {0.0 to 1.0}
                    // rcpFrame should be a uniform equal to  {1.0/frameWidth, 1.0/frameHeight}
                    vec3 FxaaPixelShader(vec2 pos, sampler2D tex, vec2 rcpFrame)
                    {
                       vec3 rgbN = FxaaTexOff(tex, pos.xy, ivec2( 0,-1), rcpFrame).xyz;
                       vec3 rgbW = FxaaTexOff(tex, pos.xy, ivec2(-1, 0), rcpFrame).xyz;
                       vec3 rgbM = FxaaTexOff(tex, pos.xy, ivec2( 0, 0), rcpFrame).xyz;
                       vec3 rgbE = FxaaTexOff(tex, pos.xy, ivec2( 1, 0), rcpFrame).xyz;
                       vec3 rgbS = FxaaTexOff(tex, pos.xy, ivec2( 0, 1), rcpFrame).xyz;

                       float lumaN = FxaaLuma(rgbN);
                       float lumaW = FxaaLuma(rgbW);
                       float lumaM = FxaaLuma(rgbM);
                       float lumaE = FxaaLuma(rgbE);
                       float lumaS = FxaaLuma(rgbS);
                       float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
                       float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));

                       float range = rangeMax - rangeMin;
                       if(range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
                       {
                           return rgbM;
                       }

                       vec3 rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;

                       float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25;
                       float rangeL = abs(lumaL - lumaM);
                       float blendL = max(0.0, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
                       blendL = min(FXAA_SUBPIX_CAP, blendL);

                       vec3 rgbNW = FxaaTexOff(tex, pos.xy, ivec2(-1,-1), rcpFrame).xyz;
                       vec3 rgbNE = FxaaTexOff(tex, pos.xy, ivec2( 1,-1), rcpFrame).xyz;
                       vec3 rgbSW = FxaaTexOff(tex, pos.xy, ivec2(-1, 1), rcpFrame).xyz;
                       vec3 rgbSE = FxaaTexOff(tex, pos.xy, ivec2( 1, 1), rcpFrame).xyz;
                       rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
                       rgbL *= vec3(1.0/9.0);

                       float lumaNW = FxaaLuma(rgbNW);
                       float lumaNE = FxaaLuma(rgbNE);
                       float lumaSW = FxaaLuma(rgbSW);
                       float lumaSE = FxaaLuma(rgbSE);

                       float edgeVert =
                           abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
                           abs((0.50 * lumaW ) + (-1.0 * lumaM) + (0.50 * lumaE )) +
                           abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
                       float edgeHorz =
                           abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
                           abs((0.50 * lumaN ) + (-1.0 * lumaM) + (0.50 * lumaS )) +
                           abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

                       bool horzSpan = edgeHorz >= edgeVert;
                       float lengthSign = horzSpan ? -rcpFrame.y : -rcpFrame.x;

                       if(!horzSpan)
                       {
                           lumaN = lumaW;
                           lumaS = lumaE;
                       }

                       float gradientN = abs(lumaN - lumaM);
                       float gradientS = abs(lumaS - lumaM);
                       lumaN = (lumaN + lumaM) * 0.5;
                       lumaS = (lumaS + lumaM) * 0.5;

                       if (gradientN < gradientS)
                       {
                           lumaN = lumaS;
                           lumaN = lumaS;
                           gradientN = gradientS;
                           lengthSign *= -1.0;
                       }

                       vec2 posN;
                       posN.x = pos.x + (horzSpan ? 0.0 : lengthSign * 0.5);
                       posN.y = pos.y + (horzSpan ? lengthSign * 0.5 : 0.0);

                       gradientN *= FXAA_SEARCH_THRESHOLD;

                       vec2 posP = posN;
                       vec2 offNP = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);
                       float lumaEndN = lumaN;
                       float lumaEndP = lumaN;
                       bool doneN = false;
                       bool doneP = false;
                       posN += offNP * vec2(-1.0, -1.0);
                       posP += offNP * vec2( 1.0,  1.0);

                       for(int i = 0; i < FXAA_SEARCH_STEPS; i++) {
                           if(!doneN)
                           {
                               lumaEndN = FxaaLuma(texture2D(tex, posN.xy).xyz);
                           }
                           if(!doneP)
                           {
                               lumaEndP = FxaaLuma(texture2D(tex, posP.xy).xyz);
                           }

                           doneN = doneN || (abs(lumaEndN - lumaN) >= gradientN);
                           doneP = doneP || (abs(lumaEndP - lumaN) >= gradientN);

                           if(doneN && doneP)
                           {
                               break;
                           }
                           if(!doneN)
                           {
                               posN -= offNP;
                           }
                           if(!doneP)
                           {
                               posP += offNP;
                           }
                       }

                       float dstN = horzSpan ? pos.x - posN.x : pos.y - posN.y;
                       float dstP = horzSpan ? posP.x - pos.x : posP.y - pos.y;
                       bool directionN = dstN < dstP;
                       lumaEndN = directionN ? lumaEndN : lumaEndP;

                       if(((lumaM - lumaN) < 0.0) == ((lumaEndN - lumaN) < 0.0))
                       {
                           lengthSign = 0.0;
                       }


                       float spanLength = (dstP + dstN);
                       dstN = directionN ? dstN : dstP;
                       float subPixelOffset = (0.5 + (dstN * (-1.0/spanLength))) * lengthSign;
                       vec3 rgbF = texture2D(tex, vec2(
                           pos.x + (horzSpan ? 0.0 : subPixelOffset),
                           pos.y + (horzSpan ? subPixelOffset : 0.0))).xyz;
                       return FxaaLerp3(rgbL, rgbF, blendL);
                    }

                    void main()
                    {
                        vec4 SourceSize = vec4(uTextureSize, 1.0 / uTextureSize); //either TextureSize or InputSize
                        gl_FragColor = vec4(FxaaPixelShader(vTexCoord, uTexture, vec2(SourceSize.z, SourceSize.w)), 1.0) * 1.0;
                    }
                )";
}
