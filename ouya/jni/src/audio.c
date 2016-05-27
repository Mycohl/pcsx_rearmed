#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>


#include "logger.h"
#include "audio.h"
#include "../../../frontend/ouya.h"

#define AUDIO_MAX_BUFFERS		2
#define AUDIO_BUFFER_SIZE		3072
#define AUDIO_BUFFER_MULT		2
#define AUDIO_RB_EXTRA_BYTES	2


static pthread_mutex_t _audioMutex;
static bool _okay_to_play;
static bool _audio_paused;
static int8_t _enqueue_count;

typedef struct {
    char *data;
    unsigned length;
    unsigned start;
    unsigned end;
    unsigned avail_da;
    unsigned avail_sp;
} RingBuffer;

static RingBuffer *_ringbuffer;

void _rb_create();
void _rb_destroy();
void _rb_reset();
int _rb_read(void *target);
int _rb_write(const void *source, size_t bytes);
//~ int _rb_empty();
//~ int _rb_full();
//~ int _rb_available_data();
//~ int _rb_available_space();
//~ #define _rb_available_data (int)((_ringbuffer->start > _ringbuffer->end) ? ((_ringbuffer->length % _ringbuffer->end) + _ringbuffer->start) + 1 : (_ringbuffer->end - _ringbuffer->start))
//~ #define _rb_available_space (_ringbuffer->length - _rb_available_data - 1)
#define _rb_available_data _ringbuffer->avail_da
#define _rb_available_space _ringbuffer->avail_sp
#define _rb_full (_rb_available_data - _ringbuffer->length == 0)
#define _rb_empty (_rb_available_data == 0)
#define _rb_starts_at (_ringbuffer->data + _ringbuffer->start)
#define _rb_ends_at (_ringbuffer->data + _ringbuffer->end)
//~ #define _rb_last_addr (_ringbuffer->data + _ringbuffer->length)
#define _rb_commit_read(amt) (_ringbuffer->start = (_ringbuffer->start + amt) % _ringbuffer->length)
#define _rb_commit_write(amt) (_ringbuffer->end = (_ringbuffer->end + amt) % _ringbuffer->length)

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;
static SLVolumeItf outputMixVolume = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay = NULL;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
//static SLEffectSendItf bqPlayerEffectSend;
//static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume = NULL;


size_t _audio_batch_callback(const int16_t *data, size_t frames);
void _audio_player_callback(SLAndroidSimpleBufferQueueItf bq, void *context);

//~ void _clear_all_audio_buffers(AudioOut *audio);
//~ 
//~ void* _audio_thread_start_callback(void *aud);

void _init_audio();
void _destroy_audio();
void _pause_unpause_audio();
void _stop_audio();

//~ void _audio_loop(AudioOut *audio);


void Audio_Init()
{
	// set PCSX callbacks
	pcsx_ouya_set_audio_callback(_audio_batch_callback);

    pthread_mutex_init(&_audioMutex, 0);    

    //~ pthread_mutex_lock(&_audioMutex);
	_rb_create();
	_okay_to_play = true;
	_audio_paused = false;
    //~ pthread_mutex_unlock(&_audioMutex);

    return;
}

void Audio_Exit()
{
    //~ pthread_mutex_lock(&_audioMutex);

	_okay_to_play = false;

	_stop_audio();
	_destroy_audio();
	_rb_destroy();

    //~ pthread_mutex_unlock(&_audioMutex);    

    pthread_mutex_destroy(&_audioMutex);
    return;
}

void Audio_Pause()
{
    //~ pthread_mutex_lock(&_audioMutex);

    _audio_paused = true;
    _pause_unpause_audio();

    //~ pthread_mutex_unlock(&_audioMutex);    

	return;
}

void Audio_UnPause()
{
	//~ while (_enqueue_count < 1) {
		//~ usleep(5);
	//~ }
    //~ pthread_mutex_lock(&_audioMutex);

    _audio_paused = false;
    _pause_unpause_audio();

    //~ pthread_mutex_unlock(&_audioMutex);    

	return;
}

void Audio_LoadGame()
{
    //~ pthread_mutex_lock(&_audioMutex);

	_init_audio();
	
	_rb_reset();

	// enqueue buffers as soon as they are available
	//~ audio->_player_needs_data = AUDIO_MAX_BUFFERS;

	_okay_to_play = true;
	_enqueue_count = 0;

    //~ pthread_mutex_unlock(&_audioMutex);    

	return;
}

void Audio_UnloadGame()
{
    //~ pthread_mutex_lock(&_audioMutex);

	_okay_to_play = false;

	sched_yield();

	//~ audio->_player_needs_data = 0;
	_stop_audio();

	//~ _clear_all_audio_buffers(audio);
	//~ (*bqPlayerBufferQueue)->Clear(bqPlayerBufferQueue);
	_destroy_audio();

    //~ pthread_mutex_unlock(&_audioMutex);    

	return;
}

//~ void* _audio_thread_start_callback(void *aud)
//~ {
    //~ _audio_loop((AudioOut*)aud);
    //~ pthread_exit(0);
    //~ 
    //~ return 0;
//~ }

void _init_audio()
{
	// clear out previous buffers if they exist
	//~ _clear_all_audio_buffers();

    SLresult result;

    // create engine
	LOG_DEBUG("Creating audio engine");
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("slCreateEngine() failed!");
		_destroy_audio();
		return;
	}
    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*engineObject)->Realize() failed!");
		_destroy_audio();
		return;
	}
    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*engineObject)->GetInterface() failed!");
		_destroy_audio();
		return;
	}
    // create output mix, with master volume specified as a non-required interface
	LOG_DEBUG("Creating output mix");
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*engineEngine)->CreateOutputMix() failed!");
		_destroy_audio();
		return;
	}
    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*outputMixObject)->Realize() failed!");
		_destroy_audio();
		return;
	}
	// try to get interface for outputmix volume control (optional)
    //~ result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_VOLUME, &outputMixVolume);
    //~ if (result != SL_RESULT_SUCCESS) {
		//~ LOG_DEBUG("Failed to get volume control for output mix");
    //~ }

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq;
    loc_bufq.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    loc_bufq.numBuffers = 4;
    SLDataFormat_PCM format_pcm;
	format_pcm.formatType = SL_DATAFORMAT_PCM;
	format_pcm.numChannels = 2;
	format_pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
	format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix;
    loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    loc_outmix.outputMix = outputMixObject;
    SLDataSink audioSnk;
    audioSnk.pLocator = &loc_outmix;
    audioSnk.pFormat = NULL;

    // create audio player
	LOG_DEBUG("Creating audio player");
    const SLInterfaceID bq_ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean bq_req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, bq_ids, bq_req);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*engineEngine)->CreateAudioPlayer() failed!");
		_destroy_audio();
		return;
	}
    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerObject)->Realize() failed!");
		_destroy_audio();
		return;
	}
    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerObject) failed to get play interface!");
		_destroy_audio();
		return;
	}
    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerObject) failed to get buffer queue interface!");
		_destroy_audio();
		return;
	}
    // register callback on the buffer queue
	LOG_DEBUG("Registering audio player callback");
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, _audio_player_callback, NULL);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerBufferQueue)->RegisterCallback() failed!");
		_destroy_audio();
		return;
	}
    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerObject) failed to get volume interface!");
		_destroy_audio();
		return;
	}
    // set the player's state to playing
	LOG_DEBUG("Setting audio playstate");
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerPlay)->SetPlayState() failed in init!");
		_destroy_audio();
		return;
	}

	LOG_DEBUG("_init_audio() complete");


	return;
}

void _destroy_audio()
{
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (bqPlayerObject != NULL) {
		LOG_DEBUG("Destroying bqPlayerObject");
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerVolume = NULL;
    }
    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != NULL) {
		LOG_DEBUG("Destroying outputMixObject");
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
        outputMixVolume = NULL;
    }
    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
		LOG_DEBUG("Destroying engineObject");
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
	return;
}

void _pause_unpause_audio()
{
	if (bqPlayerPlay == NULL)
		return;
	if (!_okay_to_play)
		return;

	//LOG_DEBUG("_pause_unpause_audio() called with _paused=%d", audio->_paused);
	SLresult result;
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, _audio_paused ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerPlay)->SetPlayState() failed in _pause_unpause_audio()!");
	}
	return;
}

void _stop_audio()
{
	if (bqPlayerPlay == NULL)
		return;

	SLresult result;
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    if (result != SL_RESULT_SUCCESS) {
		LOG_ERROR("(*bqPlayerPlay)->SetPlayState() failed in _stop_audio()!");
	}
	return;
}

bool _audio_enqueue()
{	
	static char buffer[AUDIO_BUFFER_SIZE];

	if (_ringbuffer == NULL)
		return false;
	if (!_okay_to_play)
		return false;
	if (_enqueue_count < AUDIO_MAX_BUFFERS && (_rb_available_data >= AUDIO_BUFFER_SIZE)) {

		if (_rb_read(buffer) == -1) {
			LOG_ERROR("Failed to read audio data from ringbuffer!");
		} else {
			if (bqPlayerObject == NULL)
				return false;
			SLresult result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer, AUDIO_BUFFER_SIZE);
			if (result != SL_RESULT_SUCCESS) {
				LOG_ERROR("Failed to enqueue audio data from audio callback!");
				return false;
			} else {
				_enqueue_count++;
				return true;
			}
		}
	} else {
		if (_rb_available_data < AUDIO_BUFFER_SIZE) {
			//~ LOG_ERROR("Not enough data to enqueue: %d", _rb_available_data);
			return false;
		}
		// return true if we've already enqueued the maximum buffer count
		return true;
	}
}

size_t _audio_batch_callback(const int16_t *data, size_t bytes)
{
	if (_ringbuffer == NULL)
		return 0;
		
	if (data == NULL || bytes == 0) {
		LOG_ERROR("Got null/empty audio data in audio callback!");
		return 0;
	}
	
	pthread_mutex_lock(&_audioMutex);

	while (_rb_available_space < bytes) {

		pthread_mutex_unlock(&_audioMutex);

		//~ LOG_DEBUG("Not enough space in ringbuffer, sleeping");
		//~ LOG_DEBUG("_rb_available_space(): %d    _rb_available_data(): %d", _rb_available_space(), _rb_available_data());

		sched_yield();
		usleep(5);

		pthread_mutex_lock(&_audioMutex);
	}

	int result = _rb_write(data, bytes);
	if (result == -1) {
		LOG_ERROR("Failed writing audio data to ringbuffer!");
		pthread_mutex_unlock(&_audioMutex);
		return 0;
	}

	//~ while (_rb_available_data >= (AUDIO_BUFFER_SIZE) && _enqueue_count < AUDIO_MAX_BUFFERS) {
		if (!_audio_enqueue()) {
			//LOG_ERROR("Not enough data to enqueue in batch callback!");
		}
	//~ }

	// check whether we have stopped playing
	if (bqPlayerPlay != NULL && _okay_to_play) {
		SLuint32 state;
		(*bqPlayerPlay)->GetPlayState(bqPlayerPlay, &state);
		if (state != SL_PLAYSTATE_PLAYING) {
			LOG_DEBUG("Player is not playing, resetting playstate.");
			(*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
		}
	}
	
	pthread_mutex_unlock(&_audioMutex);

	return (size_t)result;
}

// this callback handler is called every time a buffer finishes playing
void _audio_player_callback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	pthread_mutex_lock(&_audioMutex);

	// let audio callback know we need another buffer 
	if (_enqueue_count > 0) {
		_enqueue_count--;
	}
	//~ while (_enqueue_count < AUDIO_MAX_BUFFERS) {
		//~ _audio_enqueue();
	//~ }
	while (_okay_to_play && !_audio_enqueue()) {
		pthread_mutex_unlock(&_audioMutex);
		sched_yield();
		if (!_okay_to_play)
			return;
		pthread_mutex_lock(&_audioMutex);
	}
	
	pthread_mutex_unlock(&_audioMutex);
}

void _rb_create()
{
    _ringbuffer = malloc(sizeof(RingBuffer));
    _ringbuffer->length  = (AUDIO_BUFFER_SIZE * AUDIO_MAX_BUFFERS * AUDIO_BUFFER_MULT) + AUDIO_RB_EXTRA_BYTES;
    _ringbuffer->data = calloc(_ringbuffer->length, 1);
	_rb_reset();
    return;
}

void _rb_destroy()
{
    if (_ringbuffer != NULL) {
		LOG_DEBUG("Destroying ringbuffer");
        free(_ringbuffer->data);
        free(_ringbuffer);
        _ringbuffer = NULL;
    }
}

void _rb_reset()
{
    _ringbuffer->start = 0;
    _ringbuffer->end = 0;
    _ringbuffer->avail_da = 0;
    _ringbuffer->avail_sp = _ringbuffer->length - AUDIO_RB_EXTRA_BYTES;
    //~ _ringbuffer->avail_sp = _ringbuffer->length;
}

int _rb_write(const void *source, size_t bytes)
{
    //~ if (_rb_available_data == 0) {
        //~ _ringbuffer->start = _ringbuffer->end = 0;
    //~ }

	if (_rb_available_space < bytes) {
		return -1;
	}

	void *result;
	if (_ringbuffer->end + bytes > _ringbuffer->length) {
		int diff = (_ringbuffer->end + bytes) % _ringbuffer->length;
		result = memcpy(_rb_ends_at, source, bytes - diff);
		if (result == NULL) {
			return -1;
		}
		result = memcpy(_ringbuffer->data, source + (bytes - diff), diff);
		if (result == NULL) {
			return -1;
		}
	} else {
		result = memcpy(_rb_ends_at, source, bytes);
		if (result == NULL) {
			return -1;
		}
	}

    _rb_commit_write(bytes);
    _ringbuffer->avail_sp -= bytes;
    _ringbuffer->avail_da += bytes;

    return bytes;
}

int _rb_read(void *target)
{
	if (_rb_available_data < AUDIO_BUFFER_SIZE) {
		return -1;
	}

	void *result;
	if (_ringbuffer->start + AUDIO_BUFFER_SIZE > _ringbuffer->length) {
		int diff = (_ringbuffer->start + AUDIO_BUFFER_SIZE) % _ringbuffer->length;
		result = memcpy(target, _rb_starts_at, AUDIO_BUFFER_SIZE - diff);
		if (result == NULL) {
			return -1;
		}
		result = memcpy(target + (AUDIO_BUFFER_SIZE - diff), _ringbuffer->data, diff);
		if (result == NULL) {
			return -1;
		}
	} else {
		result = memcpy(target, _rb_starts_at, AUDIO_BUFFER_SIZE);
		if (result == NULL) {
			return -1;
		}
	}

    _rb_commit_read(AUDIO_BUFFER_SIZE);
    _ringbuffer->avail_sp += AUDIO_BUFFER_SIZE;
    _ringbuffer->avail_da -= AUDIO_BUFFER_SIZE;

    //~ if (_ringbuffer->end == _ringbuffer->start) {
        //~ _ringbuffer->start = _ringbuffer->end = 0;
    //~ }

    return AUDIO_BUFFER_SIZE;
}

