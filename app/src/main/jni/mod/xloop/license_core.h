#pragma once
//Original src For free by @xvaluex : @FiturinEngine.
//
// Core lisensi XLOOP PANEL (axellpemula/XLOOP-PANEL).
//
// File ini TIDAK boleh bergantung pada ImGui / menu: dipakai juga saat
// auto-login, saat validasi ulang berkala, dan saat fitur penting dibuka.
//
// Alur sesuai docs/CLIENT_INTEGRATION.md di repo panel:
//   1. nonce baru tiap request (tidak pernah dipakai ulang)
//   2. POST /api/validate  { key, tool_slug, tool_version, hardware_id, nonce }
//   3. response harus { payload, signature } -> selain itu anggap gagal
//   4. verifikasi Ed25519 atas BYTE payload (base64 di-decode dulu, JANGAN di-JSON ulang)
//   5. cek payload: valid / nonce / hardware_id / tool / version / expired_at
//   6. secret_payload hanya di memori

#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <android/log.h>

#include <curl/curl.h>

#include "json/json.hpp"
#include "ed25519/ed25519.h"

// sha512.c dibangun sebagai C, sedangkan header aslinya tidak memakai
// extern "C" -> deklarasikan ulang di sini supaya linkage-nya cocok.
extern "C" int sha512(const unsigned char* message, size_t message_len, unsigned char* out);

// ============================ KONFIGURASI ============================
// Ganti 4 nilai di bawah ini sesuai panel kamu (halaman Tool Management).
// Tool slug & version harus sama persis dengan yang terdaftar di panel,
// kalau beda server akan menjawab "wrong_tool" / "wrong_version".
// =====================================================================
#ifndef XLOOP_PANEL_URL
#define XLOOP_PANEL_URL   "https://panel.xloop.dev"
#endif
#ifndef XLOOP_TOOL_SLUG
#define XLOOP_TOOL_SLUG   "fiturinengine"
#endif
#ifndef XLOOP_TOOL_VERSION
#define XLOOP_TOOL_VERSION "1.0.0"
#endif
// Public key Ed25519 (base64, 32 byte) dari panel -> Tool Management -> Public key.
// WAJIB diganti: selama masih placeholder semua validasi akan ditolak.
#ifndef XLOOP_PUBLIC_KEY_B64
#define XLOOP_PUBLIC_KEY_B64 ""
#endif

namespace xloop {

using json = nlohmann::json;

// ---------------------------- utils ----------------------------
inline void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "XLOOP", fmt, ap);
    va_end(ap);
}

inline std::string HttpUserAgent() {
    return std::string("FiturinEngine/") + XLOOP_TOOL_VERSION + " (Android; XLOOP license client)";
}

inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

inline void ToLowerInPlace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
}

inline void ToUpperInPlace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
}

inline std::string JsonStr(const json& j, const char* key, const std::string& def = "") {
    if (!j.is_object()) return def;
    auto it = j.find(key);
    if (it == j.end()) return def;
    if (it->is_string()) return it->get<std::string>();
    return def;
}

inline int64_t JsonInt(const json& j, const char* key, int64_t def = 0) {
    if (!j.is_object()) return def;
    auto it = j.find(key);
    if (it == j.end()) return def;
    if (it->is_number_integer()) return it->get<int64_t>();
    if (it->is_number_float()) return (int64_t)it->get<double>();
    return def;
}

inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Key panel selalu berbentuk XLVP-XXXX-XXXX (dipaksa upper-case oleh server).
inline std::string NormalizeKey(const std::string& raw) {
    std::string k = Trim(raw);
    k.erase(std::remove_if(k.begin(), k.end(), [](unsigned char c) { return c == ' ' || c == '\t'; }), k.end());
    k.erase(std::remove(k.begin(), k.end(), '\r'), k.end());
    k.erase(std::remove(k.begin(), k.end(), '\n'), k.end());
    ToUpperInPlace(k);
    return k;
}

inline bool MatchesKeyRegex(const std::string& k) {
    static const char* prefix = "XLVP-";
    if (k.size() != 14) return false;
    if (k.compare(0, 5, prefix) != 0) return false;
    if (k[9] != '-') return false;
    for (size_t i = 5; i < 14; ++i) {
        if (i == 9) continue;
        char c = k[i];
        bool alnum = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum) return false;
    }
    return true;
}

// ---------------------------- base64 ----------------------------
inline std::string Base64Encode(const unsigned char* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        unsigned v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];  out += tbl[v & 63];
    }
    if (i < len) {
        unsigned v = data[i] << 16;
        bool two = (i + 1 < len);
        if (two) v |= data[i + 1] << 8;
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += two ? tbl[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

inline int Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

inline bool Base64Decode(const std::string& in, std::string& out) {
    out.clear();
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int v = Base64Value(c);
        if (v < 0) return false;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((char)((buf >> bits) & 0xFF));
        }
    }
    return true;
}

// Kunci publik boleh dikasih base64, hex, atau PEM (SPKI) seperti yang
// ditampilkan panel. Semua diubah jadi 32 byte mentah.
inline bool ParsePublicKey(const std::string& raw, unsigned char out[32]) {
    std::string v = Trim(raw);
    if (v.empty()) return false;

    if (v.find("BEGIN PUBLIC KEY") != std::string::npos) {
        std::string b64;
        size_t p = 0;
        while ((p = v.find("-----", p)) != std::string::npos) {
            size_t nl = v.find('\n', p);
            if (nl == std::string::npos) break;
            size_t next = v.find("-----", nl);
            if (next == std::string::npos) break;
            b64 += v.substr(nl + 1, next - nl - 1);
            p = next + 5;
        }
        std::string der;
        if (!Base64Decode(b64, der)) return false;
        if (der.size() < 32) return false;
        memcpy(out, der.data() + der.size() - 32, 32);
        return true;
    }

    // hex 64 karakter
    if (v.size() == 64) {
        bool hex = true;
        for (char c : v) if (!isxdigit((unsigned char)c)) { hex = false; break; }
        if (hex) {
            for (int i = 0; i < 32; ++i) {
                unsigned b = 0;
                sscanf(v.c_str() + i * 2, "%2x", &b);
                out[i] = (unsigned char)b;
            }
            return true;
        }
    }

    std::string bin;
    if (!Base64Decode(v, bin)) return false;
    if (bin.size() == 32) { memcpy(out, bin.data(), 32); return true; }
    if (bin.size() > 32)  { memcpy(out, bin.data() + (bin.size() - 32), 32); return true; } // DER SPKI
    return false;
}

inline std::string Base64UrlEncode(const unsigned char* data, size_t len) {
    std::string b = Base64Encode(data, len);
    for (auto& c : b) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!b.empty() && b.back() == '=') b.pop_back();
    return b;
}

// nonce 24 byte acak -> 32 char base64url, cocok dengan NONCE_REGEX panel
inline std::string RandomNonce() {
    unsigned char buf[24];
    bool ok = false;
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        ok = (fread(buf, 1, sizeof(buf), f) == sizeof(buf));
        fclose(f);
    }
    if (!ok) {
        uint64_t s = (uint64_t)time(nullptr) ^ ((uint64_t)getpid() << 32) ^ (uint64_t)(uintptr_t)&buf;
        for (size_t i = 0; i < sizeof(buf); ++i) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            buf[i] = (unsigned char)(s >> 33);
        }
    }
    return Base64UrlEncode(buf, sizeof(buf));
}

// ---------------------------- hardware id ----------------------------
// Stabil per device, 16-128 char [A-Za-z0-9_:.-]. Di-hash SHA-256 dari
// (ANDROID_ID + model + brand + board + serial) supaya tidak membocorkan
// identitas mentah ke server.
inline std::string ReadFileString(const char* path, size_t maxLen = 256) {
    std::string out;
    FILE* f = fopen(path, "rb");
    if (!f) return out;
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n == 0) return out;
    out.assign(buf, n);
    out = Trim(out);
    while (!out.empty() && (out.back() == '\0' || out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    if (out.size() > maxLen) out.resize(maxLen);
    return out;
}

// Diisi oleh main.cpp (lihat setAndroidIdProvider di bawah) supaya
// ANDROID_ID asli (Settings.Secure, butuh JNI) ikut jadi bahan HWID.
typedef std::string (*AndroidIdProvider)();
inline AndroidIdProvider& AndroidIdHook() {
    static AndroidIdProvider fn = nullptr;
    return fn;
}
inline void SetAndroidIdProvider(AndroidIdProvider fn) { AndroidIdHook() = fn; }

inline std::string HardwareId() {
    static std::string cached;
    if (!cached.empty()) return cached;

    // ANDROID_ID adalah identitas paling stabil di Android 8+. MAC wifi bisa
    // berubah / kosong saat wifi mati, jadi hanya dipakai sebagai pelengkap.
    std::string androidId;
    if (AndroidIdHook()) {
        androidId = AndroidIdHook()();
        androidId = Trim(androidId);
    }
    std::string macWifi = ReadFileString("/sys/class/net/wlan0/address");
    if (macWifi.empty()) macWifi = ReadFileString("/sys/class/net/eth0/address");
    std::string bootId    = ReadFileString("/proc/sys/kernel/random/boot_id");
    std::string serial    = ReadFileString("/proc/cpuinfo");
    std::string model, brand, device, board;

    // Ambil beberapa properti device lewat file system (tanpa JNI).
    std::string props = ReadFileString("/system/build.prop", 8192);
    auto pick = [&](const char* key) -> std::string {
        size_t p = props.find(std::string(key) + "=");
        if (p == std::string::npos) return std::string();
        size_t s = p + strlen(key) + 1;
        size_t e = props.find('\n', s);
        if (e == std::string::npos) e = props.size();
        return Trim(props.substr(s, e - s));
    };
    model  = pick("ro.product.model");
    brand  = pick("ro.product.brand");
    device = pick("ro.product.device");
    board  = pick("ro.product.board");
    if (model.empty()) model = "unknown-model";

    // Buang baris cpuinfo yang tidak stabil (hanya ambil processor + Hardware).
    std::string cpuStable;
    {
        size_t p = 0;
        while ((p = serial.find("Hardware", p)) != std::string::npos) {
            size_t e = serial.find('\n', p);
            if (e == std::string::npos) break;
            cpuStable += Trim(serial.substr(p, e - p));
            p = e;
        }
    }

    std::string raw = androidId + "|" + macWifi + "|" + model + "|" + brand + "|" + device + "|" + board + "|" +
                      bootId + "|" + cpuStable;

    unsigned char digest[64];
    sha512((const unsigned char*)raw.data(), raw.size(), digest);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 32; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 0xF]);
    }
    cached = "android:" + out;
    return cached;
}

// ---------------------------- storage ----------------------------
inline std::string StorageDir() {
    std::string pkg = ReadFileString("/proc/self/cmdline", 200);
    if (pkg.empty()) pkg = "com.miniclip.eightballpool";
    std::string dir = "/data/user_de/0/" + pkg + "/no_backup/xloop";
    return dir;
}

inline void EnsureDir(const std::string& dir) {
    struct stat st;
    if (stat(dir.c_str(), &st) == 0) return;
    std::string cur;
    for (size_t i = 0; i < dir.size(); ++i) {
        cur.push_back(dir[i]);
        if (dir[i] == '/') {
            if (cur.size() > 1) mkdir(cur.c_str(), 0700);
        }
    }
    mkdir(dir.c_str(), 0700);
}

inline std::string SessionPath() { return StorageDir() + "/license.dat"; }
inline std::string SessionLockPath() { return StorageDir() + "/license.lock"; }

// Session tidak menyimpan key mentah: cukup hash key + HWID + expiry.
inline std::string Sha256Hex(const std::string& data) {
    unsigned char digest[64];
    sha512((const unsigned char*)data.data(), data.size(), digest);
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 32; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 0xF]);
    }
    return out;
}

inline std::string KeyHash(const std::string& key) { return Sha256Hex("xloop:" + NormalizeKey(key)); }
inline std::string DeviceHash()             { return Sha256Hex("dev:" + HardwareId()); }

inline bool SaveSession(const json& session) {
    EnsureDir(StorageDir());
    FILE* f = fopen(SessionPath().c_str(), "wb");
    if (!f) return false;
    std::string dump = session.dump();
    fwrite(dump.data(), 1, dump.size(), f);
    fclose(f);
    return true;
}

inline json LoadSession() {
    FILE* f = fopen(SessionPath().c_str(), "rb");
    if (!f) return json::object();
    std::string raw;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) raw.append(buf, n);
    fclose(f);
    if (raw.empty()) return json::object();
    try {
        json j = json::parse(raw);
        if (j.is_object()) return j;
    } catch (...) {}
    return json::object();
}

// Key disimpan dalam bentuk ter-obfuscate (XOR keystream dari device hash).
// Ini BUKAN enkripsi kuat: session file tetap bisa dipalsukan oleh yang
// paham RE, tujuannya hanya menghindari key terbaca plaintext di storage.
inline std::vector<unsigned char> KeyStream(size_t len) {
    std::string dev = DeviceHash();
    std::vector<unsigned char> out;
    out.reserve(len + 64);
    std::string block = dev + "|xloop-keystream";
    while (out.size() < len) {
        unsigned char digest[64];
        sha512((const unsigned char*)block.data(), block.size(), digest);
        out.insert(out.end(), digest, digest + 64);
        block = std::string((const char*)digest, 32);
    }
    out.resize(len);
    return out;
}

inline std::string ProtectKey(const std::string& rawKey) {
    std::vector<unsigned char> ks = KeyStream(rawKey.size());
    std::string enc(rawKey.size(), 0);
    for (size_t i = 0; i < rawKey.size(); ++i)
        enc[i] = (char)((unsigned char)rawKey[i] ^ ks[i]);
    return Base64Encode((const unsigned char*)enc.data(), enc.size());
}

inline std::string UnprotectKey(const std::string& stored) {
    std::string enc;
    if (!Base64Decode(stored, enc) || enc.empty()) return "";
    std::vector<unsigned char> ks = KeyStream(enc.size());
    std::string out(enc.size(), 0);
    for (size_t i = 0; i < enc.size(); ++i)
        out[i] = (char)((unsigned char)enc[i] ^ ks[i]);
    return NormalizeKey(out);
}

// Hasil terakhir yang tersimpan di memori proses (dipakai gate fitur
// tanpa harus baca file tiap frame).
struct CachedLicense {
    bool    valid        = false;
    int64_t expiredEpoch = 0;
    int64_t verifiedAt   = 0;
    std::string key;
};
inline CachedLicense& Cache() {
    static CachedLicense c;
    return c;
}

inline void ClearSession() {
    remove(SessionPath().c_str());
}

// Server menandai device lewat register_device(); kalau HWID berubah
// (restore APK / ganti device) key bisa kena limit. Lock file menandai
// HWID pertama yang sukses supaya device lain tidak ikut memakai session.
inline bool LockDeviceOnce() {
    EnsureDir(StorageDir());
    struct stat st;
    std::string h = DeviceHash();
    if (stat(SessionLockPath().c_str(), &st) == 0) {
        FILE* f = fopen(SessionLockPath().c_str(), "rb");
        if (f) {
            char buf[128] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            std::string saved(buf, n);
            if (Trim(saved) != h) return false;
            return true;
        }
    }
    FILE* f = fopen(SessionLockPath().c_str(), "wb");
    if (!f) return false;
    fwrite(h.data(), 1, h.size(), f);
    fclose(f);
    return true;
}

// ---------------------------- hasil validasi ----------------------------
enum class Reason {
    Ok,
    EmptyKey,
    BadKeyFormat,
    Network,
    RateLimited,
    InvalidRequest,
    InvalidKey,
    KeyExpired,
    KeyBanned,
    WrongTool,
    WrongVersion,
    DeviceLimit,
    SigningUnavailable,
    BadSignature,
    NotValid,
    NonceMismatch,
    HwidMismatch,
    ToolMismatch,
    PublicKeyMissing,
    PublicKeyInvalid,
    Unknown
};

struct Result {
    bool ok = false;
    Reason reason = Reason::Unknown;
    std::string message;        // pesan siap tampil
    std::string key;            // key ternormalisasi saat sukses
    std::string reseller;       // kosong kalau tidak disediakan server
    std::string tool;
    std::string version;
    std::string expiredAt;      // ISO-8601 dari server ("" kalau lifetime)
    int64_t expiredEpoch = 0;   // 0 = lifetime
    int maxDevice = 0;
    int currentDevices = 0;
    bool cached = false;        // true kalau hasil dari session offline
};

inline std::string ReasonMessage(Reason r, const std::string& extra = "") {
    switch (r) {
        case Reason::Ok:                 return "License verified";
        case Reason::EmptyKey:           return "Please enter your key";
        case Reason::BadKeyFormat:       return "Key format must be XLVP-XXXX-XXXX";
        case Reason::Network:            return "Cannot reach license server. Check your connection";
        case Reason::RateLimited:        return "Too many attempts. Wait a minute and try again";
        case Reason::InvalidRequest:     return "Request rejected by server (check tool settings)";
        case Reason::InvalidKey:         return "Invalid key";
        case Reason::KeyExpired:         return "This key has expired";
        case Reason::KeyBanned:          return "This key has been banned";
        case Reason::WrongTool:          return "Key is not for this tool";
        case Reason::WrongVersion:       return "Key does not match this tool version";
        case Reason::DeviceLimit:        return "Device limit reached. Ask admin to reset your HWID";
        case Reason::SigningUnavailable: return "Server signing is unavailable. Contact admin";
        case Reason::BadSignature:       return "License signature is invalid (response rejected)";
        case Reason::NotValid:           return "License payload is not valid";
        case Reason::NonceMismatch:      return "License payload replay detected";
        case Reason::HwidMismatch:       return "License was issued for another device";
        case Reason::ToolMismatch:       return "License payload is for another tool/version";
        case Reason::PublicKeyMissing:   return "Ed25519 public key is not configured in build";
        case Reason::PublicKeyInvalid:   return "Ed25519 public key in build is invalid";
        default:                         return extra.empty() ? "License check failed" : extra;
    }
}

inline Reason ReasonFromServerString(std::string s) {
    ToLowerInPlace(s);
    if (s == "invalid_request")      return Reason::InvalidRequest;
    if (s == "invalid_key")          return Reason::InvalidKey;
    if (s == "key_expired")          return Reason::KeyExpired;
    if (s == "key_banned")           return Reason::KeyBanned;
    if (s == "wrong_tool")           return Reason::WrongTool;
    if (s == "wrong_version")        return Reason::WrongVersion;
    if (s == "device_limit_exceeded")return Reason::DeviceLimit;
    if (s == "rate_limited")         return Reason::RateLimited;
    if (s == "signing_unavailable")  return Reason::SigningUnavailable;
    return Reason::Unknown;
}

// ISO-8601 "2025-12-31T00:00:00.000Z" -> epoch (UTC). 0 kalau tidak valid.
inline int64_t ParseIso8601Utc(const std::string& s) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
    static const int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int64_t days = 0;
    for (int yy = 1970; yy < y; ++yy) days += ((yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0) ? 366 : 365;
    bool leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
    for (int mm = 1; mm < mo; ++mm) days += mdays[mm] + ((mm == 2 && leap) ? 1 : 0);
    days += d - 1;
    return days * 86400LL + h * 3600LL + mi * 60LL + se;
}

inline std::string FormatEpoch(int64_t ts) {
    if (ts <= 0) return "Lifetime";
    time_t t = (time_t)ts;
    struct tm tm_info;
    gmtime_r(&t, &tm_info);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm_info);
    return std::string(buf);
}

// ---------------------------- HTTP ----------------------------
// Timeout pendek supaya UI tidak "nyangkut" saat server lambat.
inline bool PostJson(const std::string& url, const std::string& body, std::string& out, long& httpCode) {
    out.clear();
    httpCode = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, ("User-Agent: " + HttpUserAgent()).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

// ---------------------------- verifikasi ----------------------------
// Verifikasi penuh respons /api/validate. Semua pengecekan yang diwajibkan
// docs/CLIENT_INTEGRATION.md dilakukan di sini dalam satu tempat,
// lalu dipanggil ulang di titik-titik lain lewat RequireLicense().
inline Result VerifyServerResponseEx(const std::string& responseBody, const std::string& sentKey,
                                    const std::string& sentNonce, const std::string& sentHwid,
                                    const std::string& publicKeyB64,
                                    const std::string& expectedTool,
                                    const std::string& expectedVersion) {
    Result r;

    json body;
    try {
        body = json::parse(responseBody);
    } catch (...) {
        r.reason = Reason::Network;
        r.message = "License server returned an unreadable response";
        return r;
    }

    if (!body.is_object()) {
        r.reason = Reason::Network;
        r.message = "License server returned an unexpected response";
        return r;
    }

    if (body.contains("signature") && body.contains("payload") &&
        body["signature"].is_string() && body["payload"].is_string()) {
        // lanjut ke verifikasi
    } else {
        std::string reasonStr = JsonStr(body, "reason");
        r.reason = ReasonFromServerString(reasonStr);
        if (r.reason == Reason::Unknown) r.reason = Reason::Unknown;
        r.message = ReasonMessage(r.reason, reasonStr.empty() ? "License rejected" : reasonStr);
        return r;
    }

    unsigned char pubKey[32];
    if (!ParsePublicKey(publicKeyB64, pubKey)) {
        r.reason = Reason::PublicKeyInvalid;
        r.message = ReasonMessage(r.reason);
        Log("public key build tidak valid / kosong");
        return r;
    }

    std::string payloadBytes, signatureBytes;
    if (!Base64Decode(body["payload"].get<std::string>(), payloadBytes) ||
        !Base64Decode(body["signature"].get<std::string>(), signatureBytes)) {
        r.reason = Reason::BadSignature;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (signatureBytes.size() != 64) {
        r.reason = Reason::BadSignature;
        r.message = ReasonMessage(r.reason);
        return r;
    }

    // (4) verifikasi signature atas byte payload asli
    int verified = ed25519_verify((const unsigned char*)signatureBytes.data(),
                                  (const unsigned char*)payloadBytes.data(),
                                  payloadBytes.size(), pubKey);
    if (verified != 1) {
        r.reason = Reason::BadSignature;
        r.message = ReasonMessage(r.reason);
        Log("signature mismatch");
        return r;
    }

    json payload;
    try {
        payload = json::parse(payloadBytes);
    } catch (...) {
        r.reason = Reason::BadSignature;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (!payload.is_object()) {
        r.reason = Reason::NotValid;
        r.message = ReasonMessage(r.reason);
        return r;
    }

    // (5) payload harus terikat ke request ini
    if (!payload.contains("valid") || !payload["valid"].is_boolean() || payload["valid"].get<bool>() != true) {
        r.reason = Reason::NotValid;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (JsonStr(payload, "nonce") != sentNonce) {
        r.reason = Reason::NonceMismatch;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (JsonStr(payload, "hardware_id") != sentHwid) {
        r.reason = Reason::HwidMismatch;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (JsonStr(payload, "tool") != expectedTool ||
        JsonStr(payload, "version") != expectedVersion) {
        r.reason = Reason::ToolMismatch;
        r.message = ReasonMessage(r.reason);
        return r;
    }

    r.expiredAt = JsonStr(payload, "expired_at");
    r.expiredEpoch = ParseIso8601Utc(r.expiredAt);
    if (r.expiredEpoch > 0 && r.expiredEpoch <= (int64_t)time(nullptr)) {
        r.reason = Reason::KeyExpired;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (r.expiredAt.empty()) r.expiredEpoch = 0;

    r.maxDevice = (int)JsonInt(payload, "max_device");
    r.currentDevices = (int)JsonInt(payload, "current_devices");
    r.reseller = JsonStr(payload, "reseller");
    if (r.reseller.empty()) r.reseller = JsonStr(payload, "seller");
    r.tool = JsonStr(payload, "tool");
    r.version = JsonStr(payload, "version");
    r.key = NormalizeKey(sentKey);
    r.ok = true;
    r.reason = Reason::Ok;
    r.message = "License verified";
    return r;
}

inline Result VerifyServerResponse(const std::string& responseBody, const std::string& sentKey,
                                   const std::string& sentNonce, const std::string& sentHwid) {
    return VerifyServerResponseEx(responseBody, sentKey, sentNonce, sentHwid,
                                  XLOOP_PUBLIC_KEY_B64, XLOOP_TOOL_SLUG, XLOOP_TOOL_VERSION);
}

// Request + verifikasi (blocking, maksimal ~20 detik). Panggil di thread.
inline Result VerifyKey(const std::string& rawKey) {
    Result r;
    std::string key = NormalizeKey(rawKey);
    if (key.empty()) {
        r.reason = Reason::EmptyKey;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (!MatchesKeyRegex(key)) {
        r.reason = Reason::BadKeyFormat;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (Trim(XLOOP_PUBLIC_KEY_B64).empty()) {
        r.reason = Reason::PublicKeyMissing;
        r.message = ReasonMessage(r.reason);
        Log("XLOOP_PUBLIC_KEY_B64 belum diisi");
        return r;
    }

    std::string nonce = RandomNonce();
    std::string hwid  = HardwareId();

    json req;
    req["key"] = key;
    req["tool_slug"] = XLOOP_TOOL_SLUG;
    req["tool_version"] = XLOOP_TOOL_VERSION;
    req["hardware_id"] = hwid;
    req["nonce"] = nonce;

    std::string url = Trim(XLOOP_PANEL_URL);
    while (!url.empty() && url.back() == '/') url.pop_back();
    url += "/api/validate";

    std::string response;
    long httpCode = 0;
    if (!PostJson(url, req.dump(), response, httpCode)) {
        r.reason = Reason::Network;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (response.empty()) {
        r.reason = (httpCode == 429) ? Reason::RateLimited : Reason::Network;
        r.message = ReasonMessage(r.reason);
        return r;
    }
    if (httpCode == 429) {
        r.reason = Reason::RateLimited;
        r.message = ReasonMessage(r.reason);
        return r;
    }

    r = VerifyServerResponse(response, key, nonce, hwid);
    if (r.ok) {
        Log("license ok, expires=%s devices=%d/%d",
            r.expiredEpoch ? FormatEpoch(r.expiredEpoch).c_str() : "lifetime",
            r.currentDevices, r.maxDevice);
    } else {
        Log("license failed: %s", r.message.c_str());
    }
    return r;
}

// Validasi ringan untuk gate fitur: pakai session tersimpan, lalu
// verifikasi ulang ke server kalau sudah lewat interval.
inline bool SessionStillValid(const json& session, int refreshMinutes, bool& needNetwork) {
    needNetwork = false;
    if (!session.is_object() || session.empty()) return false;
    if (JsonStr(session, "hwid") != DeviceHash()) return false;

    int64_t exp = JsonInt(session, "expired_epoch");
    int64_t now = (int64_t)time(nullptr);
    if (exp > 0 && exp <= now) return false;

    int64_t verifiedAt = JsonInt(session, "verified_at");
    if (refreshMinutes > 0 && (now - verifiedAt) > (int64_t)refreshMinutes * 60) needNetwork = true;
    return true;
}

} // namespace xloop
