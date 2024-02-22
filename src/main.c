//doomgeneric for cross-platform development library 'Simple DirectMedia Layer'

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <unistd.h>

#include <stdbool.h>
#include <SDL.h>

#include <sys/socket.h>
#include <sys/un.h>

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Surface* matrix_surface = NULL;
SDL_Surface* window_surface = NULL;

#define KEYQUEUE_SIZE 16

#define MATRIX_WIDTH 128
#define MATRIX_HEIGHT 64

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

#define TOTAL_BYTES (128*128 + 4*128)*3
#define SOCKET_NAME "/tmp/screen.socket"

int sock_client;
int sock_ret;
struct sockaddr_un addr;
uint8_t send_buf[TOTAL_BYTES];


static unsigned char convertToDoomKey(unsigned int key){
  // keypad bindings:
  // backspace - escape
  // 8456 - ULDR
  // 1 - strafe
  // del - fire
  // 9 - use
  // 3 - run
  // minus - y

  switch (key)
    {
    case SDLK_KP_MINUS:
      key = 'y';
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      key = KEY_ENTER;
      break;
    case SDLK_ESCAPE:
    case SDLK_KP_BACKSPACE:
    case SDLK_BACKSPACE:
      key = KEY_ESCAPE;
      break;
    case SDLK_LEFT:
    case SDLK_KP_4:
      key = KEY_LEFTARROW;
      break;
    case SDLK_RIGHT:
    case SDLK_KP_6:
      key = KEY_RIGHTARROW;
      break;
    case SDLK_UP:
    case SDLK_KP_8:
      key = KEY_UPARROW;
      break;
    case SDLK_DOWN:
    case SDLK_KP_5:
      key = KEY_DOWNARROW;
      break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_KP_PERIOD:
      key = KEY_FIRE;
      break;
    case SDLK_SPACE:
    case SDLK_KP_9:
      key = KEY_USE;
      break;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_KP_3:
      key = KEY_RSHIFT;
      break;
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_KP_1:
      key = KEY_LALT;
      break;
    case SDLK_F2:
      key = KEY_F2;
      break;
    case SDLK_F3:
      key = KEY_F3;
      break;
    case SDLK_F4:
      key = KEY_F4;
      break;
    case SDLK_F5:
      key = KEY_F5;
      break;
    case SDLK_F6:
      key = KEY_F6;
      break;
    case SDLK_F7:
      key = KEY_F7;
      break;
    case SDLK_F8:
      key = KEY_F8;
      break;
    case SDLK_F9:
      key = KEY_F9;
      break;
    case SDLK_F10:
      key = KEY_F10;
      break;
    case SDLK_F11:
      key = KEY_F11;
      break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
      key = KEY_EQUALS;
      break;
    case SDLK_MINUS:
      key = KEY_MINUS;
      break;
    default:
      key = tolower(key);
      break;
    }

  return key;
}

static void addKeyToQueue(int pressed, unsigned int keyCode){
  unsigned char key = convertToDoomKey(keyCode);

  unsigned short keyData = (pressed << 8) | key;

  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}
static void handleKeyInput(){
  SDL_Event e;
  while (SDL_PollEvent(&e)){
    if (e.type == SDL_QUIT){
      puts("Quit requested");
      atexit(SDL_Quit);
      exit(1);
    }
    if (e.type == SDL_KEYDOWN) {
      //KeySym sym = XKeycodeToKeysym(s_Display, e.xkey.keycode, 0);
      //printf("KeyPress:%d sym:%d\n", e.xkey.keycode, sym);
      addKeyToQueue(1, e.key.keysym.sym);
    } else if (e.type == SDL_KEYUP) {
      //KeySym sym = XKeycodeToKeysym(s_Display, e.xkey.keycode, 0);
      //printf("KeyRelease:%d sym:%d\n", e.xkey.keycode, sym);
      addKeyToQueue(0, e.key.keysym.sym);
    }
  }
}


void DG_Init(){
  window = SDL_CreateWindow("DOOM",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            DOOMGENERIC_RESX,
                            DOOMGENERIC_RESY,
                            SDL_WINDOW_SHOWN
                            );

  window_surface = SDL_GetWindowSurface(window); // get window surface
  matrix_surface = SDL_CreateRGBSurface(0, MATRIX_WIDTH, MATRIX_HEIGHT, 32, 0, 0, 0, 0); // create matrix surface

  SDL_SetSurfaceBlendMode(window_surface, SDL_BLENDMODE_NONE); // disable alpha blending
  SDL_SetSurfaceBlendMode(matrix_surface, SDL_BLENDMODE_NONE);

  // Setup renderer
  renderer =  SDL_CreateSoftwareRenderer(window_surface);
}

void socketSend(){
  uint32_t *pix = matrix_surface->pixels;
  int i = 0;

  for(int y = 0; y < MATRIX_HEIGHT; y++){
    for(int x = 0; x < MATRIX_WIDTH; x++){
      
      send_buf[i] = *pix >> 16;   // R
      send_buf[i+(MATRIX_WIDTH*3)] = *pix >> 16;   // R
      i++;
      send_buf[i] = *pix >> 8;    // G
      send_buf[i+(MATRIX_WIDTH*3)] = *pix >> 8;    // G
      i++;
      send_buf[i] = *pix & 0xFF;  // B
      send_buf[i+(MATRIX_WIDTH*3)] = *pix & 0xFF;    // B
      i++;


      pix++;
    }
    i += (MATRIX_WIDTH*3);
  }


  // send corner pixel color to LEDs
  pix--; // go back to last pixel. probably not safe code

  for(int l = 0; l < 128*4; l++){
    send_buf[i] = *pix >> 16; // R
    i++;
    send_buf[i] = *pix >> 8; // G
    i++;
    send_buf[i] = *pix & 0xFF; // B
    i++;
  }

  sock_ret = write(sock_client, send_buf, TOTAL_BYTES);
}


void DG_DrawFrame()
{
  memcpy(window_surface->pixels, DG_ScreenBuffer, sizeof(uint32_t)*DOOMGENERIC_RESX*DOOMGENERIC_RESY); // quickly copy doom framebuffer to window surface
  SDL_BlitScaled(window_surface, NULL, matrix_surface, NULL);
  SDL_UpdateWindowSurface(window);

  socketSend();

  handleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{
  SDL_Delay(ms);
}

uint32_t DG_GetTicksMs()
{
  return SDL_GetTicks();
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex){
    //key queue is empty
    return 0;
  }else{
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }

  return 0;
}

void DG_SetWindowTitle(const char * title)
{
  if (window != NULL){
    SDL_SetWindowTitle(window, title);
  }
}

int main(int argc, char **argv)
{
  // create socket
  sock_client = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sock_client == -1){
    perror("socket");
    exit(1);
  }

  // connect socket to address
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOCKET_NAME);

  sock_ret = connect(sock_client, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
  if (sock_ret == -1) {
        fprintf(stderr, "Unable to connect to socket.\n");
        exit(1);
  }

  doomgeneric_Create(argc, argv);

  for (int i = 0; ; i++)
  {
      doomgeneric_Tick();
  }
  

  return 0;
}