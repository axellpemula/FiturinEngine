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

// ======================= UI primitives =======================
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
    dl->AddText(ImVec2(bb.Min.x + (size.x - ts.x) * 0.5f, bb.Min.y + (size.y - ts.y) * 0.5f), textCol, label);

    return hovered && !disabled && g.IO.MouseReleased[0];
}

// icon: 0 = mata, 1 = paste
inline bool CircleIconButton(const char* id, ImVec2 center, float radius, int icon, bool accent, bool disabled = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    ImGuiID wid = window->GetID(id);
    ImRect bb(center - ImVec2(radius, radius), center + ImVec2(radius, radius));
    ImGui::ItemSize(bb.GetSize(), 0.0f);
    if (!ImGui::ItemAdd(bb, wid)) return false;

    bool hovered = false, held = false;
    if (!disabled) ImGui::ButtonBehavior(bb, wid, &hovered, &held);
    LerpAnim(wid, (!disabled && (hovered || held)) ? 1.0f : 0.0f);
    float anim = AnimSlot(wid);

    ImDrawList* dl = window->DrawList;
    if (accent) {
        for (int i = 6; i >= 1; --i) {
            int a = (int)(18 * (1.0f - (float)i / 6.0f) * (0.6f + 0.4f * anim));
            if (a > 0) dl->AddCircleFilled(center, radius + i, IM_COL32(255, 184, 0, a), 48);
        }
        dl->AddCircleFilled(center, radius, held ? IM_COL32(198, 139, 0, 255) : IM_COL32(255, 184, 0, 255), 48);
        dl->AddCircle(center, radius, IM_COL32(255, 245, 200, 200), 48, 1.2f);
    } else {
        dl->AddCircleFilled(center, radius, IM_COL32(26, 24, 36, 255), 48);
        dl->AddCircle(center, radius, Alpha(XC_STROKE, 1.0f + anim), 48, 1.2f);
    }

    ImU32 col = accent ? IM_COL32(30, 19, 0, 255) : Alpha(XC_TEXT, disabled ? 0.4f : 0.80f + 0.20f * anim);
    float r = radius * 0.50f;
    if (icon == 0) {
        // mata (dengan garis coret kalau key sedang tersembunyi)
        dl->PathArcTo(center + ImVec2(0, r * 0.40f), r * 1.05f, IM_PI * 1.12f, IM_PI * 1.88f, 24);
        dl->PathStroke(col, 0, 1.6f);
        dl->PathArcTo(center - ImVec2(0, r * 0.40f), r * 1.05f, IM_PI * 0.12f, IM_PI * 0.88f, 24);
        dl->PathStroke(col, 0, 1.6f);
        dl->AddCircleFilled(center, r * 0.32f, col, 20);
    } else {
        dl->AddRect(ImVec2(center.x - r * 0.68f, center.y - r * 0.68f), ImVec2(center.x + r * 0.40f, center.y + r * 0.72f), col, 3.0f, 0, 1.6f);
        dl->AddRect(ImVec2(center.x - r * 0.34f, center.y - r * 0.92f), ImVec2(center.x + r * 0.78f, center.y + r * 0.48f), col, 3.0f, 0, 1.6f);
    }

    return hovered && !disabled && g.IO.MouseReleased[0];
}

inline void OpenUrl(const char* url) {
    std::string cmd = std::string("am start -a android.intent.action.VIEW -d ") + url + " > /dev/null 2>&1";
    system(cmd.c_str());
}

// Baris fitur di panel kiri (bullet gold + teks).
inline void DrawFeatureRow(ImDrawList* dl, ImVec2 at, float s, const char* text) {
    float r = 3.2f * s;
    ImVec2 c(at.x + r + 1.0f, at.y + ImGui::GetTextLineHeight() * 0.5f + 1.0f);
    dl->AddCircleFilled(c, r * 1.9f, IM_COL32(255, 184, 0, 34), 16);
    dl->AddCircleFilled(c, r, IM_COL32(255, 184, 0, 220), 16);
    dl->AddText(ImVec2(at.x + 18.0f * s, at.y), XC_MUTED, text);
}

// ======================= kartu login =======================
inline void DrawLoginUI(ImGuiIO& io) {
    float dt = io.DeltaTime;
    if (!g_auth.sessionLoaded) BootAuth();
    if (g_auth.toastTimer > 0.0f) g_auth.toastTimer -= dt;

    if (g_auth.loggedIn) g_auth.cardAnim = ImMax(0.0f, g_auth.cardAnim - dt * 5.0f);
    else                g_auth.cardAnim = ImMin(1.0f, g_auth.cardAnim + dt * 5.0f);

    if (g_auth.checking) g_auth.progress = ImMin(1.0f, g_auth.progress + dt * 0.5f);

    // Ukuran mengikuti layar: game jalan di landscape, kartu dibuat dua kolom.
    const float S = ImClamp(io.DisplaySize.y / 900.0f, 0.52f, 1.0f);
    const float cardW = XRound(ImMin(880.0f * S, io.DisplaySize.x * 0.86f));
    const float cardH = XRound(ImMin(432.0f * S, io.DisplaySize.y * 0.88f));
    const float leftW = XRound(cardW * 0.36f);
    const float padY  = XRound(34.0f * S);
    const float padX  = XRound(34.0f * S);
    const float alpha = g_auth.cardAnim;

    if (alpha > 0.01f) {
        float anim = EaseOutCubic(g_auth.cardAnim);
        float slide = (1.0f - anim) * 22.0f * S;

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f + slide), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, XRound(24.0f * S));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

        const int vtxStart = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        if (ImGui::Begin("##XloopLoginCard", nullptr, flags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            const float t = (float)ImGui::GetTime();
            const float R = XRound(24.0f * S);
            const bool  active = g_auth.loggedIn;
            const ImU32 accent = active ? XC_GREEN : XC_GOLD;

            // ---------- background kartu ----------
            dl->AddRectFilled(ImVec2(wp.x - 3, wp.y + 10), ImVec2(wp.x + cardW + 3, wp.y + cardH + 12), IM_COL32(0, 0, 0, 130), R + 4);
            dl->AddRectFilled(wp, ImVec2(wp.x + cardW, wp.y + cardH), IM_COL32(15, 14, 20, 252), R);

            // panel kiri (identitas produk)
            dl->PushClipRect(wp, ImVec2(wp.x + leftW, wp.y + cardH), true);
            dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + leftW, wp.y + cardH),
                                        IM_COL32(44, 34, 16, 255), IM_COL32(30, 24, 14, 255),
                                        IM_COL32(20, 18, 14, 255), IM_COL32(34, 27, 15, 255));
            {
                float pulse = 0.5f + 0.5f * sinf(t * 1.4f);
                dl->AddCircleFilled(ImVec2(wp.x + leftW * 0.5f, wp.y + cardH * 0.30f),
                                    leftW * (0.52f + 0.04f * pulse), IM_COL32(255, 184, 0, (int)(10 + 10 * pulse)), 64);
            }
            dl->PopClipRect();

            dl->AddLine(ImVec2(wp.x + leftW, wp.y + 20.0f * S), ImVec2(wp.x + leftW, wp.y + cardH - 20.0f * S),
                        IM_COL32(255, 245, 200, 26), 1.0f);
            dl->AddRect(wp, ImVec2(wp.x + cardW, wp.y + cardH), Alpha(accent, 0.30f), R, 0, 1.3f);

            // garis aksen tipis di tepi atas (animasi geser)
            {
                float sh = fmodf(t * 0.22f, 1.0f);
                float x0 = wp.x + cardW * (sh * 1.4f - 0.35f);
                dl->PushClipRect(wp, ImVec2(wp.x + cardW, wp.y + 3.0f * S), true);
                dl->AddRectFilledMultiColor(ImVec2(x0 - 140.0f * S, wp.y), ImVec2(x0 + 140.0f * S, wp.y + 3.0f * S),
                                            IM_COL32(255, 184, 0, 0), Alpha(accent, 1.0f),
                                            Alpha(accent, 1.0f), IM_COL32(255, 184, 0, 0));
                dl->PopClipRect();
            }

            // ---------- panel kiri ----------
            {
                float cx = wp.x + leftW * 0.5f;
                float logoR = 30.0f * S;
                ImVec2 lc(cx, wp.y + padY + logoR);
                float pulse = 0.5f + 0.5f * sinf(t * 2.0f);

                dl->AddCircleFilled(lc, logoR * 1.5f, IM_COL32(255, 184, 0, (int)(14 + 18 * pulse)), 56);
                dl->AddCircleFilled(lc, logoR, IM_COL32(28, 21, 10, 255), 48);
                dl->AddCircle(lc, logoR, Alpha(accent, 0.90f), 48, 2.0f);
                dl->AddCircle(lc, logoR - 1.5f * S, IM_COL32(255, 245, 200, (int)(40 + 55 * pulse)), 48, 1.0f);
                float k = logoR * 0.40f;
                dl->AddLine(lc - ImVec2(k, k), lc + ImVec2(k, k), IM_COL32(255, 213, 79, 255), 2.4f);
                dl->AddLine(lc + ImVec2(k, -k), lc + ImVec2(-k, k), IM_COL32(255, 213, 79, 255), 2.4f);

                float ty = lc.y + logoR + 16.0f * S;
                const char* brand = "FITURIN ENGINE";
                float bw = ImGui::CalcTextSize(brand).x;
                dl->AddText(ImVec2(cx - bw * 0.5f, ty), IM_COL32(255, 245, 200, 255), brand);

                float ty2 = ty + ImGui::GetTextLineHeight() + 6.0f * S;
                const char* tag = "PREMIUM 8 BALL POOL";
                float tw = ImGui::CalcTextSize(tag).x;
                dl->AddText(ImVec2(cx - tw * 0.5f, ty2), XC_DIM, tag);

                // badge status
                float by = ty2 + ImGui::GetTextLineHeight() + 14.0f * S;
                const char* badge = active ? "LICENSE ACTIVE" : "LICENSE REQUIRED";
                ImVec2 bs = ImGui::CalcTextSize(badge);
                float bpw = bs.x + 26.0f * S, bph = bs.y + 12.0f * S;
                ImVec2 bp(cx - bpw * 0.5f, by);
                ImVec2 be(bp.x + bpw, bp.y + bph);
                dl->AddRectFilled(bp, be, Alpha(accent, 0.16f), bph * 0.5f);
                dl->AddRect(bp, be, Alpha(accent, 0.65f), bph * 0.5f, 0, 1.1f);
                dl->AddCircleFilled(ImVec2(bp.x + 13.0f * S, bp.y + bph * 0.5f), 3.0f * S, Alpha(accent, 0.95f), 12);
                dl->AddText(ImVec2(bp.x + 22.0f * S, bp.y + 6.0f * S), Alpha(accent, 0.95f), badge);

                // fitur singkat
                float fy = be.y + 24.0f * S;
                float fx = wp.x + 26.0f * S;
                DrawFeatureRow(dl, ImVec2(fx, fy), S, "Verified by Ed25519");
                DrawFeatureRow(dl, ImVec2(fx, fy + 22.0f * S), S, "HWID locked per device");
                DrawFeatureRow(dl, ImVec2(fx, fy + 44.0f * S), S, "Auto refresh every 10 min");

                // footer kiri
                ImGui::SetCursorScreenPos(ImVec2(wp.x + 20.0f * S, wp.y + cardH - 60.0f * S));
                if (ThemeButton("##xloopTg", "Join  @FiturinEngine", ImVec2(leftW - 40.0f * S, 38.0f * S), false)) {
                    OpenUrl("https://t.me/FiturinEngine");
                }
            }

            // ---------- panel kanan ----------
            const float rx = wp.x + leftW + padX;
            const float rw = cardW - leftW - padX * 2.0f;
            float y = wp.y + padY;

            const char* title = active ? "LICENSE ACTIVATED" : "ENTER YOUR LICENSE KEY";
            dl->AddText(ImVec2(rx, y), IM_COL32(240, 236, 250, 255), title);
            y += ImGui::GetTextLineHeight() + 5.0f * S;
            dl->AddText(ImVec2(rx, y), XC_DIM,
                        active ? "Key tersimpan di perangkat ini. Klik continue untuk masuk menu."
                               : "Tempel key dari reseller/owner, lalu tekan ACTIVATE.");
            y += ImGui::GetTextLineHeight() + 18.0f * S;

            float fieldH = XRound(52.0f * S);
            float pasteW = XRound(80.0f * S);
            float eyeW   = fieldH;
            float inputW = rw - pasteW - eyeW - 20.0f * S;

            // label (+ masked key kalau sudah aktif)
            dl->AddText(ImVec2(rx, y), XC_DIM, "LICENSE KEY");
            {
                std::string hint = "Format: XLVP-XXXX-XXXX";
                if (active && g_auth.keyInput.size() >= 14) {
                    hint = "XLVP-****-" + g_auth.keyInput.substr(g_auth.keyInput.size() - 4);
                }
                ImVec2 fs = ImGui::CalcTextSize(hint.c_str());
                dl->AddText(ImVec2(rx + rw - fs.x, y),
                            active ? Alpha(XC_GREEN, 0.85f) : Alpha(XC_DIM, 0.75f), hint.c_str());
            }
            y += ImGui::GetTextLineHeight() + 8.0f * S;

            ImVec2 fMin(rx, y);
            ImVec2 fMax(rx + inputW, y + fieldH);
            dl->AddRectFilled(fMin, fMax, XC_FIELD, 12.0f * S);

            ImGui::SetCursorScreenPos(ImVec2(fMin.x + 14.0f * S, fMin.y + (fieldH - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.95f, 0.86f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.45f, 0.44f, 0.52f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(inputW - 28.0f * S);

            char buf[128];
            snprintf(buf, sizeof(buf), "%s", g_auth.keyInput.c_str());
            ImGuiInputTextFlags iflags = ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll |
                                         ImGuiInputTextFlags_EnterReturnsTrue;
            if (!g_auth.showKey) iflags |= ImGuiInputTextFlags_Password;
            if (g_auth.wantFocusInput) { ImGui::SetKeyboardFocusHere(); g_auth.wantFocusInput = false; }

            bool submitted = ImGui::InputTextWithHint("##xloopKeyField", "XLVP-XXXX-XXXX", buf, sizeof(buf), iflags);
            bool fieldActive = ImGui::IsItemActive();
            bool fieldHovered = ImGui::IsItemHovered();
            g_auth.keyInput = buf;                      // selalu sinkron (paste keyboard dsb.)
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                g_auth.keyInput = NormalizeKey(g_auth.keyInput);
                if (g_auth.keyInput.size() > sizeof(buf) - 1) g_auth.keyInput.resize(sizeof(buf) - 1);
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(6);

            ImU32 border = fieldActive  ? Alpha(active ? XC_GREEN : XC_GOLD_SOFT, 0.95f)
                         : fieldHovered ? Alpha(active ? XC_GREEN : XC_GOLD, 0.65f)
                         : Alpha(XC_STROKE, 1.0f);
            dl->AddRect(fMin, fMax, border, 12.0f * S, 0, fieldActive ? 1.6f : 1.2f);
            if (fieldActive)
                dl->AddRectFilled(ImVec2(fMin.x + 10.0f * S, fMax.y - 2.0f * S),
                                  ImVec2(fMax.x - 10.0f * S, fMax.y), Alpha(active ? XC_GREEN : XC_GOLD, 0.75f), 1.0f);

            // tombol mata
            ImVec2 eyeC(fMax.x + 8.0f * S + eyeW * 0.5f, fMin.y + fieldH * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(eyeC.x - eyeW * 0.5f, eyeC.y - fieldH * 0.5f));
            if (CircleIconButton("##xloopEye", eyeC, eyeW * 0.5f, 0, false)) {
                g_auth.showKey = !g_auth.showKey;
            }

            // tombol paste
            ImGui::SetCursorScreenPos(ImVec2(fMax.x + eyeW + 12.0f * S, fMin.y));
            if (ThemeButton("##xloopPaste", "PASTE", ImVec2(pasteW, fieldH), false, g_auth.checking)) {
                const char* clip = ImGui::GetClipboardText();
                std::string pasted = clip ? NormalizeKey(clip) : std::string();
                if (pasted.empty()) {
                    SetToast("Clipboard is empty. Copy your key first", false);
                } else if (MatchesKeyRegex(pasted)) {
                    g_auth.keyInput = pasted;
                    SetToast("Key pasted from clipboard", true);
                } else {
                    g_auth.keyInput = pasted;
                    SetToast("Clipboard is not a XLVP-XXXX-XXXX key", false);
                    g_auth.wantFocusInput = true;
                }
            }

            y = fMax.y + 14.0f * S;

            // ---------- status / info ----------
            float statusH = XRound(44.0f * S);
            ImVec2 stMin(rx, y);
            ImVec2 stMax(rx + rw, y + statusH);
            float iconCx = stMin.x + 22.0f * S;
            float iconCy = (stMin.y + stMax.y) * 0.5f;

            if (g_auth.checking) {
                dl->AddRectFilled(stMin, stMax, IM_COL32(255, 184, 0, 18), 11.0f * S);
                dl->AddRect(stMin, stMax, IM_COL32(255, 184, 0, 70), 11.0f * S, 0, 1.0f);
                dl->AddCircle(ImVec2(iconCx, iconCy), 7.5f * S, IM_COL32(255, 184, 0, 60), 24, 2.0f);
                dl->PathArcTo(ImVec2(iconCx, iconCy), 7.5f * S, t * 4.5f, t * 4.5f + 4.2f, 20);
                dl->PathStroke(IM_COL32(255, 213, 79, 255), 0, 2.0f);
                dl->AddText(ImVec2(iconCx + 16.0f * S, iconCy - ImGui::GetTextLineHeight() * 0.5f),
                            IM_COL32(255, 213, 79, 255), "Verifying with license server...");
                float pw = (stMax.x - stMin.x) * ImClamp(g_auth.progress, 0.0f, 1.0f);
                dl->AddRectFilled(ImVec2(stMin.x, stMax.y - 2.0f * S), ImVec2(stMin.x + pw, stMax.y), IM_COL32(255, 184, 0, 190), 1.0f);
            } else if (g_auth.loggedIn) {
                dl->AddRectFilled(stMin, stMax, Alpha(XC_GREEN, 0.09f), 11.0f * S);
                dl->AddRect(stMin, stMax, Alpha(XC_GREEN, 0.40f), 11.0f * S, 0, 1.0f);
                dl->AddCircleFilled(ImVec2(iconCx, iconCy), 8.5f * S, XC_GREEN, 24);
                dl->AddLine(ImVec2(iconCx - 4.2f * S, iconCy), ImVec2(iconCx - 1.0f * S, iconCy + 3.2f * S), IM_COL32(16, 30, 20, 255), 2.0f);
                dl->AddLine(ImVec2(iconCx - 1.0f * S, iconCy + 3.2f * S), ImVec2(iconCx + 4.6f * S, iconCy - 3.6f * S), IM_COL32(16, 30, 20, 255), 2.0f);
                dl->AddText(ImVec2(iconCx + 16.0f * S, iconCy - ImGui::GetTextLineHeight() * 0.5f),
                            Alpha(XC_GREEN, 1.25f), g_auth.message.c_str());
            } else if (g_auth.hasResult) {
                dl->AddRectFilled(stMin, stMax, Alpha(XC_RED, 0.10f), 11.0f * S);
                dl->AddRect(stMin, stMax, Alpha(XC_RED, 0.45f), 11.0f * S, 0, 1.0f);
                dl->AddCircleFilled(ImVec2(iconCx, iconCy), 8.5f * S, XC_RED, 24);
                float k = 3.8f * S;
                dl->AddLine(ImVec2(iconCx - k, iconCy - k), ImVec2(iconCx + k, iconCy + k), IM_COL32(34, 14, 12, 255), 2.0f);
                dl->AddLine(ImVec2(iconCx + k, iconCy - k), ImVec2(iconCx - k, iconCy + k), IM_COL32(34, 14, 12, 255), 2.0f);
                dl->AddText(ImVec2(iconCx + 16.0f * S, iconCy - ImGui::GetTextLineHeight() * 0.5f),
                            IM_COL32(234, 154, 146, 255), g_auth.message.c_str());
            } else {
                dl->AddRectFilled(stMin, stMax, IM_COL32(255, 255, 255, 7), 11.0f * S);
                dl->AddCircle(ImVec2(iconCx, iconCy), 7.5f * S, Alpha(XC_DIM, 1.0f), 24, 1.4f);
                dl->AddText(ImVec2(iconCx + 16.0f * S, iconCy - ImGui::GetTextLineHeight() * 0.5f),
                            XC_DIM, "Key terikat ke device ini (HWID). Jangan dibagikan.");
            }

            y = stMax.y + 14.0f * S;

            // ---------- tombol utama ----------
            char primary[64];
            if (g_auth.checking)      snprintf(primary, sizeof(primary), "VERIFYING...");
            else if (g_auth.loggedIn) snprintf(primary, sizeof(primary), "CONTINUE TO MENU");
            else                      snprintf(primary, sizeof(primary), "ACTIVATE LICENSE");

            ImGui::SetCursorScreenPos(ImVec2(rx, y));
            if (ThemeButton("##xloopPrimary", primary, ImVec2(rw, XRound(52.0f * S)), true, g_auth.checking)) {
                if (g_auth.loggedIn) g_auth.cardAnim = 0.0f;
                else                 StartVerify(g_auth.keyInput, false);
            }
            if (submitted && !g_auth.checking) {
                if (g_auth.loggedIn) g_auth.cardAnim = 0.0f;
                else                 StartVerify(g_auth.keyInput, false);
            }
            y += XRound(52.0f * S) + 12.0f * S;

            // ---------- baris info kecil ----------
            {
                std::string hwid = HardwareId();
                std::string shortHwid = hwid.size() > 34 ? hwid.substr(0, 34) + ".." : hwid;
                dl->AddText(ImVec2(rx, y), XC_DIM, "DEVICE");
                dl->AddText(ImVec2(rx + 62.0f * S, y), Alpha(XC_MUTED, 0.9f), shortHwid.c_str());

                if (g_auth.expiredEpoch > 0 || g_auth.loggedIn) {
                    const char* ex = g_auth.expiredText.empty() ? "N/A" : g_auth.expiredText.c_str();
                    float ew = ImGui::CalcTextSize(ex).x;
                    dl->AddText(ImVec2(rx + rw - ew, y), Alpha(active ? XC_GREEN : XC_GOLD_SOFT, 0.95f), ex);
                }

                float ownerY = y + ImGui::GetTextLineHeight() + 10.0f * S;
                const char* ask = "Belum punya key? ";
                const char* act = "Chat owner";
                dl->AddText(ImVec2(rx, ownerY), Alpha(XC_MUTED, 0.85f), ask);
                float ax = rx + ImGui::CalcTextSize(ask).x;
                dl->AddText(ImVec2(ax, ownerY), Alpha(XC_GOLD, 0.95f), act);
                ImVec2 aMin(ax, ownerY);
                ImVec2 aMax(ax + ImGui::CalcTextSize(act).x, ownerY + ImGui::GetTextLineHeight());
                if (ImGui::IsMouseHoveringRect(aMin, aMax)) {
                    dl->AddLine(ImVec2(aMin.x, aMax.y + 1.0f), ImVec2(aMax.x, aMax.y + 1.0f), Alpha(XC_GOLD, 0.85f), 1.0f);
                    if (io.MouseReleased[0]) OpenUrl("https://t.me/axell_08");
                }

                // footer kanan
                const char* powered = "Powered by XLOOP PANEL  -  Ed25519 signed";
                ImVec2 ps = ImGui::CalcTextSize(powered);
                dl->AddText(ImVec2(rx + rw - ps.x, wp.y + cardH - 26.0f * S), Alpha(XC_DIM, 0.75f), powered);
            }

            // Fade kartu: kalikan alpha semua vertex kartu di frame ini supaya
            // animasi masuk/keluar berlaku juga untuk drawing manual di atas.
            if (alpha < 0.999f) {
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                for (int i = vtxStart; i < cdl->VtxBuffer.Size; ++i) {
                    ImU32 c = cdl->VtxBuffer[i].col;
                    ImU32 a = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
                    cdl->VtxBuffer[i].col = (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }

    // ---------- toast mengapung ----------
    if (g_auth.toastTimer > 0.0f && !g_auth.toast.empty()) {
        float a = ImClamp(g_auth.toastTimer / 0.55f, 0.0f, 1.0f);
        ImVec2 ts = ImGui::CalcTextSize(g_auth.toast.c_str());
        float round = 14.0f * S;
        ImVec2 tMin(io.DisplaySize.x * 0.5f - ts.x * 0.5f - 26.0f * S, io.DisplaySize.y - 54.0f * S - 22.0f * S);
        ImVec2 tMax(tMin.x + ts.x + 52.0f * S, tMin.y + ts.y + 24.0f * S);

        ImGui::SetNextWindowPos(ImVec2(tMin.x - 40.0f, tMin.y - 30.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(tMax.x - tMin.x + 80.0f, tMax.y - tMin.y + 60.0f), ImGuiCond_Always);
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
            dl->AddRectFilled(ImVec2(tMin.x + 12.0f * S, tMin.y + 8.0f * S),
                              ImVec2(tMin.x + 15.0f * S, tMax.y - 8.0f * S), Alpha(accent, a * 0.85f), 1.5f);
            dl->AddCircleFilled(ImVec2(tMin.x + 26.0f * S, (tMin.y + tMax.y) * 0.5f), 3.4f * S, Alpha(accent, a), 16);
            dl->AddText(ImVec2(tMin.x + 36.0f * S, tMin.y + 12.0f * S), Alpha(accent, a), g_auth.toast.c_str());
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

} // namespace xloop
