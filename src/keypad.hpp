#pragma once

#include <string>
#include <libwtk-sdl2/table.hpp>
#include <libwtk-sdl2/button.hpp>
#include <libwtk-sdl2/embedded_widget.hpp>

// Provides a widget with a keypad that can be submitted.
struct keypad : embedded_widget<table>
{

    keypad(vec size, std::string keys, std::function<void(std::string)> submit_callback);
    ~keypad() override;

    void clear();

    private:

    keypad(vec size, std::tuple<std::shared_ptr<button>, std::vector<table::entry>> tmp);

    void remove_last();
    void update();
    void append(std::string c);
    
    std::tuple<std::shared_ptr<button>, std::vector<table::entry>> construct(vec size, std::string keys, std::function<void(std::string)> submit_callback);

    std::shared_ptr<button> _submit_button;
    std::string _search_term;
};

