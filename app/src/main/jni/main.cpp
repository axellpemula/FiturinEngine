#include "include/includes.h"
#include "include/hook.h"
#include <android/log.h>
#include <stdint.h>
#include <jni.h>
#include "include/input.h"
#include "include/java.h"
#include "include/obfuscation.h"
#include "include/manual_dlsym.h"
#include "include/random_defs.h"
#include "include/Security.h"
#include "menu.h"
#include "MemoryPatch.h"
#include <netdb.h>
//Original src For free by @xvaluex : @FiturinEngine.

int64_t g_ExpiryTime = 0;
uint64_t g_AuthToken = 0;
std::string g_Key = "";
std::string g_Reseller = "";
std::string g_ExpTime = "";
bool g_isTrial = false;
bool is_logging_in = false;
int ERROR_CODE = 0;
std::string ERROR_MESSAGE = "";
std::string g_Token = "";
std::string g_Auth = "";

void Login(const std::string& hwid, const std::string& key) {
    is_logging_in = true;
    ERROR_MESSAGE = "";
    ERROR_CODE = 0;
    
    std::thread([hwid, key]() {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, "https://");
            
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Accept: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            std::string jsonData = "{\"key\": \"" + key + "\", \"hwid\": \"" + hwid + "\", \"game\": \"AimX8bp\"}";
            
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, EngineAuth::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

            res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                if (readBuffer.find("\"success\":true") != std::string::npos || readBuffer.find("\"success\": true") != std::string::npos) {
                    g_Key = key;
                    g_ExpiryTime = (int64_t)time(nullptr) + (7LL * 24LL * 3600LL);
                    g_AuthToken = (uint64_t)g_ExpiryTime ^ 0xDEADBEEFCAFEBABE;
                    g_ExpTime = formatTimestamp(g_ExpiryTime);
                    g_isTrial = false;
                    g_Token = "Only_declarations ";
                    g_Auth = "Only_declarations";
                    
                    persistent_string["key"] = key;
                    save_persistence();
                    
                    g_menu.showLanguageSelector = true;
                    g_menu.isOpen = false;
                } else {
                    std::string serverMsg = EngineAuth::ExtractJsonString(readBuffer, "message");
                    if (serverMsg.empty()) serverMsg = "Invalid or expired key";
                    ERROR_MESSAGE = serverMsg;
                    ERROR_CODE = 1;
                }
            } else {
                ERROR_MESSAGE = "Failed to connect to server";
                ERROR_CODE = 2;
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        is_logging_in = false;
    }).detach();
}

DEFINES(int32_t, setActiveVisualCue, ptr arg1) {
    sharedGameManager = arg1;
    return _setActiveVisualCue(arg1);
}

void __HOOKS__() {
    LOGI("__HOOKS__");

    HOOK(libmain + O(0x2d911e0), setActiveVisualCue);
    HOOK(libmain + O(0x3068c94), StartMatch);

    xhook_clear();
    xhook_register(O(".*/com.miniclip.eightballpool/.*"), O("eglSwapBuffers"), (void*)Draw, (void**)&_Draw);
    if (xhook_refresh(0)) LOGI("xhook_refresh failed");
}

void __1__() {
    LOGI("LIB LOADED SUCCESSFULLY");

    sleep(2);

    PACKAGE_NAME = string(getcmdline());
    LOGI("cmdline: %s", PACKAGE_NAME.c_str());
    
    __IMGUI__();

    sleep(10);
    libmain = get8BPbase();
    LOGI("libmain: %p", libmain);

    __HOOKS__();
    __INPUT__();

    setup_global_segv_handler();
    SetupSignalTraceHandler();
    
    LOGI("RETURNING NOW MY GODDY GOD");
}


#include "mod/kill.h"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad");
    VM = vm;

    CALL(0);
    
    pthread(__1__);

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL Java_android_service_SurfaceView_onSendConfig(JNIEnv* env, jobject thiz, jstring key, jstring value) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    const char* v = env->GetStringUTFChars(value, nullptr);
    
    bool isOn = (v[0] == '1');
    
    if (strcmp(k, "ESP::LINES") == 0) {
        persistent_bool[O("bESP_DrawPredictionLine")] = isOn;
    } else if (strcmp(k, "ESP::POCKETS") == 0) {
        persistent_bool[O("bESP_DrawPockets")] = isOn;
    } else if (strcmp(k, "ESP::STATES") == 0) {
        persistent_bool[O("bESP_DrawPocketsShotState")] = isOn;
    } else if (strcmp(k, "AUTO::PLAY") == 0) {
        persistent_bool[O("bAutoPlay")] = isOn;
    } else if (strcmp(k, "AUTO::QUEUE") == 0) {
        persistent_bool[O("bAutoQueue")] = isOn;
    }
    
    save_persistence();
    
    env->ReleaseStringUTFChars(key, k);
    env->ReleaseStringUTFChars(value, v);
}
//Original src For free by @xvaluex : @FiturinEngine.
extern "C" JNIEXPORT void JNICALL Java_android_service_SurfaceView_onCanvasDraw(JNIEnv* env, jobject thiz, jobject canvas, jint w, jint h, jfloat d) {
}

extern "C" JNIEXPORT jstring JNICALL Java_android_service_SurfaceView_getExpTime(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF(g_ExpTime.empty() ? "N/A" : g_ExpTime.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL Java_android_service_SurfaceView_MenuColor(JNIEnv* env, jobject thiz) {
    return (jboolean)((g_AuthToken != 0 && g_ExpiryTime > 0) ? JNI_TRUE : JNI_FALSE);
}