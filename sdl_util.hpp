#pragma once

#include <memory>

#include <SDL2/SDL.h>

struct surface_deleter
{
    void operator()(SDL_Surface * s);
};

typedef std::unique_ptr<SDL_Surface, surface_deleter> unique_surface_ptr;

void print_rect(SDL_Rect const & r);

// create a surface with similar properties
SDL_Surface * create_surface(uint32_t pfe, int width, int height);
SDL_Surface * create_surface(SDL_PixelFormat const * pf, int width, int height);

// blit a surface to another surface while preserving aspect ratio
void blit_preserve_ar(SDL_Surface * source, SDL_Surface * dest, SDL_Rect const * destrect);

