#ifndef AUDIO_H
#define AUDIO_H

#include <pthread.h>

#include <stdbool.h>

void Audio_Init();
void Audio_Exit();
void Audio_Pause();
void Audio_UnPause();
void Audio_LoadGame();
void Audio_UnloadGame();



#endif // AUDIO_H
