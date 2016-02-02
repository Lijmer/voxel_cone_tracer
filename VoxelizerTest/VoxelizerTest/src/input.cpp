#include "input.h"
#include <cstring> //memset

bool key[SDL_NUM_SCANCODES];
bool keyDown[SDL_NUM_SCANCODES];
bool keyUp[SDL_NUM_SCANCODES];

bool input::GetKey(SDL_Scancode code)
{
  return key[code];
}
bool input::GetKeyDown(SDL_Scancode code)
{
  return keyDown[code];
}
bool input::GetKeyUp(SDL_Scancode code)
{
  return keyUp[code];
}

void input::OnKeyDown(SDL_Scancode code)
{
  key[code] = true;
  keyDown[code] = true;
}
void input::OnKeyUp(SDL_Scancode code)
{
  key[code] = false;
  keyUp[code] = true;
}

void input::OnFrameEnd()
{
  memset(keyDown, 0, sizeof(keyDown));
  memset(keyUp, 0, sizeof(keyUp));
}
