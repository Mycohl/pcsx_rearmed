#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <EGL/egl.h> // requires ndk r5 or newer
#include <GLES2/gl2.h>

#include "jniapi.h"
#include "logger.h"
#include "renderer.h"
#include "../../../frontend/ouya.h"

#include "shaders.h"


static bool _pcsx_initialized;
static char _pcsx_msg[256];
static unsigned _pcsx_msg_frames;
//static bool _pcsx_msg_new;
static struct ouya_message _tmp_pmsg;
static int _shader_type;
static int _scanline_level;

void _render_callback(const void *data, unsigned width, unsigned height, size_t pitch, const void *ptr);
extern bool _settings_callback(unsigned cmd, void *data);
extern void _input_poll_callback();
extern int16_t _input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id);

extern void _clear_settings_updated();

void* _render_thread_start_callback(void *rend);

bool _init_gl_renderer(Renderer *renderer);
bool _init_anative_renderer(Renderer *renderer);
void _destroy_gl_renderer(Renderer *renderer);
void _destroy_anative_renderer(Renderer *renderer);

void _render_loop(Renderer *renderer);
void _clear_display(Renderer *renderer);

void _set_pcsx_message(const char *text, unsigned frames);

void _update_shader();

static void* _tmp_texture_mem;
static GLuint _psx_vout_texID;
static GLuint _fb_texID;
static GLuint _fboID;

static GLuint _vshHandle;
static GLuint _fshHandle;

void Renderer_Init(Renderer *renderer)
{    
	if (renderer == NULL)
		return;

    renderer->_display = NULL;
    renderer->_surface = NULL;
    renderer->_context = NULL;
    
	renderer->_width = 0;
    renderer->_height = 0;
    
	renderer->_paused = false;
	// not okay to step until we load a game
	renderer->_okay_to_step = false;

	//~ gettimeofday(&renderer->_lastFrame, NULL);

	_pcsx_initialized = false;

    pthread_mutex_init(&renderer->_renderMutex, 0);    

    LOG_DEBUG("Creating renderer thread");
    pthread_create(&renderer->_renderThreadId, 0, _render_thread_start_callback, renderer);

    return;
}

void Renderer_Exit(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    LOG_DEBUG("Stopping renderer thread");

    // send message to render thread to stop rendering
    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_RENDER_LOOP_EXIT;
    pthread_mutex_unlock(&renderer->_renderMutex);    

    pthread_join(renderer->_renderThreadId, 0);
    
    LOG_DEBUG("Renderer thread stopped");

    pthread_mutex_destroy(&renderer->_renderMutex);
    return;
	
}

void Renderer_SetWindow(Renderer *renderer, ANativeWindow *window)
{
	if (renderer == NULL)
		return;

	if (renderer->_thread_running) {
		// notify render thread that window has changed
		pthread_mutex_lock(&renderer->_renderMutex);
		renderer->_msg = RENDER_MSG_WINDOW_SET;
		renderer->_window = window;
		pthread_mutex_unlock(&renderer->_renderMutex);
	} else {
		renderer->_window = window;
	}
    return;
}

void Renderer_SetMessage(Renderer *renderer, const char* msg, int frames)
{
	if (renderer == NULL)
		return;

	static char setmsg[128];
	strncpy(setmsg, msg, 128);
    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_SET_MESSAGE;
    _tmp_pmsg.msg = setmsg;
    _tmp_pmsg.frames = frames;
    pthread_mutex_unlock(&renderer->_renderMutex);    
    return;
}

void Renderer_UpdateShader(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_UPDATE_SHADER;
    pthread_mutex_unlock(&renderer->_renderMutex);    
    return;
}

void Renderer_Pause(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_RENDER_LOOP_PAUSE;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_UnPause(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_RENDER_LOOP_UNPAUSE;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_LoadGame(Renderer *renderer, const char* path)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
	renderer->_rompath = malloc(strlen(path) + 1);
	strcpy(renderer->_rompath, path);
    renderer->_msg = RENDER_MSG_LOAD_GAME;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_UnloadGame(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_UNLOAD_GAME;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_ResetGame(Renderer *renderer)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_RESET_GAME;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_ChangeDisc(Renderer *renderer, const char* path)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
	renderer->_rompath = malloc(strlen(path) + 1);
	strcpy(renderer->_rompath, path);
    renderer->_msg = RENDER_MSG_CHANGE_DISC;
    pthread_mutex_unlock(&renderer->_renderMutex);    

	return;
}

void Renderer_SaveState(Renderer *renderer, int slot)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_SAVE_STATE;
    renderer->_state_slot = slot;
    pthread_mutex_unlock(&renderer->_renderMutex);    
}

void Renderer_LoadState(Renderer *renderer, int slot)
{
	if (renderer == NULL)
		return;

    pthread_mutex_lock(&renderer->_renderMutex);
    renderer->_msg = RENDER_MSG_LOAD_STATE;
    renderer->_state_slot = slot;
    pthread_mutex_unlock(&renderer->_renderMutex);    
}

void _gl_print_shader_log(GLuint shader)
{
    if (!glIsShader(shader)) {
		LOG_ERROR("_gl_print_shader_log(): shader %d is not a valid shader", shader);
		return;
	}

	// shader log length
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	// get info string length
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

	// allocate string
	char* infoLog = malloc(maxLength);

	// get info log
	glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0) {
		LOG_INFO("SHADER LOG:\n%s\n", infoLog);
	}

	// deallocate string
	free(infoLog);
}

void _gl_print_program_log(GLuint program)
{
    if (!glIsProgram(program)) {
		LOG_ERROR("_gl_print_program_log(): program %d is not a valid program", program);
		return;
	}

	// program log length
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	// get info string length
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

	// allocate string
	char* infoLog = malloc(maxLength);

	// get info log
	glGetProgramInfoLog(program, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0) {
		LOG_INFO("PROGRAM LOG:\n%s\n", infoLog);
	}

	// deallocate string
	free(infoLog);
}

void* _render_thread_start_callback(void *rend)
{
    _render_loop((Renderer*)rend);
    pthread_exit(0);
    
    return 0;
}

void _render_loop(Renderer *renderer)
{
    bool renderingEnabled = true;

	long loopCount = 0;
    
    LOG_DEBUG("Render loop entered");
    
    if (renderer == NULL) {
		// something bad happened
		LOG_ERROR("Renderer is NULL!");
		return;
	}

	renderer->_thread_running = true;

	if (renderer->_window != NULL)
		renderer->_msg = RENDER_MSG_WINDOW_SET;

	// set PCSX callbacks
	pcsx_ouya_set_video_callback(_render_callback, renderer);
	pcsx_ouya_set_input_poll_callback(_input_poll_callback);
	pcsx_ouya_set_input_state_callback(_input_state_callback);
	pcsx_ouya_prep_settings(_settings_callback);

    while (renderingEnabled) {

        pthread_mutex_lock(&renderer->_renderMutex);
        

        // process incoming messages
        switch (renderer->_msg) {
            case RENDER_MSG_WINDOW_SET:
				if (renderer->_window != NULL) {
					if (!_init_gl_renderer(renderer)) {
						LOG_ERROR("GL initialization failed!");
					}
				} else {
					_destroy_gl_renderer(renderer);
				}
                break;
            case RENDER_MSG_SET_MESSAGE:
				_set_pcsx_message(_tmp_pmsg.msg, _tmp_pmsg.frames);
				break;
            case RENDER_MSG_UPDATE_SHADER:
				_update_shader();
				break;
            case RENDER_MSG_RENDER_LOOP_EXIT:
                renderingEnabled = false;
                //~ renderer->_window = NULL;
                renderer->_okay_to_step = false;
                if (_pcsx_initialized) {
					pcsx_ouya_deinit();
					_pcsx_initialized = false;
				}
                _destroy_gl_renderer(renderer);
                break;
            case RENDER_MSG_RENDER_LOOP_PAUSE:
				renderer->_paused = true;
				break;
            case RENDER_MSG_RENDER_LOOP_UNPAUSE:
				renderer->_paused = false;
				break;
            case RENDER_MSG_LOAD_GAME:
				if (renderer->_rompath != NULL) {
					// some settings
					//~ Settings_Update();
					_clear_settings_updated();
					_pcsx_msg_frames = 0;
					//_pcsx_msg_new = false;
					struct ouya_message msg = {"Loading...", 90};
					_settings_callback(SETTINGS_SET_MESSAGE, (void*)&msg);

					_update_shader();

					// check if we need to unload first
					if (renderer->_okay_to_step) {
						_clear_display(renderer);

						if (_pcsx_initialized) {
							pcsx_ouya_deinit();
							_pcsx_initialized = false;
						}
					}

					LOG_DEBUG("Loading game: %s", renderer->_rompath);
					//~ usleep(20);

					// init PCSX
					if (!_pcsx_initialized) {
						pcsx_ouya_init();
						LOG_DEBUG("PCSX Initialized from render_loop");
						_pcsx_initialized = true;
					}

					// load the game here
					if (strcmp(renderer->_rompath, "BIOS") == 0) {
						// booting BIOS
						if (pcsx_ouya_run_bios()) {
							renderer->_okay_to_step = true;
							renderer->_paused = false;
							LOG_DEBUG("BIOS has been loaded");
						} else {
							pcsx_ouya_deinit();
							_pcsx_initialized = false;
							_clear_display(renderer);
							renderer->_okay_to_step = false;
							JNI_LoadError("BIOS");
							LOG_ERROR("Error loading BIOS!");
						}
					} else if (strcasecmp(renderer->_rompath+(strlen(renderer->_rompath)-4), ".exe") == 0) {
						// booting EXE
						if (pcsx_ouya_load_exe(renderer->_rompath)) {
							renderer->_okay_to_step = true;
							renderer->_paused = false;
							LOG_DEBUG("EXE has been loaded");
						} else {
							pcsx_ouya_deinit();
							_pcsx_initialized = false;
							_clear_display(renderer);
							renderer->_okay_to_step = false;
							JNI_LoadError(renderer->_rompath);
							LOG_ERROR("Error loading EXE!");
						}
					} else {
						// booting a CD image
						if (pcsx_ouya_load_game(renderer->_rompath)) {
							renderer->_okay_to_step = true;
							renderer->_paused = false;
							LOG_DEBUG("Game has been loaded");
						} else {
							pcsx_ouya_deinit();
							_pcsx_initialized = false;
							_clear_display(renderer);
							renderer->_okay_to_step = false;
							JNI_LoadError(renderer->_rompath);
							LOG_ERROR("Error loading game!");
						}
					}
					
					free(renderer->_rompath);
					renderer->_rompath = NULL;
				}
				break;
            case RENDER_MSG_UNLOAD_GAME:
				LOG_DEBUG("Unloading game");
				if (_pcsx_initialized) {
					pcsx_ouya_deinit();
					_pcsx_initialized = false;
				}
				_clear_display(renderer);
				renderer->_okay_to_step = false;
				break;
            case RENDER_MSG_RESET_GAME:
				renderer->_paused = false;
				pcsx_ouya_reset();
				break;
            case RENDER_MSG_CHANGE_DISC:
				if (renderer->_rompath != NULL) {
					pcsx_ouya_change_disc(renderer->_rompath);
					free(renderer->_rompath);
					renderer->_rompath = NULL;
				}
				break;
            case RENDER_MSG_SAVE_STATE:
				renderer->_paused = false;
				pcsx_ouya_save_state(renderer->_state_slot);
				break;
            case RENDER_MSG_LOAD_STATE:
				renderer->_paused = false;
				pcsx_ouya_load_state(renderer->_state_slot);
				break;
            default:
                break;
        }
        renderer->_msg = RENDER_MSG_NONE;
        
        // if we are cleared for rendering, tell PCSX to step
        if (renderer->_window && renderer->_okay_to_step && !(renderer->_paused)) {

			pcsx_ouya_step();

			// message check moved to render callback
			
			// let UI thread do some boring shit
			pthread_mutex_unlock(&renderer->_renderMutex);
			sched_yield();
			usleep(5);
			pthread_mutex_lock(&renderer->_renderMutex);

        } else {
			// not rendering, so relax
			pthread_mutex_unlock(&renderer->_renderMutex);
			sched_yield();
			usleep(5);
			pthread_mutex_lock(&renderer->_renderMutex);

			loopCount++;
			if (loopCount == 300000) {
				loopCount = 0;
				LOG_DEBUG("Not rendering | renderer->_okay_to_step: %d | renderer->_paused: %d",
							renderer->_okay_to_step, renderer->_paused);
			}
		}
		
        pthread_mutex_unlock(&renderer->_renderMutex);
    }
    renderer->_thread_running = false;
    
    LOG_DEBUG("Render loop exited");
    
    return;
}

static int _gl_have_error(const char *name)
{
	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		LOG_ERROR("GL error: %s %x\n", name, e);
		return 1;
	}
	return 0;
}

bool _load_shaders()
{
	GLint status;

//=====================================================================
//  DEFAULT SHADER
//=====================================================================
	_vshHandle = glCreateShader(GL_VERTEX_SHADER);
	if (_gl_have_error("glCreateShader") || _vshHandle == 0) {
		return false;
	}
	glShaderSource(_vshHandle, 1, &_default_vsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}
	glCompileShader(_vshHandle);
	// Get the compilation status.		
	glGetShaderiv(_vshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}

	_fshHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (_gl_have_error("glCreateShader") || _fshHandle == 0) {
		return false;
	}
	glShaderSource(_fshHandle, 1, &_default_fsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}
	glCompileShader(_fshHandle);
	// Get the compilation status.		
	glGetShaderiv(_fshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}

	_default_handles.program = glCreateProgram();

	if (_gl_have_error("glCreateProgram") || _default_handles.program == 0) {
		return false;
	}

	glAttachShader(_default_handles.program, _vshHandle);
	glAttachShader(_default_handles.program, _fshHandle);
	if (_gl_have_error("glAttachShader")) {
		return false;
	}

	glBindAttribLocation(_default_handles.program, 0, "a_Position");
	glBindAttribLocation(_default_handles.program, 1, "a_TexCoord");
	if (_gl_have_error("glBindAttribLocation")) {
		return false;
	}

	glLinkProgram(_default_handles.program);
	// Get the link status.
	glGetProgramiv(_default_handles.program, GL_LINK_STATUS, &status);
	if (_gl_have_error("glLinkProgram") || status != GL_TRUE) {
		return false;
	}

	glDeleteShader(_vshHandle);
	glDeleteShader(_fshHandle);

	_default_handles.position = glGetAttribLocation(_default_handles.program, "a_Position");
	if (_gl_have_error("glGetAttribLocation") || _default_handles.position == -1) {
		return false;
	}
	_default_handles.texcoord = glGetAttribLocation(_default_handles.program, "a_TexCoord");
	if (_gl_have_error("glGetAttribLocation") || _default_handles.texcoord == -1) {
		return false;
	}
	_default_handles.texunit = glGetUniformLocation(_default_handles.program, "u_TexUnit");
	if (_gl_have_error("glGetUniformLocation") || _default_handles.texunit == -1) {
		return false;
	}
	LOG_DEBUG("default shader loaded");

//=====================================================================
//  CGWG SHADER
//=====================================================================
	_vshHandle = glCreateShader(GL_VERTEX_SHADER);
	if (_gl_have_error("glCreateShader") || _vshHandle == 0) {
		return false;
	}
	glShaderSource(_vshHandle, 1, &_slcgwg_vsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}
	glCompileShader(_vshHandle);
	// Get the compilation status.		
	glGetShaderiv(_vshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}

	_fshHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (_gl_have_error("glCreateShader") || _fshHandle == 0) {
		return false;
	}
	glShaderSource(_fshHandle, 1, &_slcgwg_fsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}
	glCompileShader(_fshHandle);
	// Get the compilation status.		
	glGetShaderiv(_fshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}

	_slcgwg_handles.program = glCreateProgram();

	if (_gl_have_error("glCreateProgram") || _slcgwg_handles.program == 0) {
		return false;
	}

	glAttachShader(_slcgwg_handles.program, _vshHandle);
	glAttachShader(_slcgwg_handles.program, _fshHandle);
	if (_gl_have_error("glAttachShader")) {
		return false;
	}

	glBindAttribLocation(_slcgwg_handles.program, 0, "a_Position");
	glBindAttribLocation(_slcgwg_handles.program, 1, "a_TexCoord");
	if (_gl_have_error("glBindAttribLocation")) {
		return false;
	}

	glLinkProgram(_slcgwg_handles.program);
	// Get the link status.
	glGetProgramiv(_slcgwg_handles.program, GL_LINK_STATUS, &status);
	if (_gl_have_error("glLinkProgram") || status != GL_TRUE) {
		return false;
	}

	glDeleteShader(_vshHandle);
	glDeleteShader(_fshHandle);

	_slcgwg_handles.position = glGetAttribLocation(_slcgwg_handles.program, "a_Position");
	if (_gl_have_error("glGetAttribLocation") || _slcgwg_handles.position == -1) {
		return false;
	}
	_slcgwg_handles.texcoord = glGetAttribLocation(_slcgwg_handles.program, "a_TexCoord");
	if (_gl_have_error("glGetAttribLocation") || _slcgwg_handles.texcoord == -1) {
		return false;
	}
	_slcgwg_handles.texunit = glGetUniformLocation(_slcgwg_handles.program, "u_TexUnit");
	if (_gl_have_error("glGetUniformLocation") || _slcgwg_handles.texunit == -1) {
		return false;
	}
	_slcgwg_handles.texsize = glGetUniformLocation(_slcgwg_handles.program, "u_TexSize");
	if (_gl_have_error("glGetUniformLocation") || _slcgwg_handles.texsize == -1) {
		return false;
	}
	_slcgwg_handles.insize = glGetUniformLocation(_slcgwg_handles.program, "u_InSize");
	if (_gl_have_error("glGetUniformLocation") || _slcgwg_handles.insize == -1) {
		return false;
	}
	_slcgwg_handles.outsize = glGetUniformLocation(_slcgwg_handles.program, "u_OutSize");
	if (_gl_have_error("glGetUniformLocation") || _slcgwg_handles.outsize == -1) {
		return false;
	}

	LOG_DEBUG("cgwg shader loaded");

//=====================================================================
//  THEMAISTER SHADER
//=====================================================================
	_vshHandle = glCreateShader(GL_VERTEX_SHADER);
	if (_gl_have_error("glCreateShader") || _vshHandle == 0) {
		return false;
	}
	glShaderSource(_vshHandle, 1, &_slthemaister_vsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}
	glCompileShader(_vshHandle);
	// Get the compilation status.		
	glGetShaderiv(_vshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}

	_fshHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (_gl_have_error("glCreateShader") || _fshHandle == 0) {
		return false;
	}
	glShaderSource(_fshHandle, 1, &_slthemaister_fsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}
	glCompileShader(_fshHandle);
	// Get the compilation status.		
	glGetShaderiv(_fshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}

	_slthemaister_handles.program = glCreateProgram();

	if (_gl_have_error("glCreateProgram") || _slthemaister_handles.program == 0) {
		return false;
	}

	glAttachShader(_slthemaister_handles.program, _vshHandle);
	glAttachShader(_slthemaister_handles.program, _fshHandle);
	if (_gl_have_error("glAttachShader")) {
		return false;
	}

	glBindAttribLocation(_slthemaister_handles.program, 0, "a_Position");
	glBindAttribLocation(_slthemaister_handles.program, 1, "a_TexCoord");
	if (_gl_have_error("glBindAttribLocation")) {
		return false;
	}

	glLinkProgram(_slthemaister_handles.program);
	// Get the link status.
	glGetProgramiv(_slthemaister_handles.program, GL_LINK_STATUS, &status);
	if (_gl_have_error("glLinkProgram") || status != GL_TRUE) {
		return false;
	}

	glDeleteShader(_vshHandle);
	glDeleteShader(_fshHandle);

	_slthemaister_handles.position = glGetAttribLocation(_slthemaister_handles.program, "a_Position");
	if (_gl_have_error("glGetAttribLocation") || _slthemaister_handles.position == -1) {
		return false;
	}
	_slthemaister_handles.texcoord = glGetAttribLocation(_slthemaister_handles.program, "a_TexCoord");
	if (_gl_have_error("glGetAttribLocation") || _slthemaister_handles.texcoord == -1) {
		return false;
	}
	_slthemaister_handles.texunit = glGetUniformLocation(_slthemaister_handles.program, "u_TexUnit");
	if (_gl_have_error("glGetUniformLocation") || _slthemaister_handles.texunit == -1) {
		return false;
	}
	_slthemaister_handles.texsize = glGetUniformLocation(_slthemaister_handles.program, "u_TexSize");
	if (_gl_have_error("glGetUniformLocation") || _slthemaister_handles.texsize == -1) {
		return false;
	}
	_slthemaister_handles.insize = glGetUniformLocation(_slthemaister_handles.program, "u_InSize");
	if (_gl_have_error("glGetUniformLocation") || _slthemaister_handles.insize == -1) {
		return false;
	}
	_slthemaister_handles.outsize = glGetUniformLocation(_slthemaister_handles.program, "u_OutSize");
	if (_gl_have_error("glGetUniformLocation") || _slthemaister_handles.outsize == -1) {
		return false;
	}

	LOG_DEBUG("themaister shader loaded");

//=====================================================================
//  HUNTERK SHADER
//=====================================================================
	_vshHandle = glCreateShader(GL_VERTEX_SHADER);
	if (_gl_have_error("glCreateShader") || _vshHandle == 0) {
		return false;
	}
	glShaderSource(_vshHandle, 1, &_slhunterk_vsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}
	glCompileShader(_vshHandle);
	// Get the compilation status.		
	glGetShaderiv(_vshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_vshHandle);
		return false;
	}

	_fshHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (_gl_have_error("glCreateShader") || _fshHandle == 0) {
		return false;
	}
	glShaderSource(_fshHandle, 1, &_slhunterk_fsh_source, NULL);
	if (_gl_have_error("glShaderSource")) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}
	glCompileShader(_fshHandle);
	// Get the compilation status.		
	glGetShaderiv(_fshHandle, GL_COMPILE_STATUS, &status);
	if (_gl_have_error("glCompileShader") || status != GL_TRUE) {
		_gl_print_shader_log(_fshHandle);
		return false;
	}

	_slhunterk_handles.program = glCreateProgram();

	if (_gl_have_error("glCreateProgram") || _slhunterk_handles.program == 0) {
		return false;
	}

	glAttachShader(_slhunterk_handles.program, _vshHandle);
	glAttachShader(_slhunterk_handles.program, _fshHandle);
	if (_gl_have_error("glAttachShader")) {
		return false;
	}

	glBindAttribLocation(_slhunterk_handles.program, 0, "a_Position");
	glBindAttribLocation(_slhunterk_handles.program, 1, "a_TexCoord");
	if (_gl_have_error("glBindAttribLocation")) {
		return false;
	}

	glLinkProgram(_slhunterk_handles.program);
	// Get the link status.
	glGetProgramiv(_slhunterk_handles.program, GL_LINK_STATUS, &status);
	if (_gl_have_error("glLinkProgram") || status != GL_TRUE) {
		return false;
	}

	glDeleteShader(_vshHandle);
	glDeleteShader(_fshHandle);

	_slhunterk_handles.position = glGetAttribLocation(_slhunterk_handles.program, "a_Position");
	if (_gl_have_error("glGetAttribLocation") || _slhunterk_handles.position == -1) {
		return false;
	}
	_slhunterk_handles.texcoord = glGetAttribLocation(_slhunterk_handles.program, "a_TexCoord");
	if (_gl_have_error("glGetAttribLocation") || _slhunterk_handles.texcoord == -1) {
		return false;
	}
	_slhunterk_handles.texunit = glGetUniformLocation(_slhunterk_handles.program, "u_TexUnit");
	if (_gl_have_error("glGetUniformLocation") || _slhunterk_handles.texunit == -1) {
		return false;
	}
	_slhunterk_handles.brightness = glGetUniformLocation(_slhunterk_handles.program, "u_Brightness");
	if (_gl_have_error("glGetUniformLocation") || _slhunterk_handles.brightness == -1) {
		return false;
	}

	LOG_DEBUG("hunterk shader loaded");

	return true;
}

bool _init_egl(Renderer *renderer)
{
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_RED_SIZE, 5,
        //~ EGL_BLUE_SIZE, 8,
        //~ EGL_GREEN_SIZE, 8,
        //~ EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
	EGLConfig* matchingConfigs;
    EGLint format;
    EGLSurface surface;
    EGLContext context;
    EGLint width;
    EGLint height;
    GLfloat ratio;
	
    LOG_DEBUG("Initializing EGL context...");
    
    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOG_ERROR("eglGetDisplay() returned error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(display, 0, 0)) {
        LOG_ERROR("eglInitialize() returned error %d", eglGetError());
        return false;
    }

    //~ if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        //~ LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        //~ _destroy_gl_renderer(renderer);
        //~ return false;
    //~ }

	// Number of matching EGLConfig's returned in numberConfigs, but because
	// the 3rd argument is NULL no configs will actually be returned
	if (!eglChooseConfig(display, attribs, NULL, 0, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        return false;
	}
	if (numConfigs == 0) {
        LOG_ERROR("eglChooseConfig() returned zero configs");
        return false;
	}
 
	// Allocate some space to store list of matching configs... 
	matchingConfigs = (EGLConfig*) malloc(numConfigs * sizeof(EGLConfig));
	// ...and this time actually get the list (notice the 3rd argument is
	// now the buffer to write matching EGLConfig's to)
	if (!eglChooseConfig(display, attribs, matchingConfigs, numConfigs, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
		free(matchingConfigs);
        return false;
	}

	config = NULL;
	// Look at each EGLConfig 
	int i;
	for (i = 0; i < numConfigs; i++) {
		EGLBoolean success;
		EGLint red, green, blue;
		// Read the relevent attributes of the EGLConfig 
		success = eglGetConfigAttrib(display, matchingConfigs[i], EGL_RED_SIZE, &red);
		success &= eglGetConfigAttrib(display, matchingConfigs[i], EGL_BLUE_SIZE, &blue);
		success &= eglGetConfigAttrib(display, matchingConfigs[i], EGL_GREEN_SIZE, &green);
		// Check that no error occurred and the attributes match 
		if (success && (red == 5) && (green == 6) && (blue==5)) {
			config = matchingConfigs[i];
			break;
		} else {
			LOG_DEBUG("Rejected EGLConfig with R: %d G: %d B: %d", red, green, blue);
		}
	}
 
	if (config == NULL) {
		LOG_ERROR("Unable to find a valid EGLConfig");
		free(matchingConfigs);
		return false;
	}
 
	free(matchingConfigs);

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOG_ERROR("eglGetConfigAttrib() returned error %d", eglGetError());
        _destroy_gl_renderer(renderer);
        return false;
    }

    //~ ANativeWindow_setBuffersGeometry(renderer->_window, 0, 0, format);
    ANativeWindow_setBuffersGeometry(renderer->_window, VOUT_MAX_WIDTH, VOUT_MAX_HEIGHT, format);

    if (!(surface = eglCreateWindowSurface(display, config, renderer->_window, 0))) {
        LOG_ERROR("eglCreateWindowSurface() returned error %d", eglGetError());
        _destroy_gl_renderer(renderer);
        return false;
    }
    
	EGLint context_attribs[] = {
		 EGL_CONTEXT_CLIENT_VERSION, 2,
		 EGL_NONE
	};

    if (!(context = eglCreateContext(display, config, NULL, context_attribs))) {
        LOG_ERROR("eglCreateContext() returned error %d", eglGetError());
        _destroy_gl_renderer(renderer);
        return false;
    }
    
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
        _destroy_gl_renderer(renderer);
        return false;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) || !eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
        LOG_ERROR("eglQuerySurface() returned error %d", eglGetError());
        _destroy_gl_renderer(renderer);
        return false;
    }

    renderer->_display = display;
    renderer->_surface = surface;
    renderer->_context = context;
    renderer->_width = width;
    renderer->_height = height;

	if (_gl_have_error("EGL Initialization")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	LOG_DEBUG("EGL initializtion complete");
	return true;
}

bool _init_gl_renderer(Renderer *renderer)
{
	if (!_init_egl(renderer)) {
		LOG_ERROR("EGL initializtion failed!");
		return false;
	}

	_tmp_texture_mem = NULL;
	_psx_vout_texID = 0;
	_fb_texID = 0;


	//~ glEnable(GL_TEXTURE_2D);
	//~ if (_gl_have_error("glEnable(GL_TEXTURE_2D)")) {
		//~ _destroy_gl_renderer(renderer);
		//~ return false;
	//~ }
	

	// generate texture for psx vout
	_tmp_texture_mem = calloc(1, VOUT_MAX_WIDTH * VOUT_MAX_HEIGHT * 2);
	glGenTextures(1, &_psx_vout_texID);
	if (_gl_have_error("glGenTextures")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, _psx_vout_texID);
	if (_gl_have_error("glBindTexture")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (_gl_have_error("glTexParameterf")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VOUT_MAX_WIDTH, VOUT_MAX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, _tmp_texture_mem);
	if (_gl_have_error("glTexImage2D")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	free(_tmp_texture_mem);
	_tmp_texture_mem = NULL;

	// generate texture for off-screen framebuffer
	//~ glGenTextures(1, &_fb_texID);
	//~ if (_gl_have_error("glGenTextures")) {
		//~ _destroy_gl_renderer(renderer);
		//~ return false;
	//~ }
	//~ glBindTexture(GL_TEXTURE_2D, _fb_texID);
	//~ if (_gl_have_error("glBindTexture")) {
		//~ _destroy_gl_renderer(renderer);
		//~ return false;
	//~ }
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//~ if (_gl_have_error("glTexParameterf")) {
		//~ _destroy_gl_renderer(renderer);
		//~ return false;
	//~ }
	//~ glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	//~ glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VOUT_MAX_WIDTH, VOUT_MAX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	//~ if (_gl_have_error("glTexImage2D")) {
		//~ _destroy_gl_renderer(renderer);
		//~ return false;
	//~ }

	LOG_DEBUG("GL texture binding complete");

	// create off-screen framebuffer
	glGenFramebuffers(1, &_fboID);
	if (_gl_have_error("glGenFramebuffers")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, _fboID);
	if (_gl_have_error("glBindFramebuffer")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _fb_texID, 0);
	if (_gl_have_error("glFramebufferTexture2D")) {
		_destroy_gl_renderer(renderer);
		return false;
	}

	LOG_DEBUG("GL framebuffer binding complete");
	
	// return to window framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (_gl_have_error("glBindFramebuffer")) {
		_destroy_gl_renderer(renderer);
		return false;
	}

	// other settings
	glFrontFace(GL_CW);
	if (_gl_have_error("glFrontFace")) {
		_destroy_gl_renderer(renderer);
		return false;
	}
    glEnable(GL_CULL_FACE);
	if (_gl_have_error("glEnable(GL_CULL_FACE)")) {
		_destroy_gl_renderer(renderer);
		return false;
	}

    //~ glDisable(GL_DITHER);
    //~ glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    //~ glShadeModel(GL_SMOOTH);
    //~ glEnable(GL_DEPTH_TEST);
    

	if (!_load_shaders()) {
		_destroy_gl_renderer(renderer);
		LOG_ERROR("Loading of shaders failed");
		return false;
	}




//~ #else  // GLES1 init
    //~ glLoadIdentity();
	//~ glEnableClientState(GL_VERTEX_ARRAY);
	//~ glEnableClientState(GL_TEXTURE_COORD_ARRAY);
//~ #endif


	if (_gl_have_error("_init_gl_renderer")) {
		_destroy_gl_renderer(renderer);
		LOG_ERROR("GL initializtion failed");
		return false;
	}

	LOG_DEBUG("GL initialization complete");
    return true;
}

bool _init_anative_renderer(Renderer *renderer)
{
	//ANativeWindow_acquire(renderer->_window);
	ANativeWindow_setBuffersGeometry(renderer->_window, renderer->_width, renderer->_height, WINDOW_FORMAT_RGBA_8888);
	return;
}

void _deinit_egl(Renderer *renderer)
{
	if (renderer->_display == EGL_NO_DISPLAY || renderer->_surface == EGL_NO_SURFACE || renderer->_context == EGL_NO_CONTEXT)
		return;

    LOG_DEBUG("_deinit_egl() - Destroying EGL context");

	EGLBoolean res;
	res = eglMakeCurrent(renderer->_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (!res) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
	}
	res = eglDestroyContext(renderer->_display, renderer->_context);
    if (!res) {
        LOG_ERROR("eglDestroyContext() returned error %d", eglGetError());
	}
	res = eglDestroySurface(renderer->_display, renderer->_surface);
    if (!res) {
        LOG_ERROR("eglDestroySurface() returned error %d", eglGetError());
	}
	res = eglTerminate(renderer->_display);
    if (!res) {
        LOG_ERROR("eglTerminate() returned error %d", eglGetError());
	}
    
    renderer->_display = EGL_NO_DISPLAY;
    renderer->_surface = EGL_NO_SURFACE;
    renderer->_context = EGL_NO_CONTEXT;	
}

void _destroy_gl_renderer(Renderer *renderer) 
{
	if (renderer->_display == EGL_NO_DISPLAY || renderer->_surface == EGL_NO_SURFACE || renderer->_context == EGL_NO_CONTEXT)
		return;

    LOG_DEBUG("_destroy_gl_renderer() - Cleaning up GL structures");

	if (_tmp_texture_mem != NULL) {
		free(_tmp_texture_mem);
		_tmp_texture_mem = NULL;
	}
	if (glIsTexture(_psx_vout_texID)) {
		glDeleteTextures(1, &_psx_vout_texID);
		_psx_vout_texID = 0;
	}
	if (glIsTexture(_fb_texID)) {
		glDeleteTextures(1, &_fb_texID);
		_fb_texID = 0;
	}
	if (glIsFramebuffer(_fboID)) {
		glDeleteFramebuffers(1, &_fboID);
		_fboID = 0;
	}
	if (glIsShader(_vshHandle)) {
		glDeleteShader(_vshHandle);
		_vshHandle = 0;
	}
	if (glIsShader(_fshHandle)) {
		glDeleteShader(_fshHandle);
		_fshHandle = 0;
	}
	
	if (glIsProgram(_default_handles.program)) {
		glDeleteProgram(_default_handles.program);
		_default_handles.program = 0;
	}
	if (glIsProgram(_slhunterk_handles.program)) {
		glDeleteProgram(_slhunterk_handles.program);
		_slhunterk_handles.program = 0;
	}
	if (glIsProgram(_slthemaister_handles.program)) {
		glDeleteProgram(_slthemaister_handles.program);
		_slthemaister_handles.program = 0;
	}
	if (glIsProgram(_slcgwg_handles.program)) {
		glDeleteProgram(_slcgwg_handles.program);
		_slcgwg_handles.program = 0;
	}

	_deinit_egl(renderer);

    return; 
}

void _destroy_anative_renderer(Renderer *renderer) 
{
	return;
}

void _set_pcsx_message(const char *text, unsigned frames)
{
	strncpy(_pcsx_msg, text, 255);
	_pcsx_msg_frames = frames;

	JNI_Set_Message((const char*)&_pcsx_msg);

}

static GLfloat vertices[] = {
	-1.0f,  1.0f,  0.0f, // 0    0  1
	 1.0f,  1.0f,  0.0f, // 1  ^
	-1.0f, -1.0f,  0.0f, // 2  | 2  3
	 1.0f, -1.0f,  0.0f, // 3  +-->
};

static GLfloat gpuTexture[] = {
	0.0f, 0.0f, // we flip this:
	1.0f, 0.0f, // v^
	0.0f, 1.0f, //  |  u
	1.0f, 1.0f, //  +-->
};

static GLfloat fbTexture[] = {
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 0.0f,
};


void _blit_gl(const void *data, unsigned width, unsigned height, Renderer *renderer)
{
	static GLuint prg, pos, tex;
	static int old_w, old_h;
	static int old_shader, old_scanline_level;

	if (renderer == NULL || renderer->_window == NULL || data == NULL)
		return;

	while (_gl_have_error("clearing GL errors"))
		usleep(1);

	// first render from vout data to vout framebuffer
	if (width != old_w || height != old_h) {
		float f_w = (float)width / 1024.0f;
		float f_h = (float)height / 512.0f;
		gpuTexture[1*2 + 0] = f_w;
		gpuTexture[2*2 + 1] = f_h;
		gpuTexture[3*2 + 0] = f_w;
		gpuTexture[3*2 + 1] = f_h;
		old_w = width;
		old_h = height;
		
		old_shader = -1;
	}
	if (_shader_type == RENDER_SHADER_DEFAULT) {
		prg = _default_handles.program;
		pos = _default_handles.position;
		tex = _default_handles.texcoord;
	} else if (_shader_type == RENDER_SHADER_SLHUNTERK) {
		prg = _slhunterk_handles.program;
		pos = _slhunterk_handles.position;
		tex = _slhunterk_handles.texcoord;
	} else if (_shader_type == RENDER_SHADER_SLTHEMAISTER) {
		prg = _slthemaister_handles.program;
		pos = _slthemaister_handles.position;
		tex = _slthemaister_handles.texcoord;
	} else if (_shader_type == RENDER_SHADER_SLCGWG) {
		prg = _slcgwg_handles.program;
		pos = _slcgwg_handles.position;
		tex = _slcgwg_handles.texcoord;
	} else {
		prg = 0;
		pos = 0;
		tex = 0;
	}

	//~ glBindFramebuffer(GL_FRAMEBUFFER, _fboID);
	//~ if (_gl_have_error("glBindFramebuffer"))
		//~ return;
	glUseProgram(prg);
	if (_gl_have_error("glUseProgram"))
		return;
	//~ glBindTexture(GL_TEXTURE_2D, _psx_vout_texID);
	//~ if (_gl_have_error("glBindTexture"))
		//~ return;
	glActiveTexture(GL_TEXTURE0);
	if (_gl_have_error("glActiveTexture"))
		return;

	if (_shader_type != old_shader || _scanline_level != old_scanline_level) {
		// set our uniforms
		if (_shader_type == RENDER_SHADER_DEFAULT) {
			glUniform1i(_default_handles.texunit, 0);
			if (_gl_have_error("glUniform1i(_default_handles.texunit)"))
				return;
		} else if (_shader_type == RENDER_SHADER_SLHUNTERK) {
			glUniform1i(_slhunterk_handles.texunit, 0);
			if (_gl_have_error("glUniform1i(_slhunterk_handles.texunit)"))
				return;
			glUniform1f(_slhunterk_handles.brightness, (float)_scanline_level / 75.0);
			if (_gl_have_error("glUniform1f(_slhunterk_handles.brightness)"))
				return;
		} else if (_shader_type == RENDER_SHADER_SLTHEMAISTER) {
			glUniform1i(_slthemaister_handles.texunit, 0);
			if (_gl_have_error("glUniform1i(_slthemaister_handles.texunit)"))
				return;
			glUniform2f(_slthemaister_handles.texsize, (float)width, (float)height);
			if (_gl_have_error("glUniform2f(_slthemaister_handles.texsize)"))
				return;
			glUniform2f(_slthemaister_handles.insize, (float)width, (float)height);
			if (_gl_have_error("glUniform2f(_slthemaister_handles.insize)"))
				return;
			glUniform2f(_slthemaister_handles.outsize, (float)VOUT_MAX_WIDTH, (float)VOUT_MAX_HEIGHT);
			if (_gl_have_error("glUniform2f(_slthemaister_handles.outsize)"))
				return;
		} else if (_shader_type == RENDER_SHADER_SLCGWG) {
			glUniform1i(_slcgwg_handles.texunit, 0);
			if (_gl_have_error("glUniform1i(_slcgwg_handles.texunit)"))
				return;
			glUniform2f(_slcgwg_handles.texsize, (float)width, (float)height);
			if (_gl_have_error("glUniform2f(_slcgwg_handles.texsize)"))
				return;
			glUniform2f(_slcgwg_handles.insize, (float)width, (float)height);
			if (_gl_have_error("glUniform2f(_slcgwg_handles.insize)"))
				return;
			glUniform2f(_slcgwg_handles.outsize, (float)VOUT_MAX_WIDTH, (float)VOUT_MAX_HEIGHT);
			if (_gl_have_error("glUniform2f(_slcgwg_handles.outsize)"))
				return;
		} else {
			// dummy
		}
		old_shader = _shader_type;
		old_scanline_level = _scanline_level;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
	if (_gl_have_error("glTexSubImage2D"))
		return;

	glViewport(0, 0, VOUT_MAX_WIDTH, VOUT_MAX_HEIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(pos);
	//~ glVertexAttribPointer(tex, 2, GL_FLOAT, GL_FALSE, 0, fbTexture);
	glVertexAttribPointer(tex, 2, GL_FLOAT, GL_FALSE, 0, gpuTexture);
	glEnableVertexAttribArray(tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	if (_gl_have_error("glDrawArrays"))
		return;
	glDisableVertexAttribArray(pos);
	glDisableVertexAttribArray(tex);


	// next draw to window framebuffer using selected shader


	//~ glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//~ if (_gl_have_error("glBindFramebuffer"))
		//~ return;
	//~ glUseProgram(_default_handles.program);
	//~ if (_gl_have_error("glUseProgram"))
		//~ return;
	//~ glActiveTexture(GL_TEXTURE0);
	//~ if (_gl_have_error("glActiveTexture"))
		//~ return;
	//~ glBindTexture(GL_TEXTURE_2D, _fb_texID);
	//~ if (_gl_have_error("glBindTexture"))
		//~ return;
	//~ glUniform1i(_default_handles.texunit, 0);
	
	//~ glViewport(0, 0, VOUT_MAX_WIDTH, VOUT_MAX_HEIGHT);
	//~ glViewport(0, 0, renderer->_width, renderer->_height);
	//~ glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//~ 
	//~ glVertexAttribPointer(_default_handles.position, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	//~ glEnableVertexAttribArray(_default_handles.position);
	//~ glVertexAttribPointer(_default_handles.texcoord, 2, GL_FLOAT, GL_FALSE, 0, gpuTexture);
	//~ glEnableVertexAttribArray(_default_handles.texcoord);
	//~ glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	//~ if (_gl_have_error("glDrawArrays"))
		//~ return;
	//~ glDisableVertexAttribArray(_default_handles.position);
	//~ glDisableVertexAttribArray(_default_handles.texcoord);

	if (renderer == NULL || renderer->_window == NULL)
		return;

	if (!eglSwapBuffers(renderer->_display, renderer->_surface)) {
		EGLint error = eglGetError();
		if (error == EGL_BAD_DISPLAY) {
			//~ _deinit_egl(renderer);
			//~ _init_egl(renderer);
			_destroy_gl_renderer(renderer);
			_init_gl_renderer(renderer);
		} else
			LOG_ERROR("eglSwapBuffers() returned error %d", error);
	}
}

void _blit_anative(const void *data, unsigned width, unsigned height, Renderer *renderer)
{
	if (renderer->_width != width || renderer->_height != height) {
		LOG_DEBUG("Changing buffer size: %d x %d", width, height);
		renderer->_width = width;
		renderer->_height = height;
		ANativeWindow_setBuffersGeometry(renderer->_window, width, height, WINDOW_FORMAT_RGB_565);
	}
	
	ANativeWindow_Buffer buffer;
	ANativeWindow_lock(renderer->_window, &buffer, NULL);
	memcpy(buffer.bits, data,  width * height * 2);
	ANativeWindow_unlockAndPost(renderer->_window);
	
	return;
}

void _render_callback(const void *data, unsigned width, unsigned height, size_t pitch, const void *ptr)
{
	Renderer *renderer = (Renderer*)ptr;
	
	if (renderer == NULL || !(renderer->_okay_to_step))
		return;


	// return if we haven't gotten an updated pixel buffer
	if (data == NULL) {
		//LOG_DEBUG("_render_callback() not rendering due to NULL pixbuf");
		
		pthread_mutex_unlock(&renderer->_renderMutex);
		sched_yield();
		usleep(5);
		pthread_mutex_lock(&renderer->_renderMutex);

		return;
	}
	//LOG_DEBUG("_render_callback() got a non-NULL pixbuf");
	
	
	_blit_gl(data, width, height, renderer);
		
	//renderer->_okay_to_step = true;

	// check our message status
	if (_pcsx_msg_frames > 0) {
		_pcsx_msg_frames--;
		if (_pcsx_msg_frames == 0) {
			//~ LOG_DEBUG("Clearing PCSXMsg");
			JNI_Set_Message("");
			strcpy(_pcsx_msg, "");
		}
	}

}

void _clear_display(Renderer *renderer)
{
	uint16_t data[4] = { 0, 0, 0, 0 };
	_render_callback(data, 2, 2, 0, renderer);
}

void _update_shader()
{
	char buf[32];
	JNI_Get_Pref("pref_pcsx_rearmed_scanlines", buf, sizeof(buf));
	if (strcmp(buf, "none") == 0) {
		_shader_type = RENDER_SHADER_DEFAULT;
		_scanline_level = 0;
	} else {
		_scanline_level = atoi(buf);
		_shader_type = RENDER_SHADER_SLHUNTERK;
	}
}
