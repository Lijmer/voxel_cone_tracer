#include <iostream>

#include <SDL.h>
#include <gl\glew.h>
#include "app.h"
#include "input.h"
#include "timer.h"

#include <FreeImage.h>

#undef main
int main(int, char**)
{
  //initalize FreeImage
  FreeImage_Initialise();

  //init SDL
  SDL_Init(SDL_INIT_VIDEO);

  //setup an OpenGL context
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  //setup a window
  SDL_Window* window = SDL_CreateWindow(
    "Voxelizer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    SCRWIDTH, SCRHEIGHT, SDL_WINDOW_OPENGL);
  if(!window)
    return 1;
  
  SDL_GL_SetSwapInterval(0);

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if(!glContext)
    return 1;

  //initialize glew
  glewExperimental = true;
  GLenum glewResult = glewInit();
  if(glewResult != GLEW_OK)
    return 1;
  glGetError(); //ignore error
  
  if(!GLEW_VERSION_4_3)
  {
    printf("Not supported!\n");
    __debugbreak();
  }

  if(GLEW_NV_conservative_raster)
    printf("conservative rasterization supported!\n");
  else
    printf("conservative rasterization not supported!\n");


  

  //create the application
  App *app = new App;

  //used to sture the return values from the app object
  int result;

  //init the app
  if((result = app->Init()) != 0)
  {
    printf("error %d occured on app->Init()\n", result);
    __debugbreak();
    return result;
  }
  bool running = true;
  Timer timer;
  timer.Start();
  while(running)
  {
    //handle events
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      switch(event.type)
      {
      case SDL_QUIT:
        running = false;
        break;

      case SDL_KEYDOWN:
        if(event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
          running = false;
        input::OnKeyDown(event.key.keysym.scancode);
        break;
      case SDL_KEYUP:
        input::OnKeyUp(event.key.keysym.scancode);
        break;
      }
    }

    //update the app
    float dt = timer.IntervalMS() / 1000.0f;
    timer.Start();
    if((result = app->Update(dt)) != 0)
    {
      printf("error %d occured on app->Update()\n", result);
      __debugbreak();
      return result;
    }
    //render the app
    if((result = app->Render()) != 0)
    {
      printf("error %d occured on app->Render()\n", result);
      __debugbreak();
      return result;
    }
    //present the rendered stuff on the screen
    SDL_GL_SwapWindow(window);

    //reset input state
    input::OnFrameEnd();
  }


  //shutdown the app
  if((result = app->Shutdown()) != 0)
  {
    printf("error %d occured on app->Shutdown()\n", result);
    __debugbreak();
    return result;
  }
  delete app;

  //clean up SDL stuff
  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();

  //deinit FreImage
  FreeImage_DeInitialise();

  return 0;
}
