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
    std::string netHint;                   // detail singkat saat koneksi gagal
    bool        diagRunning = false;
    std::string expiredText;
    int64_t     expiredEpoch    = 0;
    int         maxDevice       = 0;
    int         currentDevices  = 0;
    bool        fieldFocused    = false;   // field key sedang dipilih (kursor)
    bool        wantFocusInput  = false;   // kompat: sama dengan fieldFocused
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

// ======================= teks ramah untuk user =======================
// Pesan teknis dari license_core (Ed25519, payload, HWID, tool, server, dsb)
// sengaja TIDAK ditampilkan ke user. Kartu login cuma butuh kalimat singkat.
inline std::string FriendlyMessage(Reason r) {
    switch (r) {
        case Reason::EmptyKey:      return "Key kamu masih kosong";
        case Reason::BadKeyFormat:  return "Format key-nya belum pas";
        case Reason::Network:       return "Server belum bisa dihubungi, coba lagi";
        case Reason::RateLimited:   return "Terlalu banyak percobaan, tunggu sebentar";
        case Reason::InvalidKey:    return "Key-nya nggak dikenali";
        case Reason::KeyExpired:    return "Key kamu sudah habis";
        case Reason::KeyBanned:     return "Key ini sudah diblokir";
        case Reason::DeviceLimit:   return "Key sudah dipakai di device lain";
        case Reason::WrongTool:
        case Reason::WrongVersion:
        case Reason::ToolMismatch:  return "Key ini bukan untuk versi ini";
        case Reason::HwidMismatch:  return "Key ini terikat di device lain";
        default:                    return "Belum berhasil, coba lagi ya";
    }
}

// Label kecil di bawah field: cuma angka sisa hari, tanpa istilah teknis.
inline std::string ShortExpiry(const std::string& expiryText) {
    if (expiryText.empty() || expiryText == "N/A" || expiryText == "Lifetime") return std::string();
    // "2026-12-31 00:00 UTC" -> "31-12-2026"
    if (expiryText.size() >= 10 && expiryText[4] == '-' && expiryText[7] == '-') {
        return expiryText.substr(8, 2) + "-" + expiryText.substr(5, 2) + "-" + expiryText.substr(0, 4);
    }
    return expiryText;
}

inline void ApplyResult(const Result& r, bool silent) {
    g_auth.hasResult = true;
    g_auth.lastReason = r.reason;
    g_auth.loggedIn = r.ok;
    g_auth.checking = false;

    if (r.ok) {
        g_auth.message = "Masa aktif kamu masih jalan";
        g_auth.expiredEpoch = r.expiredEpoch;
        g_auth.expiredText = r.expiredEpoch > 0 ? FormatEpoch(r.expiredEpoch) : std::string("Lifetime");
        g_auth.maxDevice = r.maxDevice;
        g_auth.currentDevices = r.currentDevices;
        g_auth.reseller = r.reseller;
        g_auth.keyInput = r.key;
        g_auth.toastTimer = 0.0f;                 // kartu menutup sendiri, tak perlu toast
        FillCache(true, r.expiredEpoch, (int64_t)time(nullptr), r.key, r.secret);
    } else {
        // Pesan server sengaja tidak dipakai mentah-mentah: bahasanya teknis
        // (signature, payload, HWID, tool) dan bikin user bingung.
        g_auth.message = FriendlyMessage(r.reason);
        if (r.reason == Reason::Network) {
            // Simpan detail teknis di state (bukan di layar) + log sekali.
            g_auth.netHint = r.httpError.empty()
                           ? ("http=" + std::to_string(r.httpCode))
                           : r.httpError;
            Log("network gagal: %s", g_auth.netHint.c_str());
        }
        FillCache(false, 0, 0, "");
        if (!silent) {
            g_auth.toast = g_auth.message;
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
    g_auth.message = "Selamat datang kembali";
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
        g_auth.message = FriendlyMessage(reason);
        SetToast(g_auth.message, false);
        g_auth.fieldFocused = true;
        g_auth.wantFocusInput = true;
    };

    if (key.empty())            { failFast(Reason::EmptyKey); return; }
    if (!MatchesKeyRegex(key))  { failFast(Reason::BadKeyFormat); return; }

    if (Trim(XLOOP_PUBLIC_KEY_B64).empty()) {
        g_auth.hasResult = true;
        g_auth.checking = false;
        g_auth.lastReason = Reason::PublicKeyMissing;
        g_auth.message = "Fitur belum siap, hubungi admin";
        SetToast(g_auth.message, false);
        return;
    }

    g_auth.checking = true;
    g_auth.progress = 0.0f;
    g_lastRefreshAttempt = (int64_t)time(nullptr);
    if (!silent) g_auth.message = "Mengecek key...";

    std::thread([key, silent]() {
        Result r = VerifyKey(key);

        std::lock_guard<std::mutex> lock(g_authMutex);

        if (r.ok) {
            PersistSession(r);
            if (!LockDeviceOnce()) {
                ClearSession();
                Result denied;
                denied.reason = Reason::HwidMismatch;
                denied.message = FriendlyMessage(Reason::HwidMismatch);
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
                    g_auth.message = "Mode offline, key kamu masih aktif";
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
        g_auth.message = "Sesi habis, masukkan key lagi";
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
        g_auth.message = FriendlyMessage(Reason::KeyExpired);
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

// ------------------------------------------------------------------
// Tap handling khusus layar sentuh.
//
// Pola "hovered && io.MouseReleased[0]" TIDAK aman di Android: kalau jari
// bergeser sedikit sebelum diangkat, item sudah tidak hovered lagi saat UP
// sehingga tombol tidak pernah terpicu. Kalau jari digeser dan diangkat,
// ImGui juga membatalkan klik. Akibatnya semua tombol terasa "mati".
//
// Karena itu tombol di kartu login memakai tap sendiri: catat posisi DOWN,
// lalu saat UP terima tap kalau jaraknya masih di dalam toleransi (atau kalau
// masih di dalam rect tombol). Down di luar tombol dan up di dalam tombol juga
// diterima, supaya tap tetap responsif.
// ------------------------------------------------------------------
static ImVec2 g_tapDownPos    = ImVec2(-1.0f, -1.0f);
static ImVec2 g_tapUpPos      = ImVec2(-1.0f, -1.0f);
static int    g_tapUpFrame    = -1;
static ImGuiID g_tapDownId    = 0;
static int    g_tapLastFrame  = -1;
static bool   g_tapUpPending  = false;   // UP dari jalur sentuhan mentah (input.h)
static bool   g_tapDownIsRaw  = false;   // DOWN terakhir datang dari jalur mentah

// ---------------------------------------------------------------
// Jalur tap utama di Android TIDAK lewat state mouse ImGui.
//
// Hook sentuhan milik engine (include/input.h) memanggil OnRawTouch()
// langsung dari nativeTouchesBegin/End, jadi tap tetap terbaca walau
// state mouse/klik ImGui gagal terbentuk (posisi bergeser, klik
// dibatalkan, window tidak menerima fokus, dsb). Ini yang bikin semua
// tombol kartu login terasa mati sebelumnya.
// ---------------------------------------------------------------
inline void OnRawTouch(bool down, float x, float y) {
    if (down) {
        g_tapDownPos = ImVec2(x, y);
        g_tapUpPos = ImVec2(-1.0f, -1.0f);
        g_tapDownId = 0;
        g_tapUpPending = false;
        g_tapDownIsRaw = true;
    } else {
        g_tapUpPos = ImVec2(x, y);
        g_tapUpPending = true;
        g_tapDownPos = ImVec2(-1.0f, -1.0f);
        g_tapDownIsRaw = false;
    }
}

// Panggil sekali per frame, sebelum widget kartu digambar.
inline void BeginTapFrame() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;

    if (g_tapUpPending) {                      // tap selesai lewat jalur mentah
        g_tapUpFrame = g.FrameCount;
        g_tapUpPending = false;
        return;                                // jangan campur dengan state mouse ImGui
    }
    if (g_tapDownIsRaw) return;                 // masih dipegang lewat jalur mentah
    if (g_tapLastFrame == g.FrameCount) return; // sudah diproses frame ini
    g_tapLastFrame = g.FrameCount;

    if (io.MouseDown[0]) {
        if (g_tapDownPos.x < 0.0f) {            // DOWN baru (jalur ImGui)
            g_tapDownPos = io.MousePos;
            g_tapDownId = 0;
            g_tapDownIsRaw = false;
        }
    } else if (g_tapDownPos.x >= 0.0f) {        // dilepas lewat jalur ImGui
        if (g_tapUpFrame != g.FrameCount) {
            g_tapUpPos = io.MousePos;
            g_tapUpFrame = g.FrameCount;
        }
        // g_tapDownId sengaja tidak direset: TapInRect masih memakainya untuk
        // memastikan DOWN memang mendarat di tombol yang sama.
        g_tapDownPos = ImVec2(-1.0f, -1.0f);
    }
}

// true kalau frame ini ada tap (DOWN lalu UP) yang berakhir di dalam rect.
// Toleransi membuat tap tetap sah walau jari bergeser sedikit sebelum diangkat.
inline bool TapInRect(ImGuiID id, const ImRect& bb, float tolerance = 0.0f) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;

    // Toleransi standar: 0.85 baris teks (jari tidak pernah presisi, tapi tetap
    // tidak boleh "melebar" sampai menabrak tombol tetangga).
    // tolerance < 0 -> pakai rect apa adanya (0 px).
    const float tol = tolerance > 0.0f ? tolerance
                    : (tolerance < 0.0f ? 0.0f : ImGui::GetTextLineHeight() * 0.85f);
    ImRect hit(bb.Min - ImVec2(tol, tol), bb.Max + ImVec2(tol, tol));

    // DOWN di dalam tombol -> tombol ini yang "dipegang".
    // Cek g_tapDownPos saja (tidak bergantung io.MouseDown) supaya jalur
    // sentuhan mentah juga tercatat.
    if (g_tapDownPos.x >= 0.0f && g_tapDownId == 0 && hit.Contains(g_tapDownPos))
        g_tapDownId = id;

    if (g_tapUpFrame != g.FrameCount) return false;          // tidak ada UP di frame ini
    if (!hit.Contains(g_tapUpPos)) return false;             // UP di luar tombol
    // DOWN harus mendarat di tombol ini: mencegah tap "nyasar" dari area lain
    // memicu tombol yang bukan dituju (mis. tap di luar kartu lalu jari bergeser).
    if (g_tapDownId != id) return false;

    // Satu tap hanya boleh memicu satu tombol: setelah dipakai, tap ditutup.
    g_tapUpFrame = -1;
    g_tapUpPos = ImVec2(-1.0f, -1.0f);
    g_tapDownId = 0;
    return true;
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
    const bool tapped = !disabled && TapInRect(wid, bb);

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

    (void)g;
    return tapped;
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
    const bool tapped = !disabled && TapInRect(wid, bb);

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

    (void)g;
    return tapped;
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
// Sengaja dibuat sesederhana mungkin: judul, satu field key, satu tombol.
// Tidak ada istilah teknis (HWID, server, signature, format key) di layar.

inline void DrawLoginUI(ImGuiIO& io) {
    BeginTapFrame();
    float dt = io.DeltaTime;
    if (!g_auth.sessionLoaded) BootAuth();
    if (g_auth.toastTimer > 0.0f) g_auth.toastTimer -= dt;

    const bool active = g_auth.loggedIn;
    if (active) g_auth.cardAnim = ImMax(0.0f, g_auth.cardAnim - dt * 5.0f);
    else        g_auth.cardAnim = ImMin(1.0f, g_auth.cardAnim + dt * 5.0f);

    if (g_auth.checking) g_auth.progress = ImMin(1.0f, g_auth.progress + dt * 0.5f);

    const float alpha = g_auth.cardAnim;
    const float scale = ImClamp(io.DisplaySize.y / 900.0f, 0.55f, 1.0f);

    if (alpha > 0.01f) {
        const float lineH  = ImGui::GetTextLineHeight();
        const float padX   = XRound(lineH * 0.85f);
        const float padY   = XRound(lineH * 0.70f);
        const float gapS   = XRound(lineH * 0.30f);
        const float gapM   = XRound(lineH * 0.60f);
        float btnH   = XRound(lineH * 1.85f);
        float fieldH = XRound(lineH * 1.90f);

        const ImU32 accent = active ? XC_GREEN : XC_GOLD;

        const char* title   = active ? "Key kamu aktif" : "Masuk dulu yuk";
        const char* subtitle = active ? "Selamat bermain!" : "Tempel key kamu di bawah ini";
        const char* primary = g_auth.checking ? "Sebentar ya..." : (active ? "Lanjut" : "Masuk");

        // ---- lebar kartu: cukup untuk baris terpanjang, tidak melebihi layar ----
        float wTitle = ImGui::CalcTextSize(title).x;
        float wSub   = ImGui::CalcTextSize(subtitle).x;
        float wPaste = ImGui::CalcTextSize("PASTE").x + padX * 1.3f;
        float wPrim  = ImGui::CalcTextSize(primary).x + padX * 2.0f;
        float wField = ImGui::CalcTextSize("XLVP-XXXX-XXXX").x * 1.45f + fieldH + wPaste + gapS * 2.0f;
        float wFoot  = ImGui::CalcTextSize("@FiturinEngine").x + gapM + ImGui::CalcTextSize("Belum punya key? Chat owner").x;

        float minCardW = ImMax(ImMax(wTitle, wSub), ImMax(wPrim, wFoot)) + padX * 2.0f;
        minCardW = ImMax(minCardW, wField + padX * 2.0f);
        const float maxW = ImMax(280.0f, io.DisplaySize.x * 0.94f);
        const float cardW = XRound(ImClamp(minCardW, 330.0f, maxW));
        const float innerW = cardW - padX * 2.0f;

        // ---- tinggi kartu dihitung dari baris yang benar-benar dipakai ----
        float pH  = padY;
        float gT  = gapS * 0.40f;    // judul -> subjudul
        float gS2 = gapM * 0.90f;    // subjudul -> field
        float gN  = gapM * 0.85f;    // field/status -> tombol
        float gB  = gapS * 1.20f;    // tombol -> footer
        float fullH = pH * 2.5f + lineH * 4.0f + gT + gS2 + gN + gB + fieldH + btnH;
        const float maxH = io.DisplaySize.y * 0.92f;
        if (fullH > maxH) {
            float k = ImMax(0.55f, maxH / fullH);
            pH = pH * k; gT = gT * k; gS2 = gS2 * k; gN = gN * k; gB = gB * k;
            fieldH = ImMax(lineH * 1.45f, fieldH * k);
            btnH   = ImMax(lineH * 1.45f, btnH * k);
            fullH = pH * 2.5f + lineH * 4.0f + gT + gS2 + gN + gB + fieldH + btnH;
        }
        const float cardH = XRound(ImMin(fullH, maxH));

        float anim = EaseOutCubic(g_auth.cardAnim);
        float slide = (1.0f - anim) * 20.0f * scale;

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
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            const float t = (float)ImGui::GetTime();
            const float R = 18.0f * scale;

            // latar kartu + garis aksen tipis di atas
            dl->AddRectFilled(ImVec2(wp.x - 2, wp.y + 8), ImVec2(wp.x + cardW + 2, wp.y + cardH + 10), IM_COL32(0, 0, 0, 120), R + 3);
            dl->AddRectFilled(wp, ImVec2(wp.x + cardW, wp.y + cardH), IM_COL32(15, 14, 20, 252), R);
            dl->AddRect(wp, ImVec2(wp.x + cardW, wp.y + cardH), Alpha(accent, 0.28f), R, 0, 1.2f);
            {
                float sh = fmodf(t * 0.22f, 1.0f);
                float x0 = wp.x + cardW * (sh * 1.4f - 0.35f);
                dl->PushClipRect(wp, ImVec2(wp.x + cardW, wp.y + 3.0f * scale), true);
                dl->AddRectFilledMultiColor(ImVec2(x0 - 120.0f * scale, wp.y), ImVec2(x0 + 120.0f * scale, wp.y + 3.0f * scale),
                                            IM_COL32(255, 184, 0, 0), Alpha(accent, 1.0f),
                                            Alpha(accent, 1.0f), IM_COL32(255, 184, 0, 0));
                dl->PopClipRect();
            }

            // semua posisi dihitung dari pojok kartu (koordinat layar absolut)
            const float x = XRound(wp.x + padX);
            float y = XRound(wp.y + pH);

            // ---- judul + subjudul (tanpa istilah teknis) ----
            dl->AddText(ImVec2(x, y), XC_TEXT, title);
            y += lineH + gT;
            dl->AddText(ImVec2(x, y), XC_DIM, FitText(subtitle, innerW).c_str());
            y += lineH + gS2;

            // ---- field key + tombol tempel ----
            const float pasteW = ImGui::CalcTextSize("PASTE").x + padX * 1.3f;
            float inputW = innerW - fieldH - pasteW - gapS * 2.0f;
            if (inputW < ImGui::CalcTextSize("XLVP-XXXX-XXXX").x + padX)
                inputW = ImGui::CalcTextSize("XLVP-XXXX-XXXX").x + padX;

            ImVec2 fMin(x, y), fMax(x + inputW, y + fieldH);
            ImGuiID fieldId = ImGui::GetCurrentWindow()->GetID("##xloopKeyField");
            bool fieldHovered = fMin.x <= io.MousePos.x && io.MousePos.x <= fMax.x &&
                                fMin.y <= io.MousePos.y && io.MousePos.y <= fMax.y;
            bool fieldActive = g_auth.fieldFocused;

            // Field digambar manual: tidak memakai InputText supaya tidak butuh
            // keyboard virtual / state fokus ImGui (yang sering tidak muncul di
            // overlay game). Ketik manual tetap bisa lewat keyboard fisik;
            // untuk layar sentuh cukup tombol PASTE.
            dl->AddRectFilled(fMin, fMax, XC_FIELD, 10.0f * scale);
            if (TapInRect(fieldId, ImRect(fMin, fMax))) {
                g_auth.fieldFocused = true;
                g_auth.wantFocusInput = true;
            }

            {
                float tx = fMin.x + padX * 0.55f;
                float ty = fMin.y + (fieldH - lineH) * 0.5f;
                float textMaxW = (fMax.x - padX * 0.55f) - tx;
                std::string shown = g_auth.keyInput;
                if (!shown.empty() && !g_auth.showKey) shown = std::string(shown.size(), '*');
                if (shown.empty()) {
                    dl->AddText(ImVec2(tx, ty), Alpha(XC_DIM, 0.85f), "Tempel key di sini");
                } else {
                    dl->AddText(ImVec2(tx, ty), IM_COL32(255, 242, 219, 255), FitText(shown, textMaxW).c_str());
                }
                // kursor berkedip saat field dipilih
                if (g_auth.fieldFocused && fmodf(t, 1.0f) < 0.5f) {
                    float cx = tx;
                    if (!shown.empty()) {
                        std::string head = FitText(shown, textMaxW);
                        cx = tx + ImGui::CalcTextSize(head.c_str()).x + 2.0f;
                    }
                    dl->AddLine(ImVec2(cx, ty - 1.0f), ImVec2(cx, ty + lineH + 1.0f), Alpha(XC_GOLD, 0.9f), 1.6f);
                }
            }

            {
                ImU32 border = fieldActive  ? Alpha(accent, 0.95f)
                             : fieldHovered ? Alpha(accent, 0.62f)
                             : Alpha(XC_STROKE, 1.0f);
                dl->AddRect(fMin, fMax, border, 10.0f * scale, 0, fieldActive ? 1.6f : 1.2f);
            }

            // tombol mata show/hide
            ImGui::SetCursorScreenPos(ImVec2(fMax.x + gapS, fMin.y));
            if (CircleIconButton("##xloopEye", ImVec2(fMax.x + gapS, fMin.y), fieldH, 0, false))
                g_auth.showKey = !g_auth.showKey;

            // tombol tempel key dari clipboard
            ImGui::SetCursorScreenPos(ImVec2(fMax.x + gapS * 2.0f + fieldH, fMin.y));
            if (ThemeButton("##xloopPaste", "PASTE", ImVec2(pasteW, fieldH), false, g_auth.checking)) {
                std::string pasted = NormalizeKey(ReadClipboard());
                if (pasted.empty()) {
                    SetToast("Belum ada teks yang di-copy", false);
                } else if (MatchesKeyRegex(pasted)) {
                    g_auth.keyInput = pasted;
                    SetToast("Key ditempel", true);
                } else {
                    g_auth.keyInput = pasted;
                    SetToast("Yang di-copy bukan key", false);
                    g_auth.fieldFocused = true;
                }
            }
            y = fMax.y + gapS;

            // ---- satu baris kecil: status / masa aktif (opsional, tanpa istilah teknis) ----
            {
                std::string note;
                ImU32 noteCol = XC_DIM;
                if (g_auth.checking) {
                    note = g_auth.message.empty() ? "Mengecek key..." : g_auth.message;
                    noteCol = IM_COL32(255, 213, 79, 255);
                } else if (active) {
                    std::string d = ShortExpiry(g_auth.expiredText);
                    note = d.empty() ? "Masa aktifmu masih jalan" : ("Aktif sampai " + d);
                    noteCol = Alpha(XC_GREEN, 0.95f);
                } else if (g_auth.hasResult && !g_auth.message.empty()) {
                    note = g_auth.message;
                    noteCol = IM_COL32(234, 154, 146, 255);
                }
                if (!note.empty())
                    dl->AddText(ImVec2(x, y), noteCol, FitText(note, innerW).c_str());
                y += lineH + gN;
            }

            // ---- tombol utama ----
            ImGui::SetCursorScreenPos(ImVec2(x, y));
            if (ThemeButton("##xloopPrimary", primary, ImVec2(innerW, btnH), true, g_auth.checking)) {
                if (active) g_auth.cardAnim = 0.0f;
                else        StartVerify(g_auth.keyInput, false);
            }
            // Ketik manual (keyboard fisik / stik) tetap didukung walau field
            // digambar manual: karakter dari ImGui input queue + backspace + enter.
            if (g_auth.fieldFocused && !g_auth.checking) {
                ImGuiIO& kio = ImGui::GetIO();
                bool changed = false;
                for (int i = 0; i < kio.InputQueueCharacters.Size; ++i) {
                    ImWchar c = kio.InputQueueCharacters[i];
                    if (c >= 32 && c < 127 && g_auth.keyInput.size() < 32) {
                        g_auth.keyInput.push_back((char)c);
                        changed = true;
                    }
                }
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace), true) && !g_auth.keyInput.empty()) {
                    g_auth.keyInput.pop_back();
                    changed = true;
                }
                if (changed) kio.InputQueueCharacters.resize(0);
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter), false) ||
                    ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_KeyPadEnter), false)) {
                    if (active) g_auth.cardAnim = 0.0f;
                    else        StartVerify(g_auth.keyInput, false);
                }
            }
            y = XRound(y + btnH + gB);

            // ---- footer kecil: brand + bantuan (area tap diperbesar) ----
            {
                const char* brand = "@FiturinEngine";
                const char* ask   = "Butuh key? ";
                const char* act   = "Chat owner";
                float brandW = ImGui::CalcTextSize(brand).x;
                float askW   = ImGui::CalcTextSize(ask).x;
                float actW   = ImGui::CalcTextSize(act).x;
                float needW  = brandW + gapM + askW + actW;
                float footH  = lineH * 1.8f;
                float padT   = (footH - lineH) * 0.5f;

                float actX = x + innerW - actW;
                float askX = actX - askW;
                dl->AddText(ImVec2(askX, y + padT), Alpha(XC_MUTED, 0.80f), ask);
                dl->AddText(ImVec2(actX, y + padT), Alpha(XC_GOLD, 0.95f), act);

                ImGuiIO& fio = ImGui::GetIO();
                ImRect actBb(ImVec2(actX, y), ImVec2(actX + actW, y + footH));
                if (actBb.Contains(fio.MousePos))
                    dl->AddLine(ImVec2(actX, y + footH), ImVec2(actX + actW, y + footH), Alpha(XC_GOLD, 0.8f), 1.0f);
                if (TapInRect(ImGui::GetCurrentWindow()->GetID("##xloopChatOwner"), actBb, -1.0f))
                    OpenUrl("https://t.me/axell_08");

                if (needW <= innerW) {
                    dl->AddText(ImVec2(x, y + padT), Alpha(XC_GOLD, 0.85f), brand);
                    ImRect brandBb(ImVec2(x, y), ImVec2(x + brandW, y + footH));
                    if (brandBb.Contains(fio.MousePos))
                        dl->AddLine(ImVec2(x, y + footH), ImVec2(x + brandW, y + footH), Alpha(XC_GOLD, 0.7f), 1.0f);
                    if (TapInRect(ImGui::GetCurrentWindow()->GetID("##xloopBrand"), brandBb, -1.0f))
                        OpenUrl("https://t.me/FiturinEngine");
                } else {
                    dl->AddText(ImVec2(x, y + padT), Alpha(XC_GOLD, 0.85f), brand);
                }
            }

            // fade animasi masuk/keluar untuk semua vertex kartu
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
