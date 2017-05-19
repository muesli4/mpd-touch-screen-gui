#include <vector>
#include <iostream>
#include <algorithm>

#include "font_atlas.hpp"
#include "util.hpp"

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

    // cache join point metrics
    TTF_GlyphMetrics(_font, ' ', &_space_minx, nullptr, nullptr, nullptr, &_space_advance);
}

font_atlas::~font_atlas()
{
    TTF_CloseFont(_font);

    for (auto p : _prerendered)
        SDL_FreeSurface(p.second);
}

// http://stackoverflow.com/questions/18534494/convert-from-utf-8-to-unicode-c
#include <deque>
uint32_t utf8_to_ucs4(std::deque<uint8_t> coded)
{
    int charcode = 0;
    int t = coded.front();
    coded.pop_front();
    if (t < 128)
    {
        return t;
    }
    int high_bit_mask = (1 << 6) -1;
    int high_bit_shift = 0;
    int total_bits = 0;
    const int other_bits = 6;
    while((t & 0xC0) == 0xC0)
    {
        t <<= 1;
        t &= 0xff;
        total_bits += 6;
        high_bit_mask >>= 1;
        high_bit_shift++;
        charcode <<= other_bits;
        charcode |= coded.front() & ((1 << other_bits)-1);
        coded.pop_front();
    }
    charcode |= ((t >> high_bit_shift) & high_bit_mask) << total_bits;
    return charcode;
}

// get the last utf8 character from a string
uint32_t get_last_ucs4(std::string s)
{
    char const * ptr = s.c_str() + s.size() - 1;
    if (*ptr == 0) ptr = " ";

    std::deque<uint8_t> d;
    while (is_utf8_following_byte(*ptr))
    {
        d.push_front(*ptr);
        ptr--;
    }
    d.push_front(*ptr);
    return utf8_to_ucs4(d);
}

uint32_t get_first_ucs4(std::string s)
{
    char const * ptr = s.c_str();
    if (*ptr == 0) ptr = " ";

    std::deque<uint8_t> d;
    d.push_back(*ptr);
    ptr++;
    while (is_utf8_following_byte(*ptr))
    {
        d.push_back(*ptr);
        ptr++;
    }
    return utf8_to_ucs4(d);
}

int font_atlas::get_word_left_kerning(std::string const & word)
{
    return TTF_GetFontKerningSizeGlyphs(_font, get_last_ucs4(word), ' ');
}

int font_atlas::get_word_right_kerning(std::string const & word)
{
    return TTF_GetFontKerningSizeGlyphs(_font, ' ', get_first_ucs4(word));
}

std::unique_ptr<SDL_Surface, void(*)(SDL_Surface *)> font_atlas::text(std::string t, int max_line_width)
{
    SDL_Surface * result = nullptr;

    if (!t.empty())
    {
        std::vector<std::string> words;

        std::size_t pos;
        std::size_t last_pos = 0;
        while ((pos = t.find(' ', last_pos)) != std::string::npos)
        {
            std::string const & w = t.substr(last_pos, pos - last_pos);
            // ignore multiple spaces
            if (!w.empty())
            {
                words.push_back(w);
            }
            last_pos = pos + 1;
        }
        {
            std::string const & w = t.substr(last_pos);
            if (!w.empty())
                words.push_back(w);
        }

        std::vector<SDL_Surface *> surfaces;

        // render or load cached word surfaces
        for (auto const & w : words)
        {
            SDL_Surface * s = word(w);
            surfaces.push_back(s);
        }
        if (!surfaces.empty())
        {
            std::vector<int> widths{surfaces[0]->w};
            std::vector<std::vector<int>> spaces_per_line{{0}};

            // TODO check if surface width does exceed line width ?
            std::vector<std::vector<SDL_Surface *>> surfaces_per_line{{surfaces[0]}};

            // assertion: surfaces.size() == words.size()
            for (std::size_t n = 0; n + 1 < words.size(); n++)
            {
                int & line_width = widths.back();

                // use proper kerning and advance of space to connect words
                int const join_width = get_word_left_kerning(words[n]) + _space_advance + get_word_right_kerning(words[n + 1]);

                int const new_line_width = line_width + join_width + surfaces[n + 1]->w;

                if (max_line_width == -1 || new_line_width < max_line_width)
                {
                    surfaces_per_line.back().push_back(surfaces[n + 1]);
                    spaces_per_line.back().push_back(join_width);
                    line_width = new_line_width;
                }
                else
                {
                    // TODO check if surface width does exceed line width ?
                    surfaces_per_line.push_back({surfaces[n + 1]});
                    spaces_per_line.push_back({0});
                    widths.push_back(surfaces[n + 1]->w);
                }
            }

            int const target_width = *std::max_element(widths.begin(), widths.end());
            int const target_height = font_line_skip() * static_cast<int>(widths.size());

            // TODO move and use create_similar_surface
            result = SDL_CreateRGBSurfaceWithFormat(0, target_width, target_height, 32, SDL_PIXELFORMAT_RGBA32);

            int x = 0;
            int y = 0;

            for (std::size_t n = 0; n < surfaces_per_line.size(); n++)
            {
                // blit line
                for (std::size_t m = 0; m < surfaces_per_line[n].size(); m++)
                {
                    SDL_Surface * s = surfaces_per_line[n][m];

                    // use no alpha blending for source, completely overwrite the target, including alpha channel
                    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);

                    x += spaces_per_line[n][m];
                    SDL_Rect r {x, y, s->w, s->h};
                    SDL_BlitSurface(s, nullptr, result, &r);
                    x += s->w;
                }
                x = 0;
                y += font_line_skip();
            }

            // use alpha blending supplied by the blitted surfaces' alpha channel
            SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
        }
    }

    if (result == nullptr)
        result = SDL_CreateRGBSurfaceWithFormat(0, 0, height(), 32, SDL_PIXELFORMAT_RGBA32);
    return std::unique_ptr<SDL_Surface, void(*)(SDL_Surface*)>(result, [](SDL_Surface * s){ SDL_FreeSurface(s); });
}

SDL_Surface * font_atlas::word(std::string w)
{
    // FIXME: temporary solution - do not use too much memory, although with words caching it's not as bad as before
    if (_prerendered.size() > 40000) _prerendered.clear();

    auto it = _prerendered.find(w);
    if (it == _prerendered.end())
    {
        return _prerendered[w] = TTF_RenderUTF8_Blended(_font, w.c_str(), {255, 255, 255});
    }
    else
    {
        return it->second;
    }
}

unsigned int font_atlas::height()
{
    return TTF_FontHeight(_font);
}

int font_atlas::font_line_skip()
{
    return TTF_FontLineSkip(_font);
}
