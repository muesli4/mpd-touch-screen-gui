#pragma once

#include <SDL2/SDL.h>

// create a surface with similar properties
SDL_Surface * create_similar_surface(SDL_Surface const * s, int width, int height);

// blit a surface to another surface while preserving aspect ratio
void blit_preserve_ar(SDL_Surface * source, SDL_Surface * dest, SDL_Rect const * destrect);

