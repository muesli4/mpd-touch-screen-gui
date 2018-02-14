
#include "cover_view.hpp"

cover_view::cover_view(std::function<void(swipe_action)> swipe_callback, std::function<void()> press_callback)
    : bin(std::make_shared<label>(""))
    , _swipe_area(swipe_callback, press_callback)
{
}

    std::shared_ptr<texture_view> _texture_ptr;
    std::shared_ptr<label> _label;
cover_view::~cover_view()
{
}

void cover_view::on_mouse_up_event(mouse_up_event const & e)
{
    _swipe_area.on_mouse_up_event(e);
}

void cover_view::set_cover(cover c)
{
    if (c.type == cover_type::IMAGE)
    {
        _child = std::make_shared<texture_view, unique_texture_ptr, int>(std::move(c.texture_ptr), 100);
    }
    else
    {
        _child = std::make_shared<label, std::vector<paragraph>>({ paragraph(c.info.title), paragraph(c.info.artist), paragraph(c.info.album) });
    }
}

