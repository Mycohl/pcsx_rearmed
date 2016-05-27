#ifndef JNIAPI_H
#define JNIAPI_H

#ifdef JNI_H_
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnStart(JNIEnv *, jclass);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnResume(JNIEnv* jenv, jobject obj);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnPause(JNIEnv* jenv, jobject obj);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnStop(JNIEnv* jenv, jobject obj);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeSetSurface(JNIEnv* jenv, jobject obj, jobject surface);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeSetMessage(JNIEnv* jenv, jobject obj, jstring jmsg, jint frames);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativePrefsChanged(JNIEnv* jenv, jobject obj);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuPauseUnpause(JNIEnv* jenv, jobject obj, jboolean which);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuLoadGame(JNIEnv* jenv, jobject obj, jstring jpath);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuResetGame(JNIEnv* jenv, jobject obj);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuChangeDisc(JNIEnv* jenv, jobject obj, jstring jpath);
JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuSaveLoad(JNIEnv* jenv, jobject obj, jboolean which, jint slot);
#endif // JNI_H_

void JNI_Get_Pref(const char* key, char* value, size_t bufSize);
void JNI_Set_Message(const char* msg);
void JNI_LoadError(const char* path);
int* JNI_Get_InputState();
void JNI_Get_AppString(int which, char* value, size_t bufSize);

#endif // JNIAPI_H
