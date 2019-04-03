#pragma once

#include <string>
#include <string_view>
#include <libwtk-sdl2/grid.hpp>
#include <libwtk-sdl2/text_button.hpp>
#include <libwtk-sdl2/embedded_widget.hpp>

// Provides a widget with a keypad that can be submitted.
struct keypad : embedded_widget<grid>
{

    keypad(vec size, std::string keys, std::function<void(std::string)> submit_callback);
    ~keypad() override;

    void clear();
    std::string_view const get_input() const;

    private:

    keypad(vec size, std::tuple<std::shared_ptr<text_button>, std::vector<grid::entry>> tmp);

    void remove_last();
    void update();
    void append(std::string c);
    
    std::tuple<std::shared_ptr<text_button>, std::vector<grid::entry>> construct(vec size, std::string keys, std::function<void(std::string)> submit_callback);

    std::shared_ptr<text_button> _submit_button;
    std::string _search_term;
};

