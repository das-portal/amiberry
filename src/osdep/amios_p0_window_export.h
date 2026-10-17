#pragma once

#include <SDL3/SDL.h>

// AmiOS P0 is deliberately opt-in. It mirrors ordinary Intuition windows
// without replacing Amiberry's rendering, input, or Picasso96 state.
bool amios_p0_export_enabled();
void amios_p0_export_present(int monid, SDL_Surface* rtg_surface, bool p96_active);
bool amios_p0_export_event(const SDL_Event& event);
