LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_FEATURES += exceptions

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES := main.cpp gfx2d_collision.cpp gfx2d_fileio.cpp gfx2d_filter.cpp gfx2d_sdl.cpp misc.cpp network_linux.cpp simdjson.cpp translator.cpp vector2d.cpp wrapper.cpp android_fopen.cpp

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
