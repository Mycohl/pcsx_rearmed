#ifndef LOGGER_H
#define LOGGER_H

#include <strings.h>
#include <android/log.h>

#define DEBUGLOG 0

#define LOG_TAG "PCSX_ReARMed"

#if DEBUGLOG
#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ; /* __VA_ARGS__ */
#endif

#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


#endif // LOGGER_H
