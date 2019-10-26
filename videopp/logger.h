#ifndef VCTRL_LOGGER_H
#define VCTRL_LOGGER_H
#include <string>
#include <functional>
namespace gfx
{

void log(const std::string& msg);
void set_extern_logger(std::function<void(const std::string&)> logger);

}

#endif // VCTRL_LOGGER_H
