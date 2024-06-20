#include "cuefinger_jni.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_franqulator_cuefinger_Cuefinger_load(JNIEnv *env, jobject obj, jobject assetManager) {

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    if (mgr == NULL) {
        writeLog(LOG_ERROR, "error loading asset manager");
    } else {
        android_fopen_set_asset_manager(mgr);
    }
}


JNIEXPORT void JNICALL
Java_franqulator_cuefinger_Cuefinger_loadSettings(JNIEnv *env, jobject obj, jstring json) {
    const char *c_str = (const char *) env->GetStringUTFChars(json, NULL);
    if(c_str == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GetStringUTFChars failed");
        return;
    }

    string njson(c_str);
    env->ReleaseStringUTFChars(json, c_str);

    if(!njson.empty()) {
        g_settings.load(njson);
    }
    g_settings.fullscreen = true; // auf android immer im fullscreen starten
}

JNIEXPORT jstring JNICALL
Java_franqulator_cuefinger_Cuefinger_getSettingsJSON(JNIEnv *env, jobject obj) {
    return env->NewStringUTF(g_settings.toJSON().c_str());
}

JNIEXPORT void JNICALL
Java_franqulator_cuefinger_Cuefinger_loadServerSettings(JNIEnv *env, jobject obj, jstring json) {
    const char *c_str = (const char *) env->GetStringUTFChars(json, NULL);
    if (c_str == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GetStringUTFChars failed");
        return;
    }

    string njson(c_str);
    env->ReleaseStringUTFChars(json, c_str);

    if (!njson.empty()) {
        size_t posSvr = njson.find("svr=", 0);
        while (posSvr != string::npos) {
            posSvr += 4;

            size_t posSet = njson.find("set=", posSvr);
            if (posSet == string::npos) {
                break;
            }
            string server = njson.substr(posSvr, posSet - posSvr);

            posSet += 4;
            posSvr = njson.find("svr=", posSet);
            if (posSvr == string::npos) {
                posSvr = njson.length();
            }
            string settings = njson.substr(posSet, posSvr - posSet);

            serverSettingsJSON.insert_or_assign(server, settings);
        }
    }
}

JNIEXPORT jstring JNICALL
Java_franqulator_cuefinger_Cuefinger_getServerSettingsJSON(JNIEnv *env, jobject obj) {
    string njson;

    if (g_ua_server_connected.length() != 0) {
        saveServerSettings(g_ua_server_connected);
    }

    for (map<string, string>::iterator it = serverSettingsJSON.begin();
         it != serverSettingsJSON.end(); ++it) {
        njson += "svr=" + it->first + "set=" + it->second;
    }

    return env->NewStringUTF(njson.c_str());
}

JNIEXPORT void JNICALL Java_franqulator_cuefinger_Cuefinger_exit(JNIEnv *env, jobject obj) {
    g_running = false;
}

#ifdef __cplusplus
}
#endif