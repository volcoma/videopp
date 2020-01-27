#pragma once
//-----------------------------------------------------------------------------
// transform Header Includes
//-----------------------------------------------------------------------------
#include "glm_includes.h"

namespace math
{
using namespace glm;
//-----------------------------------------------------------------------------
// Main class declarations
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//  Name : transform (Class)
/// <summary>
/// General purpose transformation class designed to maintain each component of
/// the transformation separate (translation, rotation, scale and shear) whilst
/// providing much of the same functionality provided by standard matrices.
/// </summary>
//-----------------------------------------------------------------------------
template <typename T, precision Q = defaultp>
class transform_t
{
public:
    using mat4_t = mat<4, 4, T, Q>;
    using vec2_t = vec<2, T, Q>;
    using vec3_t = vec<3, T, Q>;
    using vec4_t = vec<4, T, Q>;
    using quat_t = qua<T, Q>;
    //-------------------------------------------------------------------------
    // Constructors & Destructors
    //-------------------------------------------------------------------------
    transform_t() = default;
    transform_t(const transform_t& t) = default;
    transform_t(transform_t&& t) noexcept = default;
    transform_t& operator=(const transform_t& m) = default;
    transform_t& operator=(transform_t&& m) noexcept = default;

    transform_t(const mat4_t& m) noexcept;

    const vec3_t& get_position() const noexcept;
    void set_position(const vec3_t& position) noexcept;
    void set_position(T x, T y, T z) noexcept;

    vec3_t get_rotation_euler() const noexcept;
    void set_rotation_euler(const vec3_t& euler_angles) noexcept;
    void set_rotation_euler(T x, T y, T z) noexcept;

    const vec3_t& get_scale() const noexcept;
    void set_scale(const vec3_t& scale) noexcept;
    void set_scale(T x, T y, T z) noexcept;

    const quat_t& get_rotation() const noexcept;
    void set_rotation(const quat_t& rotation) noexcept;
    void set_rotation(const vec3_t& x, const vec3_t& y, const vec3_t& z) noexcept;

    vec3_t x_axis() const noexcept;
    vec3_t y_axis() const noexcept;
    vec3_t z_axis() const noexcept;
    vec3_t x_unit_axis() const noexcept;
    vec3_t y_unit_axis() const noexcept;
    vec3_t z_unit_axis() const noexcept;

    // these transform from the current state
    void rotate_axis(T a, const vec3_t& v) noexcept;
    void rotate(T x, T y, T z) noexcept;
    void rotate(const vec3_t& v) noexcept;
    void rotate_local(T x, T y, T z) noexcept;
    void rotate_local(const vec3_t& v) noexcept;
    void scale(T x, T y, T z) noexcept;
    void scale(const vec3_t& v) noexcept;
    void translate(T x, T y, T z) noexcept;
    void translate(const vec3_t& v) noexcept;
    void translate_local(T x, T y, T z) noexcept;
    void translate_local(const vec3_t& v) noexcept;

    int compare(const transform_t& t) const noexcept;
    int compare(const transform_t& t, T tolerance) const noexcept;

    vec2_t transform_coord(const vec2_t& v) const noexcept;
    vec3_t transform_coord(const vec3_t& v) const noexcept;

    vec2_t inverse_transform_coord(const vec2_t& v) const noexcept;
    vec3_t inverse_transform_coord(const vec3_t& v) const noexcept;

    vec2_t transform_normal(const vec2_t& v) const noexcept;
    vec3_t transform_normal(const vec3_t& v) const noexcept;

    vec2_t inverse_transform_normal(const vec2_t& v) const noexcept;
    vec3_t inverse_transform_normal(const vec3_t& v) const noexcept;

    static const transform_t& identity() noexcept;
    //-------------------------------------------------------------------------
    // Public Operator Overloads
    //-------------------------------------------------------------------------
    operator const mat4_t&() const noexcept;
    operator const mat4_t*() const noexcept;
    operator const typename mat4_t::value_type*() const noexcept;

    transform_t operator*(const transform_t& t) const noexcept;
    inline typename mat4_t::col_type const& operator[](typename mat4_t::length_type i) const noexcept
    {
        return get_matrix()[i];
    }

    inline vec4_t operator*(const vec4_t& v) const noexcept
    {
        vec4_t result = get_matrix() * v;
        return result;
    }

    inline const mat4_t& get_matrix() const noexcept
    {
        update_matrix();
        return matrix_;
    }

private:

    inline void update_components() noexcept
    {
        vec3_t skew;
        vec4_t perspective;

        // workaround for decompose when
        // used on projection matrix
        mat4_t m = matrix_;
        m[3][3] = 1;


        for(size_t i = 0; i < 3; ++i)
        {
            auto& el = m[math::length_t(i)][math::length_t(i)];
            if(epsilonEqual(el, 0.0f, epsilon<float>()))
            {
                el = 0.1f;
            }
        }

        fixed_decompose(m, scale_, rotation_, position_, skew, perspective);
    }

    inline void update_matrix() const noexcept
    {
        if(dirty_)
        {
            auto translation = glm::translate(position_);
            auto rotation = glm::mat4_cast(rotation_);
            auto scale = glm::scale(scale_);

            matrix_ = translation * rotation * scale;

            dirty_ = false;
        }
    }
    inline void make_dirty() noexcept
    {
        dirty_ = true;
    }
    //-------------------------------------------------------------------------
    // Protected Variables
    //-------------------------------------------------------------------------
    // this should be always first.
    mutable mat4_t matrix_ = mat4_t(1);

    vec3_t position_ = vec3_t(0, 0, 0);
    quat_t rotation_ = quat_t(1, 0, 0, 0);
    vec3_t scale_ = vec3_t(1, 1, 1);

    mutable bool dirty_ = false;
};

template <typename T, precision Q>
transform_t<T, Q> inverse(transform_t<T, Q> const& t) noexcept
{
    const auto& m = t.get_matrix();
    return glm::inverse(m);
}

template <typename T, precision Q>
transform_t<T, Q> transpose(transform_t<T, Q> const& t) noexcept
{
    const auto& m = t.get_matrix();
    return glm::transpose(m);
}

template <typename T, precision Q>
inline transform_t<T, Q>::transform_t(const typename transform_t::mat4_t& m) noexcept
    : matrix_(m)
{
    update_components();
}

template <typename T, precision Q>
inline const typename transform_t<T, Q>::vec3_t& transform_t<T, Q>::get_position() const noexcept
{
    return position_;
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_position(const typename transform_t::vec3_t& position) noexcept
{
    position_ = position;
    make_dirty();
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_position(T x, T y, T z) noexcept
{
    set_position({x, y, z});
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::get_rotation_euler() const noexcept
{
    return eulerAngles(rotation_);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_rotation_euler(const typename transform_t::vec3_t& euler_angles) noexcept
{
    set_rotation(quat_t(euler_angles));
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_rotation_euler(T x, T y, T z) noexcept
{
    set_rotation_euler({x, y, z});
}

template <typename T, precision Q>
inline const typename transform_t<T, Q>::vec3_t& transform_t<T, Q>::get_scale() const noexcept
{
    return scale_;
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_scale(const typename transform_t::vec3_t& scale) noexcept
{
    scale_ = scale;
    make_dirty();
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_scale(T x, T y, T z) noexcept
{
    set_scale({x, y, z});
}

template <typename T, precision Q>
inline const typename transform_t<T, Q>::quat_t& transform_t<T, Q>::get_rotation() const noexcept
{
    return rotation_;
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_rotation(const typename transform_t::quat_t& rotation) noexcept
{
    rotation_ = glm::normalize(rotation);
    make_dirty();
}

template <typename T, precision Q>
inline void transform_t<T, Q>::set_rotation(const typename transform_t::vec3_t& x,
                                            const typename transform_t::vec3_t& y,
                                            const typename transform_t::vec3_t& z) noexcept
{
    // Get current scale so that it can be preserved.
    const auto& scale = get_scale();

    update_matrix();
    // Set the new axis vectors (normalized)
    reinterpret_cast<vec3_t&>(matrix_[0]) = glm::normalize(x);
    reinterpret_cast<vec3_t&>(matrix_[1]) = glm::normalize(y);
    reinterpret_cast<vec3_t&>(matrix_[2]) = glm::normalize(z);

    // Scale back to original length
    reinterpret_cast<vec3_t&>(matrix_[0]) *= scale.x;
    reinterpret_cast<vec3_t&>(matrix_[1]) *= scale.y;
    reinterpret_cast<vec3_t&>(matrix_[2]) *= scale.z;

    update_components();
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::x_axis() const noexcept
{
    return get_matrix()[0];
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::y_axis() const noexcept
{
    return get_matrix()[1];
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::z_axis() const noexcept
{
    return get_matrix()[2];
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::x_unit_axis() const noexcept
{
    return normalize(x_axis());
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::y_unit_axis() const noexcept
{
    return normalize(y_axis());
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t transform_t<T, Q>::z_unit_axis() const noexcept
{
    return normalize(z_axis());
}

template <typename T, precision Q>
inline void transform_t<T, Q>::rotate_axis(T a, const typename transform_t::vec3_t& v) noexcept
{
    quat_t q = glm::angleAxis(a, v) * get_rotation();
    set_rotation(q);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::rotate(T x, T y, T z) noexcept
{
    rotate({x, y, z});
}

template <typename T, precision Q>
inline void transform_t<T, Q>::rotate(const typename transform_t::vec3_t& v) noexcept
{
    quat_t qx = glm::angleAxis(v.x, vec3_t{1, 0, 0});
    quat_t qy = glm::angleAxis(v.y, vec3_t{0, 1, 0});
    quat_t qz = glm::angleAxis(v.z, vec3_t{0, 0, 1});
    quat_t q = qz * qy * qx * get_rotation();
    set_rotation(q);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::rotate_local(T x, T y, T z) noexcept
{
    rotate_local({x, y, z});
}

template <typename T, precision Q>
inline void transform_t<T, Q>::rotate_local(const typename transform_t::vec3_t& v) noexcept
{
    quat_t qx = glm::angleAxis(v.x, x_unit_axis());
    quat_t qy = glm::angleAxis(v.y, y_unit_axis());
    quat_t qz = glm::angleAxis(v.z, z_unit_axis());
    quat_t q = qz * qy * qx * get_rotation();
    set_rotation(q);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::scale(T x, T y, T z) noexcept
{
    scale({x, y, z});
}

template <typename T, precision Q>
inline void transform_t<T, Q>::scale(const typename transform_t::vec3_t& v) noexcept
{
    set_scale(get_scale() * v);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::translate(T x, T y, T z) noexcept
{
    translate({x, y, z});
}

template <typename T, precision Q>
inline void transform_t<T, Q>::translate(const typename transform_t::vec3_t& v) noexcept
{
    set_position(get_position() + v);
}

template <typename T, precision Q>
inline void transform_t<T, Q>::translate_local(T x, T y, T z) noexcept
{
    translate_local({x, y, z});
}

template <typename T, precision Q>
inline void transform_t<T, Q>::translate_local(const typename transform_t::vec3_t& v) noexcept
{
    set_position(get_position() + (x_unit_axis() * v.x));
    set_position(get_position() + (y_unit_axis() * v.y));
    set_position(get_position() + (z_unit_axis() * v.z));
}

template <typename T, precision Q>
inline int transform_t<T, Q>::compare(const transform_t& t) const noexcept
{
    return static_cast<int>(get_matrix() == t.get_matrix());
}

template <typename T, precision Q>
inline int transform_t<T, Q>::compare(const transform_t& t, T tolerance) const noexcept
{
    const auto& m1 = get_matrix();
    const auto& m2 = t.get_matrix();
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            float diff = m1[i][j] - m2[i][j];
            if(glm::abs<T>(diff) > tolerance)
            {
                return (diff < 0) ? -1 : 1;
            }
        }
    }

    // Equivalent
    return 0;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec2_t
transform_t<T, Q>::transform_coord(const typename transform_t::vec2_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    vec4_t result = m * vec4_t{v, 0.0f, 1.0f};
    result /= result.w;
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec2_t
transform_t<T, Q>::inverse_transform_coord(const typename transform_t::vec2_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    mat4_t im = glm::inverse(m);
    vec3_t result = im * vec4_t{v, 0.0f, 1.0f};
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec2_t
transform_t<T, Q>::transform_normal(const typename transform_t::vec2_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    vec4_t result = m * vec4_t{v, 0.0f, 0.0f};
    result /= result.w;
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec2_t
transform_t<T, Q>::inverse_transform_normal(const typename transform_t::vec2_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    mat4_t im = glm::inverse(m);
    vec3_t result = im * vec4_t{v, 0.0f, 0.0f};
    return result;
}


template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t
transform_t<T, Q>::transform_coord(const typename transform_t::vec3_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    vec4_t result = m * vec4_t{v, 1.0f};
    result /= result.w;
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t
transform_t<T, Q>::inverse_transform_coord(const typename transform_t::vec3_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    mat4_t im = glm::inverse(m);
    vec3_t result = im * vec4_t{v, 1.0f};
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t
transform_t<T, Q>::transform_normal(const typename transform_t::vec3_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    vec4_t result = m * vec4_t{v, 0.0f, 0.0f};
    result /= result.w;
    return result;
}

template <typename T, precision Q>
inline typename transform_t<T, Q>::vec3_t
transform_t<T, Q>::inverse_transform_normal(const typename transform_t::vec3_t& v) const noexcept
{
    const mat4_t& m = get_matrix();
    mat4_t im = glm::inverse(m);
    vec3_t result = im * vec4_t{v, 0.0f};
    return result;
}

template <typename T, precision Q>
inline const transform_t<T, Q>& transform_t<T, Q>::identity() noexcept
{
    static transform_t identity;
    return identity;
}

template <typename T, precision Q>
inline transform_t<T, Q> transform_t<T, Q>::operator*(const transform_t& t) const noexcept
{
    return transform_t(get_matrix() * t.get_matrix());
}

template <typename T, precision Q>
inline transform_t<T, Q>::operator const typename transform_t<T, Q>::mat4_t::value_type*() const noexcept
{
    return value_ptr(get_matrix());
}

template <typename T, precision Q>
inline transform_t<T, Q>::operator const mat4_t*() const noexcept
{
    return &get_matrix();
}

template <typename T, precision Q>
inline transform_t<T, Q>::operator const mat4_t&() const noexcept
{
    return get_matrix();
}

using transformf = transform_t<float>;
}
