#include <stdint.h>
#include <stdlib.h>
#include <jni.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer

#include "jniapi.h"
#include "logger.h"
#include "renderer.h"
#include "audio.h"
#include "settings.h"

static ANativeWindow *window = NULL;
static Renderer *renderer = NULL;
//~ static AudioOut *audio = 0;

static JavaVM *jvm = NULL;
static jobject activity = NULL; // GlobalRef
static jclass activityClass = NULL; // GlobalRef

static jmethodID getPrefID = 0;
static jmethodID setPMsgID = 0;
static jmethodID loadErrorID = 0;
static jmethodID getInpStateID = 0;
static jmethodID getAppStrID = 0;

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    LOG_DEBUG("Native library loaded!");
	
	jvm = vm;

	JNIEnv *jenv;
	(*jvm)->GetEnv(jvm, (void **) &jenv, JNI_VERSION_1_6);

	activityClass = (jclass)(*jenv)->NewGlobalRef(jenv, (*jenv)->FindClass(jenv, "net/dancingdemon/pcsx/PCSX_ReARMed"));
	
	//getPrefID = (*jenv)->GetMethodID(jenv, activityClass, "getPref", "(Ljava/lang/String;)Ljava/lang/String;" );
	getPrefID = (*jenv)->GetStaticMethodID(jenv, activityClass, "getPref", "(Ljava/lang/String;)Ljava/lang/String;" );
    if (getPrefID == NULL) {
		LOG_ERROR("Could not get MethodID for getPref()!");
	} else {
		LOG_DEBUG("Found MethodID for getPref()");
	}

	//setPMsgID = (*jenv)->GetMethodID(jenv, activityClass, "setPCSXMessage", "(Ljava/lang/String;)V" );
	setPMsgID = (*jenv)->GetStaticMethodID(jenv, activityClass, "setPCSXMessage", "(Ljava/lang/String;)V" );
    if (setPMsgID == NULL) {
		LOG_ERROR("Could not get MethodID for setPCSXMessage()!");
	} else {
		LOG_DEBUG("Found MethodID for setPCSXMessage()");
	}
    
	loadErrorID = (*jenv)->GetStaticMethodID(jenv, activityClass, "loadError", "(Ljava/lang/String;)V" );
    if (loadErrorID == NULL) {
		LOG_ERROR("Could not get MethodID for loadError()!");
	} else {
		LOG_DEBUG("Found MethodID for loadError()");
	}
    
	getInpStateID = (*jenv)->GetStaticMethodID(jenv, activityClass, "getInputState", "()[I" );
    if (getInpStateID == NULL) {
		LOG_ERROR("Could not get MethodID for getInputState()!");
	} else {
		LOG_DEBUG("Found MethodID for getInputState()");
	}
    
	getAppStrID = (*jenv)->GetStaticMethodID(jenv, activityClass, "getAppString", "(I)Ljava/lang/String;" );
    if (getAppStrID == NULL) {
		LOG_ERROR("Could not get MethodID for getAppString()!");
	} else {
		LOG_DEBUG("Found MethodID for getAppString()");
	}
    
	
	return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnStart(JNIEnv* jenv, jobject obj)
{
    LOG_DEBUG("nativeOnStart()");
    

    //LOG_DEBUG("checking class of 'obj'...");
    //_check_java_object_class_name(jenv, &obj);

    // get our JVM and activity refs
    //(*jenv)->GetJavaVM(jenv, &jvm);
    activity = (*jenv)->NewGlobalRef(jenv, obj);    

    //LOG_DEBUG("checking class of 'activity'...");
    //_check_java_object_class_name(jenv, &activity);

    // init renderer
    renderer = malloc(sizeof(Renderer));
    renderer->_window = NULL;
    renderer->_thread_running = false;

    // init audio
    //~ audio = malloc(sizeof(AudioOut));
    //~ Audio_Init(audio);

    Renderer_Init(renderer);
    Audio_Init();

    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnResume(JNIEnv* jenv, jobject obj)
{
	if (renderer == NULL)
		return;

    LOG_DEBUG("nativeOnResume()");

	// don't unpause here, since menu should be open
	//~ Renderer_UnPause(renderer);
	//~ Audio_UnPause();

    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnPause(JNIEnv* jenv, jobject obj)
{
	if (renderer == NULL)
		return;

    LOG_DEBUG("nativeOnPause()");

    Renderer_Pause(renderer);
    Audio_Pause();

    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeOnStop(JNIEnv* jenv, jobject obj)
{
	if (renderer == NULL)
		return;

    LOG_DEBUG("nativeOnStop()");

    Renderer_Exit(renderer);
    Audio_Exit();

	free(renderer);
    renderer = NULL;

	(*jenv)->DeleteGlobalRef(jenv, activityClass); 
	(*jenv)->DeleteGlobalRef(jenv, activity); 
	
	//~ exit(0);

    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeSetSurface(JNIEnv* jenv, jobject obj, jobject surface)
{
    if (surface != NULL) {
        window = ANativeWindow_fromSurface(jenv, surface);
        LOG_DEBUG("nativeSetSurface() - Got window %p", window);
		if (renderer != NULL)
			Renderer_SetWindow(renderer, window);
    } else {
        LOG_DEBUG("nativeSetSurface() - Releasing window");
		if (renderer != NULL)
			Renderer_SetWindow(renderer, NULL);
        if (window != NULL)
			ANativeWindow_release(window);
    }

    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeSetMessage(JNIEnv* jenv, jobject obj, jstring jmsg, jint frames)
{
	const char *msg = (*jenv)->GetStringUTFChars(jenv, jmsg, 0);

	Renderer_SetMessage(renderer, msg, frames);

	(*jenv)->ReleaseStringUTFChars(jenv, jmsg, msg);

	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativePrefsChanged(JNIEnv* jenv, jobject obj)
{
	Settings_Update();
	Renderer_UpdateShader(renderer);
    return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuPauseUnpause(JNIEnv* jenv, jobject obj, jboolean which)
{
	if (which) {
		Renderer_Pause(renderer);
		Audio_Pause();
	} else {
		Renderer_UnPause(renderer);
		Audio_UnPause();
	}

	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuLoadGame(JNIEnv* jenv, jobject obj, jstring jpath)
{
	const char *path = (*jenv)->GetStringUTFChars(jenv, jpath, 0);

	Renderer_LoadGame(renderer, path);
	Audio_LoadGame();

	(*jenv)->ReleaseStringUTFChars(jenv, jpath, path);

	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuUnloadGame(JNIEnv* jenv, jobject obj)
{
	Renderer_UnloadGame(renderer);
	Audio_UnloadGame();
	
	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuResetGame(JNIEnv* jenv, jobject obj)
{
	Renderer_ResetGame(renderer);
	Audio_UnPause();
	
	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuChangeDisc(JNIEnv* jenv, jobject obj, jstring jpath)
{
	const char *path = (*jenv)->GetStringUTFChars(jenv, jpath, 0);

	Renderer_ChangeDisc(renderer, path);

	(*jenv)->ReleaseStringUTFChars(jenv, jpath, path);

	return;
}

JNIEXPORT void JNICALL Java_net_dancingdemon_pcsx_PCSX_1ReARMed_nativeEmuSaveLoad(JNIEnv* jenv, jobject obj, jboolean which, jint slot)
{
	if (which)
		Renderer_SaveState(renderer, (int)slot);
	else
		Renderer_LoadState(renderer, (int)slot);

	Audio_UnPause();
	
	return;
}

void JNI_Get_Pref(const char* key, char* value, size_t bufSize)
{
	//~ LOG_DEBUG("JNI_Get_Pref() called with key %s", key);
    JNIEnv* jenv;
    (*jvm)->AttachCurrentThread(jvm, &jenv, NULL);

	jstring jvalue = (jstring)(*jenv)->CallStaticObjectMethod(jenv, activityClass, getPrefID, (*jenv)->NewStringUTF(jenv, key));

	if (jvalue == NULL)
		value = NULL;
	else {
		const char *conv = (*jenv)->GetStringUTFChars(jenv, jvalue, 0);
		strncpy(value, conv, bufSize);
		(*jenv)->ReleaseStringUTFChars(jenv, jvalue, conv);
	}
    (*jvm)->DetachCurrentThread(jvm);
	return;
}

void JNI_Set_Message(const char* msg)
{
	//~ LOG_DEBUG("JNI_Set_Message() called with msg '%s'", msg);
    JNIEnv* jenv;
    (*jvm)->AttachCurrentThread(jvm, &jenv, NULL);
    
    (*jenv)->CallStaticObjectMethod(jenv, activityClass, setPMsgID, (*jenv)->NewStringUTF(jenv, msg));

    (*jvm)->DetachCurrentThread(jvm);
	return;
}

void JNI_LoadError(const char* path)
{
    JNIEnv* jenv; 
    (*jvm)->AttachCurrentThread(jvm, &jenv, NULL);
    
    (*jenv)->CallStaticVoidMethod(jenv, activityClass, loadErrorID, (*jenv)->NewStringUTF(jenv, path));

    (*jvm)->DetachCurrentThread(jvm);
	return;
}

int*  JNI_Get_InputState()
{
	static int val[9];

    JNIEnv* jenv; 
    (*jvm)->AttachCurrentThread(jvm, &jenv, NULL);
    
    jintArray arr = (jintArray)(*jenv)->CallStaticObjectMethod(jenv, activityClass, getInpStateID);

	if (arr == NULL) {
		//LOG_ERROR("Input array is null!");
		return NULL;
	}
	
	//jsize len = (*jenv)->GetArrayLength(jenv, arr);
	jint *input = (*jenv)->GetIntArrayElements(jenv, arr, NULL);
	//Log("Accessing input: %d", input[0]);
	int i;
	for (i=0;i<9;i++) {
		val[i] = input[i];
	}

	(*jenv)->ReleaseIntArrayElements(jenv, arr, input, 0);

    (*jvm)->DetachCurrentThread(jvm);
	return val;
}

void JNI_Get_AppString(int which, char* value, size_t bufSize)
{
    JNIEnv* jenv;
    (*jvm)->AttachCurrentThread(jvm, &jenv, NULL);

	jstring jvalue = (jstring)(*jenv)->CallStaticObjectMethod(jenv, activityClass, getAppStrID, which);

	if (jvalue == NULL)
		value = NULL;
	else {
		const char *conv = (*jenv)->GetStringUTFChars(jenv, jvalue, 0);
		strncpy(value, conv, bufSize);
		(*jenv)->ReleaseStringUTFChars(jenv, jvalue, conv);
	}
    (*jvm)->DetachCurrentThread(jvm);
	return;
}

/*
void _check_java_object_class_name(JNIEnv *jenv, jobject *jobj)
{
	jclass cls = (*jenv)->GetObjectClass(jenv, *jobj);

	// First get the class object
	jmethodID mid = (*jenv)->GetMethodID(jenv, cls, "getClass", "()Ljava/lang/Class;");
	jobject clsObj = (*jenv)->CallObjectMethod(jenv, *jobj, mid);

	// Now get the class object's class descriptor
	cls = (*jenv)->GetObjectClass(jenv, clsObj);

	// Find the getName() method on the class object
	mid = (*jenv)->GetMethodID(jenv, cls, "getName", "()Ljava/lang/String;");

	// Call the getName() to get a jstring object back
	jstring strObj = (jstring)(*jenv)->CallObjectMethod(jenv, clsObj, mid);

	// Now get the c string from the java jstring object
	const char* str = (*jenv)->GetStringUTFChars(jenv, strObj, NULL);

	// Print the class name
	LOG_DEBUG("JObj class is: %s", str);

	// Release the memory pinned char array
	(*jenv)->ReleaseStringUTFChars(jenv, strObj, str);
}
*/
