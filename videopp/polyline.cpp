#include "polyline.h"
#include <array>
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


inline float sqr (float a)
{
    return a * a;
}

inline float positive_angle (float angle)
{
    return angle < 0 ? angle + 2 * math::pi<float>() : angle;
}

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

void polyline::arc_between(const math::vec2& p1, const math::vec2& p, const math::vec2& p2, float radius)
{
    if(math::epsilonEqual(radius, 0.0f, math::epsilon<float>()))
    {
        line_to(p);
        return;
    }

    // 2
    float angle = positive_angle (math::atan2<float, math::highp> (p.y - p1.y, p.x - p1.x) - math::atan2<float, math::highp>(p.y - p2.y, p.x - p2.x));


    if(math::epsilonEqual(angle, 0.0f, math::epsilon<float>()))
    {
        line_to(p);
        return;
    }

    // 3
    float segment = radius / math::abs (math::tan (angle / 2.0f));
    float p_c1 = segment;
    float p_c2 = segment;

    // 4
    float p_p1 = math::sqrt(sqr (p.x - p1.x) + sqr (p.y - p1.y));
    float p_p2 = math::sqrt(sqr (p.x - p2.x) + sqr (p.y - p2.y));
    float min = math::min(p_p1, p_p2);
    if (segment > min)
    {
        segment = min;
        radius = segment * math::abs (math::tan (angle / 2.0f));
    }

    // 5
    float p_o = math::sqrt (sqr (radius) + sqr (segment));

    // 6
    math::vec2 c1;
    c1.x =  (p.x - (p.x - p1.x) * p_c1 / p_p1);
    c1.y =  (p.y - (p.y - p1.y) * p_c1 / p_p1);

    //  7
    math::vec2 c2;
    c2.x = (p.x - (p.x - p2.x) * p_c2 / p_p2);
    c2.y = (p.y - (p.y - p2.y) * p_c2 / p_p2);

    // 8
    float dx = p.x * 2 - c1.x - c2.x;
    float dy = p.y * 2 - c1.y - c2.y;

    float p_c = math::sqrt (sqr (dx) + sqr (dy));

    math::vec2 o;
    o.x = p.x - dx * p_o / p_c;
    o.y = p.y - dy * p_o / p_c;

    // 9
    float start_angle = positive_angle (math::atan2<float, math::highp> ((c1.y - o.y), (c1.x - o.x)));
    float end_angle = positive_angle (math::atan2<float, math::highp> ((c2.y - o.y), (c2.x - o.x)));

    if(angle <= math::pi<float>())
    {
        arc_to(o, radius, start_angle, end_angle);
    }
    else
    {
        arc_to_negative(o, radius, start_angle, end_angle);
    }
}

void polyline::arc_to(const math::vec2 &centre, float radius, float a_min, float a_max, size_t num_segments)
{
    arc_to(centre, {radius, radius}, a_min, a_max, num_segments);
}

void polyline::arc_to(const math::vec2 &centre, const math::vec2& radii, float a_min, float a_max, size_t num_segments)
{
    if(math::any(math::epsilonEqual(radii, {0.0f, 0.0f}, math::epsilon<float>())))
    {
        line_to(centre);
        return;
    }

    while(a_max < a_min)
    {
        a_max += 2.0f * math::pi<float>();
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
        line_to(centre);
        return;
    }

    while(a_max > a_min)
    {
        a_max -= 2.0f * math::pi<float>();
    }

    // Note that we are adding a point at both a_min and a_max.
    // If you are trying to draw a full closed circle you don't want the overlapping points!
    points_.reserve(points_.size() + (num_segments + 1));
    for (size_t i = num_segments + 1; i > 0; i--)
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
        line_to(centre);
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

void polyline::ellipse(const math::vec2& center, const math::vec2& radii, size_t num_segments)
{
    const float a_max = math::pi<float>() * 2.0f * (float(num_segments) - 1.0f) / float(num_segments);
    arc_to(center, radii-0.5f, 0.0f, a_max, num_segments - 1);
}


void polyline::path(const std::vector<math::vec2>& points, float corner_radius)
{
    size_t count = points.size();

    if(count < 3)
    {
        for (const auto& p : points)
        {
            line_to(p);
        }
        return;
    }

    line_to(points.front());

    for (size_t i = 1; i < count - 1; ++i)
    {
        math::vec2 prev = points[i - 1];
        math::vec2 edge = points[i + 0];
        math::vec2 next = points[i + 1];
        arc_between(prev, edge, next, corner_radius);
    }

    line_to(points.back());

}

}
