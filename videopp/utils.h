#pragma once

#define GL_GLEXT_PROTOTYPES
#include "detail/glad.h"

#include "logger.h"
#include <sstream>

namespace video_ctrl
{
#ifndef NDEBUG
inline void gl_clear_error()
{
	while(glGetError() != GL_NO_ERROR)
		;
}

inline void gl_log_call(const char* function, const char* file, int line)
{
	while(GLenum error = glGetError())
	{
		std::stringstream s;
		s << "[OpenGL Error] (" << error << "): " << function << " " << file << ":" << line << "\n";
		log(s.str());
	}
}

#define gl_call(x)                                                                                           \
	gl_clear_error();                                                                                        \
	x;                                                                                                       \
	gl_log_call(#x, __FILE__, __LINE__)
#else

#define gl_call(x) x

#endif
}
