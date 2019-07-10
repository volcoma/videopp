#include "polyline.h"

namespace video_ctrl
{

namespace
{
std::array<math::vec2, 12> circle_vtx12 = []()
{
    std::array<math::vec2, 12> vtx12;
    for (size_t i = 0; i < vtx12.size(); i++)
    {
        const float a = (float(i) * 2 * math::pi<float>()) / float(vtx12.size());
        vtx12[i] = {math::cos(a), math::sin(a)};
    }
    return vtx12;
}();
}

void polyline::clear()
{
    points_.clear();
}

void polyline::line_to(const math::vec2 &pos)
{
    if(points_.empty() || points_.back() != pos)
        points_.push_back(pos);
}

void polyline::arc_to(const math::vec2 &centre, float radius, float a_min, float a_max, size_t num_segments)
{
    arc_to(centre, {radius, radius}, a_min, a_max, num_segments);
}

void polyline::arc_to(const math::vec2 &centre, const math::vec2& radii, float a_min, float a_max, size_t num_segments)
{
    if(math::any(math::epsilonEqual(radii, {0.0f, 0.0f}, math::epsilon<float>())))
    {
        points_.push_back(centre);
        return;
    }

    // Note that we are adding a point at both a_min and a_max.
    // If you are trying to draw a full closed circle you don't want the overlapping points!
    points_.reserve(points_.size() + (num_segments + 1));
    for (size_t i = 0; i <= num_segments; i++)
    {
        const float a = a_min + (float(i) / float(num_segments)) * (a_max - a_min);
        points_.emplace_back(centre.x + math::cos(a) * radii.x, centre.y + math::sin(a) * radii.y);
    }

}

void polyline::arc_to_negative(const math::vec2 &centre, float radius, float a_min, float a_max, size_t num_segments)
{
    arc_to_negative(centre, {radius, radius}, a_min, a_max, num_segments);
}

void polyline::arc_to_negative(const math::vec2 &centre, const math::vec2& radii, float a_min, float a_max, size_t num_segments)
{
    if(math::any(math::epsilonEqual(radii, {0.0f, 0.0f}, math::epsilon<float>())))
    {
        points_.push_back(centre);
        return;
    }

    // Note that we are adding a point at both a_min and a_max.
    // If you are trying to draw a full closed circle you don't want the overlapping points!
    points_.reserve(points_.size() + (num_segments + 1));
    for (size_t i = num_segments; i > 0; i--)
    {
        const float a = a_max - (float(i-1) / float(num_segments)) * (a_max - a_min);
        points_.emplace_back(centre.x + math::cos(a) * radii.x, centre.y + math::sin(a) * radii.y);
    }

}

void polyline::arc_to_fast(const math::vec2 &centre, float radius, size_t a_min_of_12, size_t a_max_of_12)
{
    arc_to_fast(centre, {radius, radius}, a_min_of_12, a_max_of_12);
}

void polyline::arc_to_fast(const math::vec2 &centre, const math::vec2& radii, size_t a_min_of_12, size_t a_max_of_12)
{
    if ((math::any(math::epsilonEqual(radii, {0.0f, 0.0f}, math::epsilon<float>()))) || a_min_of_12 > a_max_of_12)
    {
        points_.emplace_back(centre);
        return;
    }
    points_.reserve(points_.size() + (a_max_of_12 - a_min_of_12 + 1));
    for (size_t a = a_min_of_12; a <= a_max_of_12; a++)
    {
        const auto& c = circle_vtx12[a % circle_vtx12.size()];
        points_.emplace_back(centre.x + c.x * radii.x, centre.y + c.y * radii.y);
    }
}


static void bezier_to_casteljau(std::vector<math::vec2>& path, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tess_tol, int level)
{
    float dx = x4 - x1;
    float dy = y4 - y1;
    float d2 = ((x2 - x4) * dy - (y2 - y4) * dx);
    float d3 = ((x3 - x4) * dy - (y3 - y4) * dx);
    d2 = (d2 >= 0) ? d2 : -d2;
    d3 = (d3 >= 0) ? d3 : -d3;
    if ((d2+d3) * (d2+d3) < tess_tol * (dx*dx + dy*dy))
    {
        path.emplace_back(x4, y4);
    }
    else if (level < 10)
    {
        float x12 = (x1+x2)*0.5f,       y12 = (y1+y2)*0.5f;
        float x23 = (x2+x3)*0.5f,       y23 = (y2+y3)*0.5f;
        float x34 = (x3+x4)*0.5f,       y34 = (y3+y4)*0.5f;
        float x123 = (x12+x23)*0.5f,    y123 = (y12+y23)*0.5f;
        float x234 = (x23+x34)*0.5f,    y234 = (y23+y34)*0.5f;
        float x1234 = (x123+x234)*0.5f, y1234 = (y123+y234)*0.5f;

        bezier_to_casteljau(path, x1,y1,        x12,y12,    x123,y123,  x1234,y1234, tess_tol, level+1);
        bezier_to_casteljau(path, x1234,y1234,  x234,y234,  x34,y34,    x4,y4,       tess_tol, level+1);
    }
}

void polyline::bezier_curve_to(const math::vec2 &p2, const math::vec2 &p3, const math::vec2 &p4, int num_segments)
{
    if(points_.empty())
    {
        points_.emplace_back();
    }
    auto p1 = points_.back();
    if (num_segments == 0)
    {
        // Auto-tessellated
        bezier_to_casteljau(points_, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, 1, 0);
    }
    else
    {
        float t_step = 1.0f / float(num_segments);
        for (int i_step = 1; i_step <= num_segments; i_step++)
        {
            float t = t_step * i_step;
            float u = 1.0f - t;
            float w1 = u*u*u;
            float w2 = 3*u*u*t;
            float w3 = 3*u*t*t;
            float w4 = t*t*t;
            points_.emplace_back(w1*p1.x + w2*p2.x + w3*p3.x + w4*p4.x, w1*p1.y + w2*p2.y + w3*p3.y + w4*p4.y);
        }
    }

}

void polyline::rectangle(const math::vec2& a, const math::vec2& b, float rounding, uint32_t rounding_corners)
{
    rounding = math::min(rounding, math::abs(b.x - a.x) * ( ((rounding_corners & corner_flags::top)  == corner_flags::top)  || ((rounding_corners & corner_flags::bot)   == corner_flags::bot)   ? 0.5f : 1.0f ) - 1.0f);
    rounding = math::min(rounding, math::abs(b.y - a.y) * ( ((rounding_corners & corner_flags::left) == corner_flags::left) || ((rounding_corners & corner_flags::right) == corner_flags::right) ? 0.5f : 1.0f ) - 1.0f);

    if (rounding <= 0.0f || rounding_corners == 0)
    {
        line_to(a);
        line_to({b.x, a.y});
        line_to(b);
        line_to({a.x, b.y});
    }
    else
    {
        const float rounding_tl = (rounding_corners & corner_flags::top_left) ? rounding : 0.0f;
        const float rounding_tr = (rounding_corners & corner_flags::top_right) ? rounding : 0.0f;
        const float rounding_br = (rounding_corners & corner_flags::bot_right) ? rounding : 0.0f;
        const float rounding_bl = (rounding_corners & corner_flags::bot_left) ? rounding : 0.0f;
        arc_to_fast({a.x + rounding_tl, a.y + rounding_tl}, rounding_tl, 6, 9);
        arc_to_fast({b.x - rounding_tr, a.y + rounding_tr}, rounding_tr, 9, 12);
        arc_to_fast({b.x - rounding_br, b.y - rounding_br}, rounding_br, 0, 3);
        arc_to_fast({a.x + rounding_bl, b.y - rounding_bl}, rounding_bl, 3, 6);
    }
}

void polyline::rectangle(const rect& r, float rounding, uint32_t rounding_corners_flags)
{
    rectangle(math::vec2{r.x, r.y}, math::vec2{r.x + r.w, r.y + r.h}, rounding, rounding_corners_flags);
}

}
