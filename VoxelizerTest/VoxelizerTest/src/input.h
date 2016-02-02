#pragma once

#include <SDL.h>

namespace input
{

bool GetKey(SDL_Scancode code);
bool GetKeyDown(SDL_Scancode code);
bool GetKeyUp(SDL_Scancode code);

void OnKeyDown(SDL_Scancode code);
void OnKeyUp(SDL_Scancode code);
void OnFrameEnd();

}
