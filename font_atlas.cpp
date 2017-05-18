#include <vector>
#include <iostream>

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

std::unique_ptr<SDL_Surface, void(*)(SDL_Surface *)> font_atlas::text(std::string t)
{
    SDL_Surface * result;

    if (t.empty())
    {
        result = SDL_CreateRGBSurfaceWithFormat(0, 0, height(), 32, SDL_PIXELFORMAT_RGBA32);
    }
    else
    {
        std::vector<std::string> words;

        std::size_t pos;
        std::size_t last_pos = 0;
        while ((pos = t.find(' ', last_pos)) != std::string::npos)
        {
            words.push_back(t.substr(last_pos, pos - last_pos));
            last_pos = pos + 1;
        }
        words.push_back(t.substr(last_pos));

        int width = 0;
        std::vector<SDL_Surface *> surfaces;

        for (auto const & w : words)
        {
            // FIXME how to handle double spaces in text?
            SDL_Surface * s = word(w.empty() ? " " : w);
            surfaces.push_back(s);
            width += s->w;
        }

        std::vector<int> spaces;
        spaces.push_back(0);
        for (std::size_t n = 0; n + 1 < words.size(); n++)
        {
            // use proper kerning with space
            int const left_kerning = TTF_GetFontKerningSizeGlyphs(_font, get_last_ucs4(words[n]), ' ');
            int const right_kerning = TTF_GetFontKerningSizeGlyphs(_font, ' ', get_last_ucs4(words[n + 1]));
            int const join_width = (_space_minx < 0 ? -_space_minx : 0) + _space_advance + left_kerning + right_kerning;
            width += join_width;
            spaces.push_back(join_width);
            //std::cout << "word[" << n << "] = \"" << words[n]
            //          << "\",  with join_width " << left_kerning << " + " << _space_advance << " + " << right_kerning
            //          << ", minx = " << minx
            //          << ", maxx = " << maxx
            //          << std::endl;
            //std::wcout << L"    chars in question '" << (wchar_t) get_last_ucs4(words[n]) << L"' and '" << (wchar_t) get_first_ucs4(words[n + 1]) << L"'" << std::endl;
        }
        //std::cout << "spaces" << std::endl;

        // TODO move and use create_similar_surface
        result = SDL_CreateRGBSurfaceWithFormat(0, width, height(), 32, SDL_PIXELFORMAT_RGBA32);

        int x = 0;
        for (std::size_t n = 0; n < surfaces.size(); n++)
        {
            SDL_Surface * s = surfaces[n];

            // use no alpha blending for source, completely overwrite the target, including alpha channel
            SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);

            x += spaces[n];
            SDL_Rect r {x, 0, s->w, s->h};
            SDL_BlitSurface(s, nullptr, result, &r);
            x += s->w;
        }

        // use alpha blending supplied by the blitted surfaces' alpha channel
        SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    }
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
