#pragma once

#include <utility>

#include <libwtk-sdl2/embedded_widget.hpp>
#include <libwtk-sdl2/notebook.hpp>
#include <libwtk-sdl2/list_view.hpp>

#include "keypad.hpp"

struct search_view : embedded_widget<notebook>
{
    search_view(vec size, std::string keys, std::vector<std::string> const & values, std::function<void(std::size_t)> activate_callback);

    private:

    search_view(std::shared_ptr<keypad> keypad, std::shared_ptr<list_view> list_view, std::vector<std::string> const & values);

    void on_submit(std::string search_term);
    void on_back();

    std::shared_ptr<keypad> _keypad;
    std::shared_ptr<list_view> _list_view;

    std::reference_wrapper<std::vector<std::string> const> _values;
    std::vector<std::string> _filtered_values;
    std::vector<std::size_t> _filtered_indices;

};
