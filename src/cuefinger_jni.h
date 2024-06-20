#include <jni.h>
#include "main.h"

#ifndef CUEFINGER_FRANQULATOR_CUEFINGER_CUEFINGER_H
#define CUEFINGER_FRANQULATOR_CUEFINGER_CUEFINGER_H

extern bool g_running;
class Settings;
extern Settings g_settings;
extern map<string, string> serverSettingsJSON;
extern string g_ua_server_connected;
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_franqulator_cuefinger_Cuefinger_load(JNIEnv *env, jobject obj, jobject assetManager);
JNIEXPORT void JNICALL Java_franqulator_cuefinger_Cuefinger_loadSettings(JNIEnv *env, jobject obj, jstring json);
JNIEXPORT jstring JNICALL Java_franqulator_cuefinger_Cuefinger_getSettingsJSON(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_franqulator_cuefinger_Cuefinger_loadServerSettings(JNIEnv* env, jobject obj, jstring json);
JNIEXPORT jstring JNICALL Java_franqulator_cuefinger_Cuefinger_getServerSettingsJSON(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_franqulator_cuefinger_Cuefinger_exit(JNIEnv *env, jobject obj);

#ifdef __cplusplus
}
#endif
#endif //CUEFINGER_FRANQULATOR_CUEFINGER_CUEFINGER_H
