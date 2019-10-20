#pragma once
#include <functional>
#include <stdexcept>
#include <vector>

namespace utils
{

template <class T>
inline void hash(uint64_t& seed, const T & v) noexcept
{
    std::hash<T> hasher;
    seed ^= static_cast<uint64_t>(hasher(v)) + 0x9e3779b967dd4acc + (seed << 6) + (seed >> 2);
}

template <typename Arg0, typename Arg1, typename... Args>
inline void hash(uint64_t& seed, Arg0&& arg0, Arg1&& arg1, Args&& ...args) noexcept
{
    hash(seed, std::forward<Arg0>(arg0));
    hash(seed, std::forward<Arg1>(arg1), std::forward<Args>(args)...);
}

}

namespace video_ctrl
{

template<typename Domain>
struct cache
{
template<typename T>
inline static std::vector<T>& recycle_bin() noexcept
{
    static std::vector<T> bin;
    return bin;
}

template<typename T>
inline static bool get(T& val) noexcept
{
    auto& bin = recycle_bin<std::decay_t<T>>();
    if(bin.empty())
    {
        return false;
    }

    val = std::move(bin.back());
    bin.pop_back();
    return true;
}

template<typename T>
inline static void add(T& val)
{
    val.clear();
    auto& bin = recycle_bin<std::decay_t<T>>();
    bin.emplace_back(std::move(val));
}
};

struct exception : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};
}
