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

namespace gfx
{
template<typename T>
struct sparse_list
{
    size_t free_idx{0};
    std::array<T, 16> block;
};

template<typename Domain>
struct cache
{
template<typename T>
inline static auto& free_list() noexcept
{
    static sparse_list<T> list;
    return list;
}

template<typename T>
inline static bool get(T& val, size_t capacity) noexcept
{
    auto& list = free_list<T>();
    for(size_t i = 0, sz = list.block.size(); i < sz; ++i)
    {
        auto& el = list.block[i];

        if(el.capacity() >= capacity)
        {
            val = std::move(el);
            list.free_idx = i;
            return true;
        }
    }
    return false;
}


template<typename T>
inline static void add(T& val)
{
    auto& list = free_list<T>();
    if(list.free_idx < list.block.size())
    {
        val.clear();
        list.block[list.free_idx++] = std::move(val);
    }
}
};

struct exception : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};
}
