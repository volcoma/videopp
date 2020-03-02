#include "renderer.h"
#include "font.h"
#include "ttf_font.h"
#include "texture.h"
#include "detail/shaders.h"
#include "detail/utils.h"

#ifdef WGL_CONTEXT
#include "detail/wgl/context_wgl.h"
#elif GLX_CONTEXT
#include "detail/glx/context_glx.h"
#elif EGL_CONTEXT
#include "detail/egl/context_egl.h"
#else

#endif

namespace gfx
{
namespace
{
inline GLenum to_gl_primitive(primitive_type type)
{
	switch(type)
	{
		case primitive_type::lines:
			return GL_LINES;
		case primitive_type::lines_loop:
			return GL_LINE_LOOP;
		case primitive_type::triangle_fan:
			return GL_TRIANGLE_FAN;
		case primitive_type::triangle_strip:
			return GL_TRIANGLE_STRIP;
		default:
			return GL_TRIANGLES;
	}
}

constexpr float FARTHEST_Z = -1.0f;

// Callback function for printing debug statements
void APIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
							GLenum severity, GLsizei,
							const GLchar *msg, const void*)
{
	std::string source_str;
	std::string type_str;
	std::string severity_str;

	switch (source) {
		case GL_DEBUG_SOURCE_API:
		source_str = "api";
		break;

		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		source_str = "window system";
		break;

		case GL_DEBUG_SOURCE_SHADER_COMPILER:
		source_str = "shader compiler";
		break;

		case GL_DEBUG_SOURCE_THIRD_PARTY:
		source_str = "THIRD PARTY";
		break;

		case GL_DEBUG_SOURCE_APPLICATION:
		source_str = "application";
		break;

		case GL_DEBUG_SOURCE_OTHER:
		source_str = "unknown";
		break;

		default:
		source_str = "unknown";
		break;
	}

	switch (type) {
		case GL_DEBUG_TYPE_ERROR:
		type_str = "error";
		break;

		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		type_str = "deprecated behavior";
		break;

		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		type_str = "undefined behavior";
		break;

		case GL_DEBUG_TYPE_PORTABILITY:
		type_str = "portability";
		break;

		case GL_DEBUG_TYPE_PERFORMANCE:
		type_str = "performance";
		break;

		case GL_DEBUG_TYPE_OTHER:
		type_str = "other";
		break;

		case GL_DEBUG_TYPE_MARKER:
		type_str = "unknown";
		break;

		default:
		type_str = "unknown";
		break;
	}

	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
		severity_str = "high";
		break;

		case GL_DEBUG_SEVERITY_MEDIUM:
		severity_str = "medium";
		break;

		case GL_DEBUG_SEVERITY_LOW:
		severity_str = "low";
		break;

		case GL_DEBUG_SEVERITY_NOTIFICATION:
		severity_str = "info";
		break;

		default:
		severity_str = "unknown";
		break;
	}

	std::stringstream ss;
	ss << "--[OPENGL CALLBACK]--\n"
	   << "   source   : " << source_str << "\n"
	   << "   type     : " << type_str << "\n"
	   << "   severity : " << severity_str << "\n"
	   << "   id       : " << id << "\n"
	   << "   message  : " << msg << "\n";

	if(severity > GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		log(ss.str());
	}

	assert(type != GL_DEBUG_TYPE_ERROR);
}


}
/// Construct the renderer and initialize default rendering states
///	@param win - the window handle
///	@param vsync - true to enable vsync
renderer::renderer(os::window& win, bool vsync)
	: win_(win)
#ifdef WGL_CONTEXT
	, context_(std::make_unique<context_wgl>(win.get_native_handle()))
#elif GLX_CONTEXT
	, context_(std::make_unique<context_glx>(win.get_native_handle(), win.get_native_display()))
#elif EGL_CONTEXT
	, context_(std::make_unique<context_egl>(win.get_native_handle(), win.get_native_display()))
#endif
{
	context_->set_vsync(vsync);

	if(!gladLoadGL())
	{
		throw gfx::exception("Cannot load glad.");
	}


	// During init, enable debug output
	gl_call(glEnable(GL_DEBUG_OUTPUT));
	gl_call(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
	gl_call(glDebugMessageCallback(MessageCallback, nullptr));

	rect_.w = win_.get_size().w;
	rect_.h = win_.get_size().h;

	gl_call(glDisable(GL_DEPTH_TEST));
	gl_call(glDepthMask(GL_FALSE));

	// Default blending mode
	set_blending_mode(blending_mode::blend_normal);

	// Default texture interpolation methods
	gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

	// Default texture wrapping methods
	gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

	// Default vertex buffer budget. This is not static, so no worries
	for(auto& vbo : stream_vbos_)
	{
		vbo.create();
		vbo.bind();
		vbo.reserve(nullptr, sizeof(vertex_2d) * 1024, true);
		vbo.unbind();
	}

	// Default index buffer budget. This is not static, so no worries
	for(auto& ibo : stream_ibos_)
	{
		ibo.create();
		ibo.bind();
		ibo.reserve(nullptr, sizeof(draw_list::index_t) * 1024, true);
		ibo.unbind();
	}

	reset_transform();
	set_model_view(0, rect_);

	auto create_program = [&](auto& program, auto fs, auto vs)
	{
		if(!program.shader)
		{
			auto shader = create_shader(fs, vs);
			embedded_shaders_.emplace_back(shader);
			program.shader = shader.get();
			auto& layout = shader->get_layout();
			constexpr auto stride = sizeof(vertex_2d);
			layout.template add<float>(2, offsetof(vertex_2d, pos), "aPosition", stride);
			layout.template add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", stride);
			layout.template add<uint8_t>(4, offsetof(vertex_2d, col) , "aColor", stride, true);
			layout.template add<uint8_t>(4, offsetof(vertex_2d, extra_col) , "aExtraColor", stride, true);
			layout.template add<float>(2, offsetof(vertex_2d, extra_data), "aExtraData", stride);

//            layout.template add<uint8_t>(1, offsetof(vertex_2d, tex_idx), "aTexIdx", stride);
		}
	};

	GLint texture_units = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);

	create_program(get_program<programs::simple>(),
				   std::string(version).append(fs_simple).c_str(), std::string(version).append(vs_simple).c_str());

	create_program(get_program<programs::multi_channel>(),
				   std::string(version).append(fs_multi_channel).c_str(), std::string(version).append(vs_simple).c_str());
	create_program(get_program<programs::multi_channel_crop>(),
				   std::string(version).append(user_defines).append(fs_multi_channel).c_str(), std::string(version).append(vs_simple).c_str());

	create_program(get_program<programs::single_channel>(),
				   std::string(version).append(fs_single_channel).c_str(), std::string(version).append(vs_simple).c_str());
	create_program(get_program<programs::single_channel_crop>(),
				   std::string(version).append(user_defines).append(fs_single_channel).c_str(), std::string(version).append(vs_simple).c_str());

	create_program(get_program<programs::distance_field>(),
				   std::string(version).append(fs_derivatives).append(fs_distance_field).c_str(), std::string(version).append(vs_simple).c_str());
	create_program(get_program<programs::distance_field_crop>(),
				   std::string(version).append(fs_derivatives).append(user_defines).append(fs_distance_field).c_str(), std::string(version).append(vs_simple).c_str());

	create_program(get_program<programs::blur>(),
				   std::string(version).append(fs_blur).c_str(), std::string(version).append(vs_simple).c_str());
	create_program(get_program<programs::fxaa>(),
				   std::string(version).append(fs_fxaa).c_str(), std::string(version).append(vs_simple).c_str());
	if(!default_font())
	{
		default_font() = create_font(create_default_font(13, 2));
		embedded_fonts_.emplace_back(default_font());
	}

	setup_sampler(texture::wrap_type::clamp, texture::interpolation_type::linear);
	setup_sampler(texture::wrap_type::repeat, texture::interpolation_type::linear);
	setup_sampler(texture::wrap_type::mirror, texture::interpolation_type::linear);
	setup_sampler(texture::wrap_type::clamp, texture::interpolation_type::nearest);
	setup_sampler(texture::wrap_type::repeat, texture::interpolation_type::nearest);
	setup_sampler(texture::wrap_type::mirror, texture::interpolation_type::nearest);

	clear(color::black());
}

void renderer::setup_sampler(texture::wrap_type wrap, texture::interpolation_type interp) noexcept
{
	uint32_t sampler_state = 0;
	gl_call(glGenSamplers(1, &sampler_state));
	switch (wrap) {
	case texture::wrap_type::mirror:
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT));
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
		break;
	case texture::wrap_type::repeat:
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_REPEAT));
		break;
	case texture::wrap_type::clamp:
	default:
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		break;
	}

	switch (interp) {
	case texture::interpolation_type::nearest:
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		break;
	case texture::interpolation_type::linear:
	default:
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		break;
	}

	samplers_[size_t(wrap)][size_t(interp)] = sampler_state;
}

renderer::~renderer()
{
	embedded_fonts_.clear();
	embedded_shaders_.clear();

	auto clear_embedded = [](auto& shared)
	{
		if(shared.use_count() == 1)
		{
			shared.reset();
		}
	};

	clear_embedded(default_font());
}

/// Bind FBO, set projection, switch to modelview
///	@param fbo - framebuffer object, use 0 for the backbuffer
///	@param rect - the dirty area to draw into
void renderer::set_model_view(const uint32_t fbo, const rect& rect) const noexcept
{
	// Bind the FBO or backbuffer if 0
	gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

	// Set the viewport
	gl_call(glViewport(0, 0, rect.w, rect.h));

	// Set the projection
	if(fbo == 0)
	{
		// If we have 0 it is the back buffer
		current_ortho_ = math::ortho<float>(0.0f, float(rect.w), float(rect.h), 0.0f, 0.0f, FARTHEST_Z);
	}
	else
	{
		// If we have > 0 the fbo must be flipped
		current_ortho_ = math::ortho<float>(0.0f, float(rect.w), 0.0f, float(rect.h), 0.0f, FARTHEST_Z);
	}

	// Switch to modelview matrix and setup identity
	reset_transform();
}

void renderer::clear_fbo(uint32_t fbo_id, const color &color) const noexcept
{
	set_current_context();
	gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo_id));
	gl_call(glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
	gl_call(glClear(GL_COLOR_BUFFER_BIT));

	set_old_framebuffer();
}

/// Bind this renderer & window
///	@return true on success
bool renderer::set_current_context() const noexcept
{
	return context_->make_current();
}

///
void renderer::set_old_framebuffer() const noexcept
{
	if (fbo_stack_.empty())
	{
		gl_call(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		return;
	}

	const auto& top_texture = fbo_stack_.top();
	gl_call(glBindFramebuffer(GL_FRAMEBUFFER, top_texture->get_fbo()));
}

/// Resize the dirty rectangle
/// @param new_width - the new width
/// @param new_height - the new height
void renderer::resize(uint32_t new_width, uint32_t new_height) noexcept
{
	rect_.w = new_width;
	rect_.h = new_height;
}

/// Create a texture from a surface
///	@param surface - surface to extract data from
///	@param allocatorName - allocator name
///	@param fileName - file name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(const surface& surface) const noexcept
{
	if(!set_current_context())
	{
		return {};
	}

	try
	{
		auto tex = new texture(*this, surface);
		return texture_ptr(tex);
	}
	catch(const std::exception& e)
	{
		log(std::string("ERROR: Cannot create texture from surface. Reason ") + e.what());
	}

	return texture_ptr(nullptr);
}

/// Create a texture from a filename
///	@param file_name - file name
///	@param allocatorName - allocator name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(const std::string& file_name) const noexcept
{
	if(!set_current_context())
	{
		return {};
	}

	try
	{
		auto tex = new texture(*this);
		texture_ptr texture(tex);

		if(!texture->load_from_file(file_name))
		{
			return texture_ptr();
		}

		return texture;
	}
	catch(const std::exception& e)
	{
		log(std::string("ERROR: Cannot create texture from file. Reason ") + e.what());
	}

	return texture_ptr();
}

/// Create a blank texture
///	@param w, h - size of texture
///	@param pixel_type - color type of pixel
///	@param format_type - texture puprose
///	@param allocatorName - allocator name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(int w, int h, pix_type pixel_type, texture::format_type format_type) const noexcept
{
	if(!set_current_context())
	{
		return {};
	}

	try
	{
		std::string name = format_type == texture::format_type::streaming ? "streaming" : "target";
		auto tex = new texture(*this, w, h, pixel_type, format_type);
		return texture_ptr(tex);
	}
	catch(const std::exception& e)
	{
		log(std::string("ERROR: Cannot create blank texture. Reason ") + e.what());
	}

	return texture_ptr();
}

/// Create a shader
///	@param fragment_code - fragment shader written in GLSL
///	@param vertex_code - vertex shader code written in GLSL
///	@return a shared pointer to the shader
shader_ptr renderer::create_shader(const char* fragment_code, const char* vertex_code) const noexcept
{
	if(!set_current_context())
	{
		return {};
	}

	try
	{
		auto* shad = new shader(*this, fragment_code, vertex_code);
		return shader_ptr(shad);
	}
	catch(const std::exception& e)
	{
		log(std::string("ERROR: Cannot create shader. Reason ") + e.what());
	}

	return {};
}

/// Create a font
///	@param info - description of font
///	@return a shared pointer to the font
font_ptr renderer::create_font(font_info&& info) const noexcept
{
	if(!set_current_context())
	{
		return {};
	}

	try
	{
		auto r = std::make_shared<font>();
		font_info& slice = *r;
		slice = std::move(info);

		r->texture = texture_ptr(new texture(*this, *r->surface));
		//if(r->sdf_spread == 0)
		{
			r->texture->generate_mipmap();
		}
		r->surface.reset();
		return r;
	}
	catch(const std::exception& e)
	{
		log(std::string("ERROR: Cannot create font. Reason ") + e.what());
	}


	return {};
}

/// Set the blending mode for drawing
///	@param mode - blending mode to set
///	@return true on success
bool renderer::set_blending_mode(blending_mode mode) const noexcept
{
	if(!set_current_context())
	{
		return false;
	}

	switch(mode)
	{
		case blending_mode::blend_none:
			gl_call(glDisable(GL_BLEND));
			break;
		case blending_mode::blend_normal:
			gl_call(glEnable(GL_BLEND));
			gl_call(glBlendEquation(GL_FUNC_ADD));
			gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
			break;
		case blending_mode::blend_add:
			gl_call(glEnable(GL_BLEND));
			gl_call(glBlendEquation(GL_FUNC_ADD));
			gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
			break;
		case blending_mode::blend_lighten:
			gl_call(glEnable(GL_BLEND));
			gl_call(glBlendEquation(GL_MAX));
			gl_call(glBlendFunc(GL_ONE, GL_ONE));
			break;
		case blending_mode::pre_multiplication:
			gl_call(glEnable(GL_BLEND));
			gl_call(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE));
			break;
		case blending_mode::unmultiplied_alpha:
			gl_call(glEnable(GL_BLEND));
			gl_call(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
			break;
		case blending_mode::blend_screen:
		default:
			// TODO
			gl_call(glEnable(GL_BLEND));
			break;
	}

	return true;
}

/// Lowest level texture binding call, for both fixed pipeline and shaders, and provide optional wrapping and interpolation modes
///     @param texture - the texture view
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::bind_texture(texture_view texture, uint32_t id) const noexcept
{
	// Activate texture for shader
	gl_call(glActiveTexture(GL_TEXTURE0 + id));

	// Bind texture to the pipeline, and the currently active texture
	gl_call(glBindTexture(GL_TEXTURE_2D, texture.id));

	// Check errors and return
	return true;
}

void renderer::bind_sampler(texture_view texture, uint32_t id) const noexcept
{
	auto sampler = samplers_[size_t(texture.wrap_type)][size_t(texture.interp_type)];
	if(sampler != 0)
	{
		gl_call(glBindSampler(id, sampler));
	}
	else
	{
		log("ERROR: Undefined sampler used! If it's legit - add it on initialization phase"); //auto add?
	}
}

void renderer::unbind_sampler(uint32_t id) const noexcept
{
	// Unbind active sampler
	gl_call(glBindSampler(id, 0));
}

/// Reset a texture slot
///     @param id - the texture to reset
void renderer::unbind_texture(uint32_t id) const noexcept
{
	gl_call(glActiveTexture(GL_TEXTURE0 + id));
	gl_call(glBindTexture(GL_TEXTURE_2D, 0));
}

texture_ptr renderer::blur(const texture_ptr& texture, uint32_t passes)
{
	passes = std::max<uint32_t>(1, passes);

	texture_ptr input = texture;
	auto radius = passes / 2.0f;

	for(uint32_t i = 0; i < passes; ++i)
	{
		if(i % 2 == 0)
		{
			radius = ((passes - i)/ 2.0f);
		}
		auto input_size = math::vec2(input->get_rect().w, input->get_rect().h);
		auto fbo = create_texture(int(input_size.x), int(input_size.y), input->get_pix_type(), texture::format_type::streaming);
		auto direction = i % 2 == 0 ? math::vec2{radius, 0.0f} : math::vec2{0.0f, radius};

		if (!push_fbo(fbo))
		{
			return {};
		}

		draw_list list;

		gfx::program_setup setup;
		setup.program = get_program<programs::blur>();
		setup.begin = [=](const gfx::gpu_context& ctx)
		{
			ctx.program.shader->set_uniform("uTexture", input);
			ctx.program.shader->set_uniform("uTextureSize", input_size);
			ctx.program.shader->set_uniform("uDirection", direction);
		};

		list.add_image(input, input->get_rect(), fbo->get_rect(), color::white(), flip_format::none, setup);

		draw_cmd_list(list);

		if (!pop_fbo())
		{
			return {};
		}

		input = fbo;
	}

	return input;
}

/// Push and load a modelview matrix
///     @param transform - the transformation to push
///     @return true on success
bool renderer::push_transform(const math::transformf& transform) const noexcept
{
	transforms_.push(transform);

	return true;
}

/// Restore a previously set matrix
///     @return true on success
bool renderer::pop_transform() const noexcept
{
	transforms_.pop();

	if (transforms_.empty())
	{
		reset_transform();
	}
	return true;
}

/// Reset the modelview transform to identity
///     @return true on success
bool renderer::reset_transform() const noexcept
{
	decltype (transforms_) new_tranform;
	new_tranform.push(math::transformf::identity());
	transforms_.swap(new_tranform);

	return true;
}

/// Set scissor test rectangle
///	@param rect - rectangle to clip
///	@return true on success
bool renderer::push_clip(const rect& rect) const noexcept
{
	if (!set_current_context())
	{
		return false;
	}

	gl_call(glEnable(GL_SCISSOR_TEST));

	if (fbo_stack_.empty())
	{
		// If we have no fbo_target_ it is a back buffer
		gl_call(glScissor(rect.x, rect_.h - rect.y - rect.h, rect.w, rect.h));
	}
	else
	{
		gl_call(glScissor(rect.x, rect.y, rect.w, rect.h));
	}

	return true;
}

bool renderer::pop_clip() const noexcept
{
	if (!set_current_context())
	{
		return false;
	}

	gl_call(glDisable(GL_SCISSOR_TEST));

	return true;
}

/// Set FBO target for drawing
///	@param texture - texture to draw to
///	@return true on success
bool renderer::push_fbo(const texture_ptr& texture)
{
	if (texture->format_type_ != texture::format_type::streaming)
	{
		return false;
	}

	if (!set_current_context())
	{
		return false;
	}

	fbo_stack_.push(texture);

	auto& top_texture = fbo_stack_.top();
	set_model_view(top_texture->get_fbo(), top_texture->get_rect());
	return true;
}

/// Set old FBO target for drawing. if empty set draw to back render buffer
///	@return true on success
bool renderer::pop_fbo()
{
	if (fbo_stack_.empty())
	{
		return false;
	}

	if (!set_current_context())
	{
		return false;
	}

	fbo_stack_.pop();

	if (fbo_stack_.empty())
	{
		set_model_view(0, rect_);
		return true;
	}

	auto& top_texture = fbo_stack_.top();
	set_model_view(top_texture->get_fbo(), top_texture->get_rect());
	return true;
}

/// Remove all FBO targets from stack and set draw to back render buffer
///	@return true on success
bool renderer::reset_fbo()
{
	while (!fbo_stack_.empty())
	{
		fbo_stack_.pop();
	}

	set_model_view(0, rect_);
	return true;
}

bool renderer::is_with_fbo() const
{
	return !fbo_stack_.empty();
}

/// Swap buffers
void renderer::present() noexcept
{
	auto sz = win_.get_size();
	resize(sz.w, sz.h);

	draw_list list{};
	list.add_line({0, 0}, {1, 1}, {255, 255, 255, 0});
	draw_cmd_list(list);

	context_->swap_buffers();

	set_current_context();
	set_model_view(0, rect_);
}

/// Clear the currently bound buffer
///	@param color - color to clear with
void renderer::clear(const color& color) const noexcept
{
	set_current_context();

	gl_call(glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
	gl_call(glClear(GL_COLOR_BUFFER_BIT));
}

constexpr uint16_t get_index_type()
{
	if(sizeof(draw_list::index_t) == sizeof(uint32_t))
	{
		return GL_UNSIGNED_INT;
	}

	return GL_UNSIGNED_SHORT;
}
/// Render a draw list
///	@param list - list to draw
///	@return true on success
bool renderer::draw_cmd_list(const draw_list& list) const noexcept
{
	if (!set_current_context())
	{
		return false;
	}

	if (list.vertices.empty())
	{
		return false;
	}

	//log(list.to_string());
	list.validate_stacks();

	const auto vtx_stride = sizeof(decltype(list.vertices)::value_type);
	const auto vertices_mem_size = list.vertices.size() * vtx_stride;
	const auto idx_stride = sizeof(decltype(list.indices)::value_type);
	const auto indices_mem_size = list.indices.size() * idx_stride;

	// We are using several stream buffers to avoid syncs caused by
	// uploading new data while the old one is still processing
	auto& vbo = stream_vbos_[current_vbo_idx_];
	auto& ibo = stream_ibos_[current_ibo_idx_];

	current_vbo_idx_ = (current_vbo_idx_ + 1) % stream_vbos_.size();
	current_ibo_idx_ = (current_ibo_idx_ + 1) % stream_ibos_.size();

	// Bind the vertex buffer
	vbo.bind();

	// Upload vertices to VRAM
	if (!vbo.update(list.vertices.data(), 0, vertices_mem_size))
	{
		// We're out of vertex budget. Allocate a new vertex buffer
		vbo.reserve(list.vertices.data(), vertices_mem_size, true);
	}

	// Bind the index buffer
	ibo.bind();

	// Upload indices to VRAM
	if (!ibo.update(list.indices.data(), 0, indices_mem_size))
	{
		// We're out of index budget. Allocate a new index buffer
		ibo.reserve(list.indices.data(), indices_mem_size, true);
	}


	{
		blending_mode last_blend{blending_mode::blend_none};
		set_blending_mode(last_blend);

		// Draw commands
		for (const auto& cmd : list.commands)
		{
			{
				if(cmd.setup.program.shader)
				{
					cmd.setup.program.shader->enable();
				}

				if (cmd.clip_rect)
				{
					push_clip(cmd.clip_rect);
				}

				if (cmd.blend != last_blend)
				{
					set_blending_mode(cmd.blend);
					last_blend = cmd.blend;
				}

				if(cmd.setup.begin)
				{
					cmd.setup.begin(gpu_context{*this, cmd.setup.program});
				}

				if(cmd.setup.program.shader && cmd.setup.program.shader->has_uniform("uProjection"))
				{
					const auto& projection = current_ortho_ * transforms_.top();
					cmd.setup.program.shader->set_uniform("uProjection", projection);
				}

			}

			switch(cmd.dr_type)
			{
				case draw_type::elements:
				{
					gl_call(glDrawElements(to_gl_primitive(cmd.type), GLsizei(cmd.indices_count), get_index_type(),
										   reinterpret_cast<const GLvoid*>(uintptr_t(cmd.indices_offset * idx_stride))));
				}
				break;

				case draw_type::array:
				{
					gl_call(glDrawArrays(to_gl_primitive(cmd.type), GLint(cmd.vertices_offset), GLsizei(cmd.vertices_count)));
				}
				break;
			}


			{
				if (cmd.setup.end)
				{
					cmd.setup.end(gpu_context{*this, cmd.setup.program});
				}

				if (cmd.clip_rect)
				{
					pop_clip();
				}

				if(cmd.setup.program.shader)
				{
					cmd.setup.program.shader->disable();
				}
			}
		}


		set_blending_mode(blending_mode::blend_none);
	}

	vbo.unbind();
	ibo.unbind();

	if(list.debug)
	{
		return draw_cmd_list(*list.debug);
	}

	return true;
}

/// Enable vertical synchronization to avoid tearing
///     @return true on success
bool renderer::enable_vsync() noexcept
{
	return context_->set_vsync(true);
}

/// Disable vertical synchronization
///     @return true on success
bool renderer::disable_vsync() noexcept
{
	return context_->set_vsync(false);
}

/// Get the dirty rectangle we're drawing into
///     @return the currently used rect
const rect& renderer::get_rect() const
{
	if (fbo_stack_.empty())
	{
		return rect_;
	}

	const auto& top_texture = fbo_stack_.top();
	return top_texture->get_rect();
}
}
