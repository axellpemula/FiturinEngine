#pragma once
//Original src For free by @xvaluex : @FiturinEngine.
//
// Auth runtime + UI key login (tema: dark glass + aksen gold #FFB800,
// sama seperti menu utama Fiturin Engine).
//
// Alur:
//   - Boot  : baca session -> kalau masih valid, langsung masuk (silent),
//             kalau sudah lewat interval verifikasi, validasi ulang ke panel.
//   - Login : tombol ACTIVATE -> thread -> xloop::VerifyKey() -> session disimpan.
//   - Gate  : RequireLicense() dipanggil tersebar (sebelum ESP, sebelum
//             Auto Play, sebelum toggle fitur) sesuai anjuran panel.
//
// Catatan UI: ukuran font di device bisa jauh lebih besar dari asumsi
// (font di-push dengan skala Arial Black), jadi semua posisi & lebar dihitung
// dari hasil CalcTextSize(), bukan konstanta mati. Isi kartu juga memakai
// BeginChild supaya kalau layarnya pendek / teks kebesaran, isinya bisa
// di-scroll, tidak saling tumpang-tindih.

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "include/external/imgui/imgui.h"
#include "mod/xloop/license_core.h"

namespace xloop {

// ======================= state global auth =======================
struct AuthState {
    bool        sessionLoaded   = false;   // session file sudah dicoba dibaca
    bool        loggedIn        = false;
    bool        checking        = false;   // request sedang berjalan
    bool        hasResult       = false;   // ada hasil pengecekan terakhir
    float       cardAnim        = 0.0f;    // 0..1 animasi masuk/keluar kartu
    float       toastTimer      = 0.0f;
    float       toastColor[4]   = {1.0f, 1.0f, 1.0f, 1.0f};
    float       progress        = 0.0f;    // progress bar saat checking
    Reason      lastReason      = Reason::Unknown;
    std::string keyInput;                  // isi field (plaintext, di memori saja)
    std::string toast;
    std::string message;                   // pesan status di kartu
    std::string reseller;
    std::string expiredText;
    int64_t     expiredEpoch    = 0;
    int         maxDevice       = 0;
    int         currentDevices  = 0;
    bool        wantFocusInput  = false;
    bool        showKey         = false;   // tombol mata
};

static AuthState g_auth;
static std::mutex g_authMutex;             // melindungi hasil dari thread

// Durasi (menit) sebelum validasi ulang ke server.
// Panel membatasi 30 request/menit per IP & 15/menit per key,
// jadi jangan validasi tiap klik / tiap frame.
static const int REFRESH_MINUTES = 10;
// Jeda minimum antar percobaan refresh di background (detik).
static const int REFRESH_COOLDOWN_SECONDS = 45;
// Selama server tidak terjangkau, session cache masih dipakai sampai batas ini.
static const int OFFLINE_GRACE_MINUTES = 90;

// ======================= helper state =======================
inline void SetToast(const std::string& msg, bool success) {
    g_auth.toast = msg;
    g_auth.toastTimer = 3.2f;
    if (success) {
        g_auth.toastColor[0] = 0.65f; g_auth.toastColor[1] = 0.90f; g_auth.toastColor[2] = 0.70f; g_auth.toastColor[3] = 1.0f;
    } else {
        g_auth.toastColor[0] = 1.00f; g_auth.toastColor[1] = 0.48f; g_auth.toastColor[2] = 0.46f; g_auth.toastColor[3] = 1.0f;
    }
}

inline void FillCache(bool valid, int64_t expiredEpoch, int64_t verifiedAt, const std::string& key,
                      const std::string& secret = "") {
    CachedLicense& c = Cache();
    c.valid = valid;
    c.expiredEpoch = expiredEpoch;
    c.verifiedAt = verifiedAt;
    c.key = key;
    c.secret = secret;      // hanya di memori, tidak pernah ditulis ke disk
}

inline void ApplyResult(const Result& r, bool silent) {
    g_auth.hasResult = true;
    g_auth.lastReason = r.reason;
    g_auth.loggedIn = r.ok;
    g_auth.checking = false;

    if (r.ok) {
        g_auth.message = "License activated. Welcome back!";
        g_auth.expiredEpoch = r.expiredEpoch;
        g_auth.expiredText = r.expiredEpoch > 0 ? FormatEpoch(r.expiredEpoch) : std::string("Lifetime");
        g_auth.maxDevice = r.maxDevice;
        g_auth.currentDevices = r.currentDevices;
        g_auth.reseller = r.reseller;
        g_auth.keyInput = r.key;
        g_auth.toastTimer = 0.0f;                 // kartu menutup sendiri, tak perlu toast
        FillCache(true, r.expiredEpoch, (int64_t)time(nullptr), r.key, r.secret);
    } else {
        g_auth.message = r.message;
        FillCache(false, 0, 0, "");
        if (!silent) {
            g_auth.toast = r.message;
            g_auth.toastTimer = 4.0f;
            g_auth.toastColor[0] = 1.00f; g_auth.toastColor[1] = 0.48f; g_auth.toastColor[2] = 0.46f; g_auth.toastColor[3] = 1.0f;
        }
    }
}

inline void PersistSession(const Result& r) {
    json s;
    s["hwid"] = DeviceHash();
    s["key_protected"] = ProtectKey(r.key);
    s["key_hash"] = KeyHash(r.key);
    s["tool"] = std::string(XLOOP_TOOL_SLUG);
    s["version"] = std::string(XLOOP_TOOL_VERSION);
    s["expired_epoch"] = r.expiredEpoch;
    s["max_device"] = r.maxDevice;
    s["current_devices"] = r.currentDevices;
    s["reseller"] = r.reseller;
    s["verified_at"] = (int64_t)time(nullptr);
    SaveSession(s);
}

inline void RestoreFromSession(const json& s) {
    g_auth.loggedIn = true;
    g_auth.expiredEpoch = JsonInt(s, "expired_epoch");
    g_auth.expiredText = g_auth.expiredEpoch > 0 ? FormatEpoch(g_auth.expiredEpoch) : std::string("Lifetime");
    g_auth.maxDevice = (int)JsonInt(s, "max_device");
    g_auth.currentDevices = (int)JsonInt(s, "current_devices");
    g_auth.reseller = JsonStr(s, "reseller");
    g_auth.keyInput = UnprotectKey(JsonStr(s, "key_protected"));
    g_auth.hasResult = true;
    g_auth.lastReason = Reason::Ok;
    g_auth.message = "Session restored";
    g_auth.checking = false;
    FillCache(true, g_auth.expiredEpoch, JsonInt(s, "verified_at"), g_auth.keyInput);
}

static int64_t g_lastRefreshAttempt = 0;

// ======================= worker async =======================
inline void StartVerify(const std::string& rawKey, bool silent) {
    if (g_auth.checking) return;

    std::string key = NormalizeKey(rawKey);
    g_auth.keyInput = key;

    auto failFast = [](Reason reason) {
        g_auth.hasResult = true;
        g_auth.checking = false;
        g_auth.lastReason = reason;
        g_auth.message = ReasonMessage(reason);
        SetToast(g_auth.message, false);
        g_auth.wantFocusInput = true;
    };

    if (key.empty())            { failFast(Reason::EmptyKey); return; }
    if (!MatchesKeyRegex(key))  { failFast(Reason::BadKeyFormat); return; }

    if (Trim(XLOOP_PUBLIC_KEY_B64).empty()) {
        g_auth.hasResult = true;
        g_auth.checking = false;
        g_auth.lastReason = Reason::PublicKeyMissing;
        g_auth.message = ReasonMessage(Reason::PublicKeyMissing);
        SetToast(g_auth.message, false);
        return;
    }

    g_auth.checking = true;
    g_auth.progress = 0.0f;
    g_lastRefreshAttempt = (int64_t)time(nullptr);
    if (!silent) g_auth.message = "Contacting license server...";

    std::thread([key, silent]() {
        Result r = VerifyKey(key);

        std::lock_guard<std::mutex> lock(g_authMutex);

        if (r.ok) {
            PersistSession(r);
            if (!LockDeviceOnce()) {
                ClearSession();
                Result denied;
                denied.reason = Reason::HwidMismatch;
                denied.message = "This license is already activated on another device";
                ApplyResult(denied, silent);
                return;
            }
            ApplyResult(r, silent);
            return;
        }

        if (r.reason == Reason::Network) {
            // Server tidak terjangkau: masih boleh lanjut kalau cache belum lewat grace.
            json s = LoadSession();
            bool needNet = false;
            if (SessionStillValid(s, 0, needNet)) {
                int64_t age = (int64_t)time(nullptr) - JsonInt(s, "verified_at");
                if (age < (int64_t)OFFLINE_GRACE_MINUTES * 60) {
                    RestoreFromSession(s);
                    g_auth.message = "Offline mode: last verified " + FormatEpoch(JsonInt(s, "verified_at"));
                    return;
                }
            }
            ApplyResult(r, silent);
            return;
        }

        // Key ditolak server -> session lama tidak boleh dipakai lagi.
        ClearSession();
        ApplyResult(r, silent);
    }).detach();
}

// Dipanggil sekali saat boot.
inline void BootAuth() {
    if (g_auth.sessionLoaded) return;
    g_auth.sessionLoaded = true;

    json s = LoadSession();
    bool needNet = false;
    if (SessionStillValid(s, REFRESH_MINUTES, needNet)) {
        RestoreFromSession(s);
        if (needNet) StartVerify(g_auth.keyInput, true);
    } else if (!s.empty()) {
        // Session kedaluwarsa / beda device: key tetap dibawa ke field biar
        // user tidak perlu ketik ulang, tapi menu masih terkunci.
        std::string key = UnprotectKey(JsonStr(s, "key_protected"));
        ClearSession();
        FillCache(false, 0, 0, "");
        g_auth.keyInput = key;
        g_auth.message = "Session expired. Please activate again";
        g_auth.hasResult = !key.empty();
    }
}

// ======================= gate fitur =======================
// Sengaja dipanggil di beberapa titik (bukan satu fungsi isValid() tunggal),
// sesuai catatan anti-bypass di docs panel. Cepat: memakai cache memori,
// bukan baca file setiap frame.
inline bool RequireLicense(const char* where = "") {
    if (!g_auth.sessionLoaded) BootAuth();

    CachedLicense& c = Cache();
    if (!g_auth.loggedIn || !c.valid) {
        if (where && *where) Log("license gate blocked: %s", where);
        return false;
    }

    int64_t now = (int64_t)time(nullptr);

    if (c.expiredEpoch > 0 && c.expiredEpoch <= now) {
        g_auth.loggedIn = false;
        g_auth.hasResult = true;
        g_auth.lastReason = Reason::KeyExpired;
        g_auth.message = ReasonMessage(Reason::KeyExpired);
        FillCache(false, 0, 0, "");
        ClearSession();
        Log("license expired at runtime");
        return false;
    }

    // Refresh berkala di background (dibatasi cooldown supaya tidak kena rate limit).
    if (now - c.verifiedAt > (int64_t)REFRESH_MINUTES * 60 &&
        now - g_lastRefreshAttempt > REFRESH_COOLDOWN_SECONDS &&
        !g_auth.checking) {
        std::string key = c.key;
        if (key.empty()) key = UnprotectKey(JsonStr(LoadSession(), "key_protected"));
        if (!key.empty()) StartVerify(key, true);
    }
    return true;
}

// Nilai ini hanya hidup di RAM selama proses berjalan (boleh dipakai bagian
// tool yang butuh config/kunci dari panel, jangan ditulis ke storage).
inline std::string SecretPayload() { return Cache().secret; }

inline bool IsLoggedIn() { return g_auth.loggedIn; }
inline std::string CurrentKey() { return g_auth.keyInput; }
inline std::string ExpiryText() { return g_auth.expiredText.empty() ? "N/A" : g_auth.expiredText; }
inline std::string ResellerText() { return g_auth.reseller; }
inline int64_t ExpiryEpoch() { return g_auth.expiredEpoch; }

// ======================= UI constants =======================
static const ImU32 XC_GOLD      = IM_COL32(255, 184, 0, 255);
static const ImU32 XC_GOLD_SOFT = IM_COL32(255, 213, 79, 255);
static const ImU32 XC_TEXT      = IM_COL32(240, 236, 250, 255);
static const ImU32 XC_MUTED     = IM_COL32(158, 154, 176, 255);
static const ImU32 XC_DIM       = IM_COL32(120, 116, 140, 255);
static const ImU32 XC_STROKE    = IM_COL32(255, 245, 200, 34);
static const ImU32 XC_FIELD     = IM_COL32(20, 18, 28, 255);
static const ImU32 XC_GREEN     = IM_COL32(111, 174, 130, 255);
static const ImU32 XC_RED       = IM_COL32(201, 100, 92, 255);

inline ImU32 Alpha(ImU32 col, float a) {
    ImU32 alpha = (col >> IM_COL32_A_SHIFT) & 0xFF;
    ImU32 na = (ImU32)ImClamp((float)alpha * a, 0.0f, 255.0f);
    return (col & ~IM_COL32_A_MASK) | (na << IM_COL32_A_SHIFT);
}

inline ImU32 F4(float r, float g, float b, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

inline float XRound(float v) { return (float)((int)(v + (v >= 0.0f ? 0.5f : -0.5f))); }

inline float EaseOutCubic(float x) {
    x = ImClamp(x, 0.0f, 1.0f);
    return 1.0f - powf(1.0f - x, 3.0f);
}

inline float& AnimSlot(ImGuiID id) {
    static std::map<ImGuiID, float>* s_anim = new std::map<ImGuiID, float>();
    return (*s_anim)[id];
}

inline void LerpAnim(ImGuiID id, float target, float speed = 12.0f) {
    float& v = AnimSlot(id);
    v = ImLerp(v, target, ImClamp(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f));
}

// Teks yang otomatis dipotong dengan "..." supaya tidak menabrak elemen lain.
// Dipakai di baris yang lebarnya terbatas (device id, pesan status, dsb).
inline std::string FitText(const std::string& text, float maxWidth) {
    if (text.empty() || maxWidth <= 0.0f) return text;
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;
    const char* ell = "...";
    float ellW = ImGui::CalcTextSize(ell).x;
    std::string cut = text;
    while (!cut.empty() && ImGui::CalcTextSize(cut.c_str()).x + ellW > maxWidth)
        cut.pop_back();
    return cut + ell;
}

inline void CenterText(ImDrawList* dl, ImVec2 centerX, const char* text, ImU32 col) {
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(centerX.x - ts.x * 0.5f, centerX.y), col, text);
}

// Tombol custom bertema: accent = gold penuh, non-accent = glass gelap.
inline bool ThemeButton(const char* id, const char* label, ImVec2 size, bool accent, bool disabled = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID wid = window->GetID(id);
    ImVec2 p = window->DC.CursorPos;
    ImRect bb(p, p + size);
    ImGui::ItemSize(size, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false, held = false;
    if (!disabled) ImGui::ButtonBehavior(bb, wid, &hovered, &held);
    LerpAnim(wid, (!disabled && (hovered || held)) ? 1.0f : 0.0f);
    float anim = AnimSlot(wid);

    ImDrawList* dl = window->DrawList;
    float rounding = size.y * 0.30f;

    if (accent) {
        for (int i = 6; i >= 1; --i) {
            int a = (int)(22 * (1.0f - (float)i / 6.0f) * (0.55f + 0.45f * anim));
            if (a > 0)
                dl->AddRectFilled(ImVec2(bb.Min.x - i, bb.Min.y - i), ImVec2(bb.Max.x + i, bb.Max.y + i),
                                  IM_COL32(255, 184, 0, a), rounding + i);
        }
        ImU32 c1 = IM_COL32(255, 213, 79, 255);
        ImU32 c2 = IM_COL32(255, 184, 0, 255);
        if (held)     { c1 = IM_COL32(198, 139, 0, 255); c2 = IM_COL32(160, 110, 0, 255); }
        if (disabled) { c1 = IM_COL32(78, 68, 40, 255);  c2 = IM_COL32(62, 54, 32, 255); }
        dl->PushClipRect(ImVec2(bb.Min.x, bb.Min.y - 2.0f), bb.Max, true);
        dl->AddRectFilledMultiColor(bb.Min, bb.Max, c1, c2, c2, c1);
        dl->PopClipRect();
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, (int)(22 * anim)), rounding);
        dl->AddRect(bb.Min, bb.Max, IM_COL32(255, 245, 200, (int)(70 + 110 * anim)), rounding, 0, 1.2f);
    } else {
        dl->AddRectFilled(bb.Min, bb.Max, disabled ? IM_COL32(22, 20, 28, 255) : IM_COL32(26, 24, 36, 255), rounding);
        if (anim > 0.01f)
            dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 184, 0, (int)(18 * anim)), rounding);
        dl->AddRect(bb.Min, bb.Max, Alpha(XC_STROKE, 1.0f + 1.6f * anim), rounding, 0, 1.1f);
    }

    ImU32 textCol = accent ? IM_COL32(30, 19, 0, 255) : Alpha(XC_TEXT, disabled ? 0.45f : 1.0f);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(XRound(bb.Min.x + (size.x - ts.x) * 0.5f), XRound(bb.Min.y + (size.y - ts.y) * 0.5f)),
                textCol, label);

    return hovered && !disabled && g.IO.MouseReleased[0];
}

// icon: 0 = mata, 1 = paste
inline bool CircleIconButton(const char* id, ImVec2 topLeft, float size, int icon, bool accent, bool disabled = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    ImGuiID wid = window->GetID(id);
    ImRect bb(topLeft, topLeft + ImVec2(size, size));
    ImGui::ItemSize(bb.GetSize(), 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false, held = false;
    if (!disabled) ImGui::ButtonBehavior(bb, wid, &hovered, &held);
    LerpAnim(wid, (!disabled && (hovered || held)) ? 1.0f : 0.0f);
    float anim = AnimSlot(wid);

    ImDrawList* dl = window->DrawList;
    float radius = size * 0.5f;
    ImVec2 center = bb.Min + ImVec2(radius, radius);
    float rounding = size * 0.30f;

    if (accent) {
        for (int i = 6; i >= 1; --i) {
            int a = (int)(18 * (1.0f - (float)i / 6.0f) * (0.6f + 0.4f * anim));
            if (a > 0) dl->AddRectFilled(ImVec2(bb.Min.x - i, bb.Min.y - i), ImVec2(bb.Max.x + i, bb.Max.y + i),
                                         IM_COL32(255, 184, 0, a), rounding + i);
        }
        dl->AddRectFilled(bb.Min, bb.Max, held ? IM_COL32(198, 139, 0, 255) : IM_COL32(255, 184, 0, 255), rounding);
        dl->AddRect(bb.Min, bb.Max, IM_COL32(255, 245, 200, 200), rounding, 0, 1.2f);
    } else {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(26, 24, 36, 255), rounding);
        dl->AddRect(bb.Min, bb.Max, Alpha(XC_STROKE, 1.0f + anim), rounding, 0, 1.2f);
    }

    ImU32 col = accent ? IM_COL32(30, 19, 0, 255) : Alpha(XC_TEXT, disabled ? 0.4f : 0.80f + 0.20f * anim);
    float r = radius * 0.44f;
    if (icon == 0) {
        dl->PathArcTo(center + ImVec2(0, r * 0.40f), r * 1.05f, IM_PI * 1.12f, IM_PI * 1.88f, 24);
        dl->PathStroke(col, 0, 1.6f);
        dl->PathArcTo(center - ImVec2(0, r * 0.40f), r * 1.05f, IM_PI * 0.12f, IM_PI * 0.88f, 24);
        dl->PathStroke(col, 0, 1.6f);
        dl->AddCircleFilled(center, r * 0.32f, col, 20);
    } else {
        dl->AddRect(ImVec2(center.x - r * 0.68f, center.y - r * 0.68f), ImVec2(center.x + r * 0.40f, center.y + r * 0.72f), col, 2.0f, 0, 1.5f);
        dl->AddRect(ImVec2(center.x - r * 0.34f, center.y - r * 0.92f), ImVec2(center.x + r * 0.78f, center.y + r * 0.48f), col, 2.0f, 0, 1.5f);
    }

    return hovered && !disabled && g.IO.MouseReleased[0];
}

inline void OpenUrl(const char* url) {
    std::string cmd = std::string("am start -a android.intent.action.VIEW -d ") + url + " > /dev/null 2>&1";
    system(cmd.c_str());
}

// Baca clipboard Android lewat helper JNI milik engine (java.h).
// Backend ImGui Android tidak menyediakan clipboard, jadi io.GetClipboardText()
// selalu kosong -> tombol PASTE tidak akan pernah bekerja tanpanya.
typedef std::string (*ClipboardProvider)();
inline ClipboardProvider& ClipboardHook() {
    static ClipboardProvider fn = nullptr;
    return fn;
}
inline void SetClipboardProvider(ClipboardProvider fn) { ClipboardHook() = fn; }

inline std::string ReadClipboard() {
    if (ClipboardHook()) {
        std::string s = ClipboardHook()();
        if (!s.empty()) return s;
    }
    const char* imguiClip = ImGui::GetClipboardText();   // fallback (desktop/harness)
    return imguiClip ? std::string(imguiClip) : std::string();
}

// ======================= kartu login =======================

// Tinggi kartu dihitung dari isi sebenarnya (semua ukuran berbasis tinggi
// baris teks + padding proporsional), bukan dari 1080p. Dengan begitu kartu
// tetap rapi walau font device besar atau layarnya 16:9 pendek (mis. 1565x810,
// 1600x900, 1280x720).
inline float LoginCardHeight(float padY, float gapS, float gapM, float headH,
                             float lineH, float btnH, float fieldH, int footerLines) {
    float h = padY;                                   // padding atas isi
    h += lineH + gapS * 0.5f;                         // judul
    h += lineH + gapM;                                // hint
    h += lineH + gapS;                                // label + format
    h += fieldH + gapM;                               // field
    h += (lineH + gapM * 0.7f) + gapM * 0.7f;         // kotak status
    h += btnH + gapM;                                 // tombol utama
    h += lineH + gapS * 0.3f;                         // baris DEVICE
    h += (float)footerLines * lineH + gapS;           // footer
    h += padY;                                        // padding bawah
    return XRound(h + headH);
}

inline void DrawLoginUI(ImGuiIO& io) {
    float dt = io.DeltaTime;
    if (!g_auth.sessionLoaded) BootAuth();
    if (g_auth.toastTimer > 0.0f) g_auth.toastTimer -= dt;

    if (g_auth.loggedIn) g_auth.cardAnim = ImMax(0.0f, g_auth.cardAnim - dt * 5.0f);
    else                g_auth.cardAnim = ImMin(1.0f, g_auth.cardAnim + dt * 5.0f);

    if (g_auth.checking) g_auth.progress = ImMin(1.0f, g_auth.progress + dt * 0.5f);

    const float alpha = g_auth.cardAnim;
    const float scale = ImClamp(io.DisplaySize.y / 900.0f, 0.55f, 1.0f);   // skala dekorasi

    if (alpha > 0.01f) {
        const float lineH   = ImGui::GetTextLineHeight();
        const float padX    = XRound(lineH * 0.80f);
        float padY    = XRound(lineH * 0.55f);
        float gapS    = XRound(lineH * 0.26f);
        float gapM    = XRound(lineH * 0.52f);
        float btnH    = XRound(lineH * 1.70f);
        float fieldH  = XRound(lineH * 1.75f);

        const bool active = g_auth.loggedIn;
        const ImU32 accent = active ? XC_GREEN : XC_GOLD;

        const char* titleIn = active ? "LICENSE ACTIVATED" : "ENTER LICENSE KEY";
        const char* hintIn  = active ? "Key tersimpan di device ini."
                                     : "Tempel key, lalu tekan ACTIVATE.";
        const char* primary = g_auth.checking ? "VERIFYING..."
                            : active ? "CONTINUE TO MENU" : "ACTIVATE LICENSE";

        // ---- lebar kartu: cukup untuk isi terpanjang, tapi tidak melebihi layar ----
        float wTitle = ImGui::CalcTextSize(titleIn).x;
        float wHint  = ImGui::CalcTextSize(hintIn).x;
        float wLabel = ImGui::CalcTextSize("LICENSE KEY").x;
        float wFmt   = ImGui::CalcTextSize("Format: XLVP-XXXX-XXXX").x;
        float wPaste = ImGui::CalcTextSize("PASTE").x + padX * 1.3f;
        float wPrim  = ImGui::CalcTextSize(primary).x + padX * 1.6f;
        float wFoot  = ImGui::CalcTextSize("@FiturinEngine  -  Belum punya key? Chat owner").x;
        float wFieldMin = ImGui::CalcTextSize("XLVP-XXXX-XXXX").x * 1.55f + fieldH + wPaste + gapS * 2.0f;

        float minCardW = ImMax(ImMax(wTitle, wHint), ImMax(wPrim, wFoot)) + padX * 2.0f;
        minCardW = ImMax(minCardW, (wLabel + wFmt + gapM) + padX * 2.0f);
        minCardW = ImMax(minCardW, wFieldMin + padX * 2.0f);
        const float maxW = ImMax(300.0f, io.DisplaySize.x * 0.96f);
        const float cardW = XRound(ImClamp(minCardW, 340.0f, maxW));
        const float innerW = cardW - padX * 2.0f;

        float headH = XRound(lineH * 1.60f + padY * 0.9f);
        const float maxH = io.DisplaySize.y * 0.94f;
        float fullH = LoginCardHeight(padY, gapS, gapM, headH, lineH, btnH, fieldH, 1);
        // Layar sangat pendek: rapatkan jarak (bukan perkecil font) supaya
        // isi kartu tetap utuh dan tombol utama tetap kelihatan.
        float padY2 = padY, gapS2 = gapS, gapM2 = gapM, btnH2 = btnH, fieldH2 = fieldH;
        if (fullH > maxH) {
            float k = ImMax(0.50f, maxH / fullH);
            padY2 = padY * k; gapS2 = gapS * k; gapM2 = gapM * k;
            btnH2  = ImMax(lineH * 1.35f, btnH * k);
            fieldH2 = ImMax(lineH * 1.35f, fieldH * k);
            headH = XRound(lineH * 1.50f + padY2 * 0.9f);
            fullH = LoginCardHeight(padY2, gapS2, gapM2, headH, lineH, btnH2, fieldH2, 1);
        }
        const float cardH = ImMin(fullH, maxH);

        float anim = EaseOutCubic(g_auth.cardAnim);
        float slide = (1.0f - anim) * 22.0f * scale;

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f + slide),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gapS, gapS));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNav;

        const int vtxStart = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        if (ImGui::Begin("##XloopLoginCard", nullptr, flags)) {
            padY = padY2; gapS = gapS2; gapM = gapM2; btnH = btnH2; fieldH = fieldH2;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            const float t = (float)ImGui::GetTime();
            const float R = 18.0f * scale;

            // ---------- latar kartu ----------
            dl->AddRectFilled(ImVec2(wp.x - 2, wp.y + 9), ImVec2(wp.x + cardW + 2, wp.y + cardH + 11), IM_COL32(0, 0, 0, 130), R + 3);
            dl->AddRectFilled(wp, ImVec2(wp.x + cardW, wp.y + cardH), IM_COL32(15, 14, 20, 253), R);
            dl->AddRect(wp, ImVec2(wp.x + cardW, wp.y + cardH), Alpha(accent, 0.32f), R, 0, 1.3f);

            // garis aksen bergerak di tepi atas
            {
                float sh = fmodf(t * 0.22f, 1.0f);
                float x0 = wp.x + cardW * (sh * 1.4f - 0.35f);
                dl->PushClipRect(wp, ImVec2(wp.x + cardW, wp.y + 3.0f * scale), true);
                dl->AddRectFilledMultiColor(ImVec2(x0 - 120.0f * scale, wp.y), ImVec2(x0 + 120.0f * scale, wp.y + 3.0f * scale),
                                            IM_COL32(255, 184, 0, 0), Alpha(accent, 1.0f),
                                            Alpha(accent, 1.0f), IM_COL32(255, 184, 0, 0));
                dl->PopClipRect();
            }

            // ---------- header: logo + nama + badge ----------
            {
                float logoR = XRound(lineH * 0.58f);
                ImVec2 lc(wp.x + padX + logoR, wp.y + headH * 0.5f);
                float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
                dl->AddCircleFilled(lc, logoR * 1.22f, IM_COL32(255, 184, 0, (int)(14 + 16 * pulse)), 40);
                dl->AddCircleFilled(lc, logoR, IM_COL32(28, 21, 10, 255), 32);
                dl->AddCircle(lc, logoR, Alpha(accent, 0.90f), 32, 1.8f);
                float k = logoR * 0.42f;
                dl->AddLine(lc - ImVec2(k, k), lc + ImVec2(k, k), IM_COL32(255, 213, 79, 255), 2.2f);
                dl->AddLine(lc + ImVec2(k, -k), lc + ImVec2(-k, k), IM_COL32(255, 213, 79, 255), 2.2f);

                const char* badge = active ? "ACTIVE" : "LOCKED";
                float bw = ImGui::CalcTextSize(badge).x + padX * 1.25f;
                float bh = lineH + 5.0f * scale;
                float bx = wp.x + cardW - padX - bw;
                float by = wp.y + headH * 0.5f - bh * 0.5f;

                float nameX = lc.x + logoR + 10.0f * scale;
                float nameMax = bx - 8.0f * scale - nameX;
                std::string name = FitText("FITURIN ENGINE", nameMax);
                dl->AddText(ImVec2(nameX, wp.y + headH * 0.5f - lineH * 0.5f), IM_COL32(255, 245, 200, 255), name.c_str());

                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), Alpha(accent, 0.15f), bh * 0.5f);
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), Alpha(accent, 0.62f), bh * 0.5f, 0, 1.1f);
                dl->AddCircleFilled(ImVec2(bx + 8.0f * scale + 2.0f, by + bh * 0.5f), 2.8f * scale, Alpha(accent, 0.95f), 12);
                dl->AddText(ImVec2(bx + 15.0f * scale, by + (bh - lineH) * 0.5f), Alpha(accent, 0.95f), badge);

                dl->AddLine(ImVec2(wp.x + padX, wp.y + headH), ImVec2(wp.x + cardW - padX, wp.y + headH),
                            IM_COL32(255, 245, 200, 26), 1.0f);
            }

            // ---------- isi kartu ----------
            float bodyH = cardH - headH;
            ImGui::SetCursorScreenPos(ImVec2(wp.x + padX, wp.y + headH));
            ImGui::BeginChild("##xloopBody", ImVec2(innerW + padX * 2.0f - padX, bodyH), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                const ImVec2 org = ImGui::GetCursorScreenPos();   // draw list = koordinat layar
                const float x = org.x;
                float y = org.y;

                cdl->AddText(ImVec2(x, y), XC_TEXT, titleIn);
                y += lineH + gapS * 0.5f;
                cdl->AddText(ImVec2(x, y), XC_DIM, FitText(hintIn, innerW).c_str());
                y += lineH + gapM;

                // label + keterangan format
                cdl->AddText(ImVec2(x, y), XC_DIM, "LICENSE KEY");
                {
                    std::string right = (active && g_auth.keyInput.size() >= 14)
                        ? ("XLVP-****-" + g_auth.keyInput.substr(g_auth.keyInput.size() - 4))
                        : std::string("Format: XLVP-XXXX-XXXX");
                    float rw = ImGui::CalcTextSize(right.c_str()).x;
                    cdl->AddText(ImVec2(x + innerW - rw, y), active ? Alpha(XC_GREEN, 0.85f) : Alpha(XC_DIM, 0.75f), right.c_str());
                }
                y += lineH + gapS;

                // ---------- baris input: field | mata | PASTE ----------
                const float pasteW = ImGui::CalcTextSize("PASTE").x + padX * 1.3f;
                float inputW = innerW - fieldH - pasteW - gapS * 2.0f;
                if (inputW < ImGui::CalcTextSize("XLVP-XXXX-XXXX").x + padX)
                    inputW = ImGui::CalcTextSize("XLVP-XXXX-XXXX").x + padX;

                ImVec2 fMin(x, y), fMax(x + inputW, y + fieldH);
                cdl->AddRectFilled(fMin, fMax, XC_FIELD, 10.0f * scale);

                ImGui::SetCursorScreenPos(ImVec2(fMin.x + padX * 0.55f, fMin.y + (fieldH - lineH) * 0.5f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.95f, 0.86f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.45f, 0.44f, 0.52f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(inputW - padX * 1.1f);

                char buf[128];
                snprintf(buf, sizeof(buf), "%s", g_auth.keyInput.c_str());
                ImGuiInputTextFlags iflags = ImGuiInputTextFlags_CharsUppercase |
                                             ImGuiInputTextFlags_AutoSelectAll |
                                             ImGuiInputTextFlags_EnterReturnsTrue;
                if (!g_auth.showKey) iflags |= ImGuiInputTextFlags_Password;
                if (g_auth.wantFocusInput) { ImGui::SetKeyboardFocusHere(); g_auth.wantFocusInput = false; }

                bool submitted = ImGui::InputTextWithHint("##xloopKeyField", "XLVP-XXXX-XXXX", buf, sizeof(buf), iflags);
                bool fieldActive = ImGui::IsItemActive();
                bool fieldHovered = ImGui::IsItemHovered();
                g_auth.keyInput = buf;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    g_auth.keyInput = NormalizeKey(g_auth.keyInput);
                    if (g_auth.keyInput.size() > sizeof(buf) - 1) g_auth.keyInput.resize(sizeof(buf) - 1);
                }

                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(5);

                ImU32 border = fieldActive  ? Alpha(accent, 0.95f)
                             : fieldHovered ? Alpha(accent, 0.62f)
                             : Alpha(XC_STROKE, 1.0f);
                cdl->AddRect(fMin, fMax, border, 10.0f * scale, 0, fieldActive ? 1.6f : 1.2f);

                // tombol mata (show/hide key)
                ImGui::SetCursorScreenPos(ImVec2(fMax.x + gapS, fMin.y));
                if (CircleIconButton("##xloopEye", ImVec2(fMax.x + gapS, fMin.y), fieldH, 0, false))
                    g_auth.showKey = !g_auth.showKey;

                // tombol PASTE -> clipboard Android (JNI), bukan clipboard ImGui
                ImGui::SetCursorScreenPos(ImVec2(fMax.x + gapS * 2.0f + fieldH, fMin.y));
                if (ThemeButton("##xloopPaste", "PASTE", ImVec2(pasteW, fieldH), false, g_auth.checking)) {
                    std::string clip = ReadClipboard();
                    std::string pasted = NormalizeKey(clip);
                    if (pasted.empty()) {
                        SetToast("Clipboard kosong. Copy key dulu", false);
                    } else if (MatchesKeyRegex(pasted)) {
                        g_auth.keyInput = pasted;
                        SetToast("Key ditempel dari clipboard", true);
                    } else {
                        g_auth.keyInput = pasted;
                        SetToast("Clipboard bukan key XLVP-XXXX-XXXX", false);
                        g_auth.wantFocusInput = true;
                    }
                }
                y = fMax.y + gapM;

                // ---------- kotak status ----------
                float statusH = XRound(lineH + gapM * 0.85f);
                ImVec2 stMin(x, y), stMax(x + innerW, y + statusH);
                const float iconCx = stMin.x + XRound(lineH * 0.62f);
                const float iconCy = (stMin.y + stMax.y) * 0.5f;
                const float textX  = iconCx + lineH * 0.62f;
                const float textMaxW = stMax.x - textX - padX * 0.5f;

                if (g_auth.checking) {
                    cdl->AddRectFilled(stMin, stMax, IM_COL32(255, 184, 0, 18), statusH * 0.32f);
                    cdl->AddRect(stMin, stMax, IM_COL32(255, 184, 0, 70), statusH * 0.32f, 0, 1.0f);
                    float sr = lineH * 0.26f;
                    cdl->AddCircle(ImVec2(iconCx, iconCy), sr, IM_COL32(255, 184, 0, 60), 24, 2.0f);
                    cdl->PathArcTo(ImVec2(iconCx, iconCy), sr, t * 4.5f, t * 4.5f + 4.2f, 20);
                    cdl->PathStroke(IM_COL32(255, 213, 79, 255), 0, 2.0f);
                    cdl->AddText(ImVec2(textX, iconCy - lineH * 0.5f), IM_COL32(255, 213, 79, 255),
                                 FitText(g_auth.message.empty() ? "Verifying..." : g_auth.message, textMaxW).c_str());
                    float pw = (stMax.x - stMin.x) * ImClamp(g_auth.progress, 0.0f, 1.0f);
                    cdl->AddRectFilled(ImVec2(stMin.x + 1.0f, stMax.y - 2.0f), ImVec2(stMin.x + pw, stMax.y - 1.0f),
                                       IM_COL32(255, 184, 0, 190), 1.0f);
                } else if (active) {
                    cdl->AddRectFilled(stMin, stMax, Alpha(XC_GREEN, 0.09f), statusH * 0.32f);
                    cdl->AddRect(stMin, stMax, Alpha(XC_GREEN, 0.40f), statusH * 0.32f, 0, 1.0f);
                    float sr = lineH * 0.28f;
                    cdl->AddCircleFilled(ImVec2(iconCx, iconCy), sr, XC_GREEN, 24);
                    cdl->AddLine(ImVec2(iconCx - sr * 0.5f, iconCy), ImVec2(iconCx - sr * 0.10f, iconCy + sr * 0.38f), IM_COL32(16, 30, 20, 255), 2.0f);
                    cdl->AddLine(ImVec2(iconCx - sr * 0.10f, iconCy + sr * 0.38f), ImVec2(iconCx + sr * 0.54f, iconCy - sr * 0.42f), IM_COL32(16, 30, 20, 255), 2.0f);
                    cdl->AddText(ImVec2(textX, iconCy - lineH * 0.5f), IM_COL32(160, 214, 172, 255),
                                 FitText(g_auth.message.empty() ? "License aktif" : g_auth.message, textMaxW).c_str());
                } else if (g_auth.hasResult) {
                    cdl->AddRectFilled(stMin, stMax, Alpha(XC_RED, 0.10f), statusH * 0.32f);
                    cdl->AddRect(stMin, stMax, Alpha(XC_RED, 0.45f), statusH * 0.32f, 0, 1.0f);
                    float sr = lineH * 0.28f;
                    cdl->AddCircleFilled(ImVec2(iconCx, iconCy), sr, XC_RED, 24);
                    float k = sr * 0.45f;
                    cdl->AddLine(ImVec2(iconCx - k, iconCy - k), ImVec2(iconCx + k, iconCy + k), IM_COL32(34, 14, 12, 255), 2.0f);
                    cdl->AddLine(ImVec2(iconCx + k, iconCy - k), ImVec2(iconCx - k, iconCy + k), IM_COL32(34, 14, 12, 255), 2.0f);
                    cdl->AddText(ImVec2(textX, iconCy - lineH * 0.5f), IM_COL32(234, 154, 146, 255),
                                 FitText(g_auth.message.empty() ? "Aktivasi gagal" : g_auth.message, textMaxW).c_str());
                } else {
                    cdl->AddRectFilled(stMin, stMax, IM_COL32(255, 255, 255, 7), statusH * 0.32f);
                    float sr = lineH * 0.26f;
                    cdl->AddCircle(ImVec2(iconCx, iconCy), sr, Alpha(XC_DIM, 1.0f), 20, 1.4f);
                    cdl->AddText(ImVec2(textX, iconCy - lineH * 0.5f), XC_DIM,
                                 FitText("Key terikat ke device ini (HWID)", textMaxW).c_str());
                }
                y = stMax.y + gapM;

                // ---------- tombol utama ----------
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                if (ThemeButton("##xloopPrimary", primary, ImVec2(innerW, btnH), true, g_auth.checking)) {
                    if (active) g_auth.cardAnim = 0.0f;
                    else        StartVerify(g_auth.keyInput, false);
                }
                if (submitted && !g_auth.checking) {
                    if (active) g_auth.cardAnim = 0.0f;
                    else        StartVerify(g_auth.keyInput, false);
                }
                y += btnH + gapM * 0.7f;

                // ---------- baris DEVICE + masa aktif ----------
                {
                    const char* devLabel = "DEVICE";
                    float lw = ImGui::CalcTextSize(devLabel).x;
                    cdl->AddText(ImVec2(x, y), XC_DIM, devLabel);

                    std::string right;
                    if (active) {
                        std::string d = g_auth.expiredText.empty() ? std::string("N/A") : g_auth.expiredText;
                        size_t sp = d.find(' ');
                        if (sp != std::string::npos) d = d.substr(0, sp);   // cukup tanggal
                        right = "Expires: " + d;
                    }
                    float rightW = right.empty() ? 0.0f : ImGui::CalcTextSize(right.c_str()).x;

                    std::string hwid = HardwareId();
                    std::string devShort = hwid.size() > 18 ? hwid.substr(0, 18) + "..." : hwid;
                    float devMaxW = innerW - lw - gapS - (right.empty() ? 0.0f : rightW + gapS);
                    cdl->AddText(ImVec2(x + lw + gapS, y), Alpha(XC_MUTED, 0.90f), FitText(devShort, devMaxW).c_str());
                    if (!right.empty())
                        cdl->AddText(ImVec2(x + innerW - rightW, y), Alpha(XC_GREEN, 0.95f), right.c_str());
                    y += lineH + gapS * 0.4f;
                }

                // ---------- footer (satu baris, dirapatkan ke kiri-kanan) ----------
                {
                    const char* brand = "@FiturinEngine";
                    const char* ask   = "Belum punya key? ";
                    const char* act   = "Chat owner";
                    float brandW = ImGui::CalcTextSize(brand).x;
                    float askW   = ImGui::CalcTextSize(ask).x;
                    float actW   = ImGui::CalcTextSize(act).x;
                    float needW  = brandW + gapM + askW + actW;

                    float brandX = x;
                    float actX   = x + innerW - actW;
                    float askX   = actX - askW;
                    if (needW > innerW) {
                        // layar sempit: brand naik satu baris sendiri
                        cdl->AddText(ImVec2(x, y), Alpha(XC_GOLD, 0.88f), brand);
                        y += lineH;
                        askX = x;
                        actX = x + askW;
                    }
                    cdl->AddText(ImVec2(askX, y), Alpha(XC_MUTED, 0.85f), ask);
                    cdl->AddText(ImVec2(actX, y), Alpha(XC_GOLD, 0.95f), act);
                    if (ImGui::IsMouseHoveringRect(ImVec2(actX, y), ImVec2(actX + actW, y + lineH))) {
                        cdl->AddLine(ImVec2(actX, y + lineH + 1.0f), ImVec2(actX + actW, y + lineH + 1.0f), Alpha(XC_GOLD, 0.85f), 1.0f);
                        if (io.MouseReleased[0]) OpenUrl("https://t.me/axell_08");
                    }
                    if (needW <= innerW) {
                        cdl->AddText(ImVec2(brandX, y), Alpha(XC_GOLD, 0.88f), brand);
                        if (ImGui::IsMouseHoveringRect(ImVec2(brandX, y), ImVec2(brandX + brandW, y + lineH))) {
                            cdl->AddLine(ImVec2(brandX, y + lineH + 1.0f), ImVec2(brandX + brandW, y + lineH + 1.0f), Alpha(XC_GOLD, 0.7f), 1.0f);
                            if (io.MouseReleased[0]) OpenUrl("https://t.me/FiturinEngine");
                        }
                    }
                }
            }
            ImGui::EndChild();

            // Fade: kalikan alpha seluruh vertex kartu frame ini.
            if (alpha < 0.999f) {
                for (int i = vtxStart; i < dl->VtxBuffer.Size; ++i) {
                    ImU32 c = dl->VtxBuffer[i].col;
                    ImU32 a = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
                    dl->VtxBuffer[i].col = (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor();
    }

    // ---------- toast mengapung ----------
    if (g_auth.toastTimer > 0.0f && !g_auth.toast.empty()) {
        const float a = ImClamp(g_auth.toastTimer / 0.55f, 0.0f, 1.0f);
        const float lineH = ImGui::GetTextLineHeight();
        std::string msg = FitText(g_auth.toast, io.DisplaySize.x * 0.80f);
        ImVec2 ts = ImGui::CalcTextSize(msg.c_str());
        const float round = 12.0f * scale;
        const float padT = XRound(lineH * 0.42f);
        ImVec2 tMin(io.DisplaySize.x * 0.5f - ts.x * 0.5f - padT * 2.4f, io.DisplaySize.y - 34.0f * scale - ts.y - padT * 2.0f);
        ImVec2 tMax(tMin.x + ts.x + padT * 4.8f, tMin.y + ts.y + padT * 2.0f);

        ImGui::SetNextWindowPos(ImVec2(tMin.x - 10.0f, tMin.y - 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(tMax.x - tMin.x + 20.0f, tMax.y - tMin.y + 20.0f), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin("##xloopToast", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoBackground)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 accent = F4(g_auth.toastColor[0], g_auth.toastColor[1], g_auth.toastColor[2], 1.0f);
            dl->AddRectFilled(ImVec2(tMin.x + 2, tMin.y + 6), ImVec2(tMax.x + 2, tMax.y + 6), IM_COL32(0, 0, 0, (int)(110 * a)), round);
            dl->AddRectFilled(tMin, tMax, IM_COL32(24, 22, 32, (int)(246 * a)), round);
            dl->AddRect(tMin, tMax, Alpha(accent, a * 0.65f), round, 0, 1.2f);
            dl->AddCircleFilled(ImVec2(tMin.x + padT * 1.5f, (tMin.y + tMax.y) * 0.5f), 3.2f * scale, Alpha(accent, a), 16);
            dl->AddText(ImVec2(tMin.x + padT * 2.6f, tMin.y + padT), Alpha(accent, a), msg.c_str());
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

} // namespace xloop
