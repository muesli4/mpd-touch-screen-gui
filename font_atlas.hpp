#ifndef FONT_ATLAS_HPP
#define FONT_ATLAS_HPP

#include <stdexcept>
#include <unordered_map>
#include <memory>

#include <SDL2/SDL_ttf.h>

struct font_not_found : std::runtime_error
{
    font_not_found(std::string);
};

struct font_atlas
{
    font_atlas(std::string font_path, int ptsize);
    ~font_atlas();
    std::unique_ptr<SDL_Surface, void(*)(SDL_Surface *)> text(std::string);

    unsigned int height();

    int font_line_skip();

    private:

    SDL_Surface * word(std::string);

    std::unordered_map<std::string, SDL_Surface *> _prerendered;
    TTF_Font * _font;
    int _space_advance;
    int _space_minx;
};

#endif
