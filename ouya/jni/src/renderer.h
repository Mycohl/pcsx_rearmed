#ifndef RENDERER_H
#define RENDERER_H

#include <pthread.h>
#include <EGL/egl.h> // requires ndk r5 or newer
//~ #include <GLES/gl.h>

#include <stdbool.h>


enum RenderShaderType {
	RENDER_SHADER_DEFAULT = 0,
	RENDER_SHADER_SLHUNTERK,
	RENDER_SHADER_SLTHEMAISTER,
	RENDER_SHADER_SLCGWG,
	RENDER_SHADER_DUMMY
};

enum RenderThreadMessage {
	RENDER_MSG_NONE = 0,
	RENDER_MSG_WINDOW_SET,
	RENDER_MSG_SET_MESSAGE,
	RENDER_MSG_UPDATE_SHADER,
	RENDER_MSG_RENDER_LOOP_EXIT,
	RENDER_MSG_RENDER_LOOP_PAUSE,
	RENDER_MSG_RENDER_LOOP_UNPAUSE,
	RENDER_MSG_LOAD_GAME,
	RENDER_MSG_UNLOAD_GAME,
	RENDER_MSG_RESET_GAME,
	RENDER_MSG_CHANGE_DISC,
	RENDER_MSG_SAVE_STATE,
	RENDER_MSG_LOAD_STATE
};

struct Renderer {

    pthread_t _renderThreadId;
    pthread_mutex_t _renderMutex;

    enum RenderThreadMessage _msg;
	
    // android window, supported by NDK r5 and newer
    ANativeWindow* _window;

    EGLDisplay* _display;
    EGLSurface* _surface;
    //EGLSurface* _pbuffer;
    EGLContext* _context;
    
	EGLint _width;
    EGLint _height;
    
	bool _thread_running;

	bool _okay_to_step;
	bool _paused;
	
	int _state_slot;
	
	//~ struct timeval _lastFrame;

	char* _rompath;
	
};
typedef struct Renderer Renderer;


void Renderer_Init(Renderer *renderer);
void Renderer_Exit(Renderer *renderer);
void Renderer_SetWindow(Renderer *renderer, ANativeWindow* window);
void Renderer_SetMessage(Renderer *renderer, const char* msg, int frames);
void Renderer_UpdateShader(Renderer *renderer);
void Renderer_Pause(Renderer *renderer);
void Renderer_UnPause(Renderer *renderer);
void Renderer_LoadGame(Renderer *renderer, const char* path);
void Renderer_UnloadGame(Renderer *renderer);
void Renderer_ResetGame(Renderer *renderer);
void Renderer_ChangeDisc(Renderer *renderer, const char* path);
void Renderer_SaveState(Renderer *renderer, int slot);
void Renderer_LoadState(Renderer *renderer, int slot);

#endif // RENDERER_H
