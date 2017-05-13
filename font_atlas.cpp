#include "font_atlas.hpp"

font_not_found::font_not_found(std::string msg)
    : std::runtime_error("font not found: " + msg)
{
}

font_atlas::font_atlas(std::string font_path, int ptsize)
{
    // load font and generate glyphs
    _font = TTF_OpenFont(font_path.c_str(), ptsize);

    if (_font == nullptr)
        throw font_not_found(TTF_GetError());
}

font_atlas::~font_atlas()
{
    TTF_CloseFont(_font);

    for (auto p : _prerendered)
        SDL_FreeSurface(p.second);
}

SDL_Surface * font_atlas::text(std::string t)
{
    // TODO add limit to prevent filling memory
    auto it = _prerendered.find(t);
    if (it == _prerendered.end())
    {
        return _prerendered[t] = TTF_RenderUTF8_Blended(_font, t.c_str(), {255, 255, 255});
    }
    else
    {
        return it->second;
    }
}
