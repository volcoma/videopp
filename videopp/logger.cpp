#include "logger.h"

namespace gfx
{

namespace detail
{
static std::function<void(const std::string&)>& get_logger()
{
    static std::function<void(const std::string&)> logger;
    return logger;
}
} 

void log(const std::string& msg)
{
    auto& logger = detail::get_logger();
    if(logger)
    {
        logger(msg);
    }
}

void set_extern_logger(std::function<void(const std::string&)> logger)
{
    detail::get_logger() = std::move(logger);
}

}
