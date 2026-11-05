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
#include "mod/xloop/auth_ui.h"
#include "MemoryPatch.h"
#include <netdb.h>
//Original src For free by @xvaluex : @FiturinEngine.

// HWID: suntikkan ANDROID_ID asli (lewat JNI) ke license_core tanpa membuat
// file itu bergantung pada java.h (java.h hanya boleh di-include sekali).
static std::string XloopAndroidId() {
    static std::string cached;
    if (!cached.empty()) return cached;
    std::string id = getAndroidID();
    if (!id.empty()) cached = id;
    return cached;
}

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

// Login lama (endpoint kosong, response "success" plain) sudah tidak dipakai.
// Semua autentikasi sekarang lewat XLOOP PANEL /api/validate yang
// ditandatangani Ed25519 -> lihat mod/xloop/license_core.h + auth_ui.h.
void Login(const std::string& hwid, const std::string& key) {
    (void)hwid;
    is_logging_in = true;
    ERROR_MESSAGE = "";
    ERROR_CODE = 0;

    std::thread([key]() {
        xloop::Result r = xloop::VerifyKey(key);
        if (r.ok) {
            g_Key = r.key;
            g_Reseller = r.reseller;
            g_ExpiryTime = r.expiredEpoch > 0 ? r.expiredEpoch : (int64_t)time(nullptr) + (3650LL * 24 * 3600);
            g_AuthToken = 0xDEADBEEFCAFEBABEULL ^ (uint64_t)g_ExpiryTime;
            g_ExpTime = formatTimestamp(g_ExpiryTime);
            g_isTrial = false;
            g_Token = "xloop-ed25519";
            g_Auth = "xloop-ed25519";
            persistent_string["key"] = r.key;
            save_persistence();
            g_menu.showLanguageSelector = true;
            g_menu.isOpen = false;
        } else {
            ERROR_MESSAGE = r.message;
            ERROR_CODE = 1;
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

    // Baca session lisensi lebih awal supaya menu tidak sempat tampil
    // sebelum status login diketahui.
    xloop::SetAndroidIdProvider(&XloopAndroidId);
    xloop::BootAuth();
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