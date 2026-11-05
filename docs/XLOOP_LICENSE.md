# Lisensi XLOOP PANEL di FiturinEngine

Sistem key lama (`mod/keylogin.h`, endpoint kosong `https://` + cek `"success":true` polos) diganti
total dengan validasi resmi **XLOOP PANEL** — `POST /api/validate` yang response-nya ditandatangani
**Ed25519**. Response yang tidak lolos verifikasi tanda tangan dianggap gagal, jadi tidak bisa
dipalsukan atau diputar ulang.

Referensi: `axellpemula/XLOOP-PANEL` → `app/api/validate/route.ts`,
`lib/utils/constants.ts`, `lib/utils/signing.ts`, dan `docs/CLIENT_INTEGRATION.md`.

---

## 1. Yang berubah

| File | Status | Keterangan |
|---|---|---|
| `jni/mod/xloop/license_core.h` | **baru** | HTTP + verifikasi Ed25519 + HWID + session (tanpa ImGui) |
| `jni/mod/xloop/auth_ui.h` | **baru** | state auth, gate fitur, UI kartu key bertema menu |
| `jni/include/external/ed25519/*` | **baru** | verifier Ed25519 (ref10, lisensi zlib) — 5 file `.c` |
| `jni/mod/keylogin.h` | **dihapus** | sistem login lama |
| `jni/main.cpp` | diubah | `Login()` sekarang memakai `xloop::VerifyKey()`; `ANDROID_ID` disuntik ke HWID |
| `jni/menu.h` | diubah | include auth baru, gate fitur, Info tab lisensi, kartu login paling depan |
| `jni/game/inc/AutoQueue.h` | diubah | `StartMatch` diblok tanpa lisensi |
| `jni/Android.mk` | diubah | menambah 5 source Ed25519 |

---

## 2. Konfigurasi (sudah diisi untuk panel produksi)

Semua di bagian atas `jni/mod/xloop/license_core.h`:

```cpp
#define XLOOP_PANEL_URL     "https://xlooppanelid.vercel.app"
#define XLOOP_TOOL_SLUG     "fiturin8bp"
#define XLOOP_TOOL_VERSION  "1.0.0"
#define XLOOP_PUBLIC_KEY_B64 \
    "-----BEGIN PUBLIC KEY-----\n" \
    "MCowBQYDK2VwAyEAKgIC1sJK8ls/kT0dYZTpJkHb6Imjp0knnY/mn6FkOb4=\n" \
    "-----END PUBLIC KEY-----\n"
```

Nilai `XLOOP_PUBLIC_KEY_B64` juga menerima **base64 32 byte mentah** atau **hex 64 karakter**
(kalau lebih suka format ringkas). Ketiga format diurai oleh `ParsePublicKey()`; key panel di atas
sudah diverifikasi ter-parse menjadi 32 byte dan cocok dengan slug `fiturin8bp`.

Kalau panel pindah domain / ganti tool / ganti keypair, cukup ubah 4 nilai ini — tidak ada
tempat lain yang perlu disentuh. **Ganti keypair di panel = semua build lama harus di-update
public key-nya.**

## 3. Alur validasi (sama persis dengan docs panel)

1. Client membuat **nonce baru** tiap request (`/dev/urandom`, 24 byte → base64url 32 char).
2. `POST {PANEL}/api/validate` dengan body:

   ```json
   {
     "key": "XLVP-AB12-CD34",
     "tool_slug": "fiturin8bp",
     "tool_version": "1.0.0",
     "hardware_id": "android:<sha512-64-hex>",
     "nonce": "<32 char>"
   }
   ```

3. Response sukses `{ "payload": "<base64>", "signature": "<base64>" }`.
   Selain bentuk itu (mis. `{"valid":false,"reason":"key_banned"}`) → langsung gagal.
4. **Verifikasi Ed25519 atas byte payload** (base64 di-decode dulu; payload tidak pernah di-JSON ulang
   sebelum verifikasi, karena urutan key/whitespace bisa berubah dan signature jadi tidak valid).
5. Cek isi payload: `valid === true`, `nonce` sama dengan yang dikirim, `hardware_id` sama,
   `tool`/`version` cocok, dan `expired_at` masih berlaku.
6. `secret_payload` hanya disimpan di memori proses (tidak ditulis ke disk, tidak di-log),
   bisa dibaca lewat `xloop::SecretPayload()` untuk bagian tool yang butuh config/kunci panel.

Semua langkah di atas ada di `xloop::VerifyServerResponseEx()` / `xloop::VerifyKey()`, lalu dipakai
ulang oleh gate fitur.

---

## 4. Session lokal (biar tidak login tiap buka game)

- Lokasi: `/data/user_de/0/<package>/no_backup/xloop/license.dat`
- Isi: `hwid` hash, `key_protected` (key di-XOR dengan keystream dari device hash, lalu base64),
  `key_hash`, `tool`, `version`, `expired_epoch`, `max_device`, `current_devices`, `verified_at`.
- **Key mentah tidak pernah ditulis plaintext** ke file.
- `license.lock` menyimpan hash device pertama yang sukses aktivasi; device lain yang menyalin folder
  session akan ditolak (`This license is already activated on another device`).
- Validasi ulang ke server otomatis **tiap 10 menit** (dengan cooldown 45 detik supaya tidak
  menabrak rate limit panel: 30 req/menit per IP, 15/menit per key).
- Kalau server tidak terjangkau, session cache masih dipakai maksimal **90 menit**
  (mode offline), setelah itu minta aktivasi ulang.

> Catatan jujur: obfuscation key di atas bukan enkripsi kuat. Session file tetap bisa dipalsukan
> oleh orang yang paham reverse engineering. Yang membuat bypass mahal adalah tanda tangan Ed25519
> (tidak bisa dipalsukan tanpa private key panel) dan pengecekan yang tersebar di beberapa titik.

---

## 5. Gate fitur (sengaja tersebar, bukan satu `isValid()`)

`xloop::RequireLicense("<titik>")` dipanggil di:

| Titik | Efek kalau lisensi tidak valid |
|---|---|
| `DrawESP()` (`"esp"`) | ESP / prediction line / pocket state tidak digambar, menu ditutup |
| `AutoPlay::Update()` (`"autoplay"`) | auto play & auto aim dipaksa OFF |
| Toggle Auto Play / Auto Aim (`"toggle_*"`) | toggle ditolak dan kembali OFF |
| `StartMatch()` (`"start_match"`) | match tidak dimulai (auto queue diam) |

UI juga dikunci: selama `IsLoggedIn() == false`, floating button + menu tidak digambar dan
`g_menu.isOpen` dipaksa `false`, jadi hanya kartu aktivasi yang tampil.

---

## 5b. Jaringan (kalau muncul "Server belum bisa dihubungi")

Kartu hanya menampilkan pesan ramah. Penyebab sebenarnya ada di log:

```bash
adb logcat -s XLOOP
```

Baris yang dikeluarkan saat validasi:

```
validate -> https://xlooppanelid.vercel.app/api/validate (tool=fiturin8bp v=1.0.0)
http transport gagal: url=... curl=7(7) Couldn't connect to server      <- tidak sampai server
diag hasil: stage=connect|tls|http curl=<n> <pesan>
license failed: ... (http=200 body={"error":"invalid_key"})              <- server menjawab, key salah
```

Arti `curl` yang paling sering:

| curl | Arti | Tindakan |
|---|---|---|
| 6 | DNS gagal (tidak ada internet / DNS diblokir) | cek koneksi device |
| 7 / 28 | tidak bisa connect / timeout | coba jaringan lain (ISP/DNS bisa memblokir) |
| 60 / 77 / 83 | TLS: sertifikat/CA tidak bisa diverifikasi | lihat catatan CA di bawah |
| 0 | request tidak pernah jalan (`curl_easy_init` gagal) | `libcurl.a` tidak ter-load |

Fitur diagnosa `RunDiagnostics()` mengirim satu POST kosong ke endpoint yang sama supaya
`stage` jelas: `connect` (belum sampai server), `tls` (sertifikat), atau `http` (server
menjawab, berarti masalahnya bukan jaringan).

**Catatan CA (penting).** Build ini memakai `libcurl 7.51.0` + `OpenSSL 1.1.0c` statis yang
tidak membawa CA bundle. Pada Android modern (7+) `/etc/ssl/certs` tidak lengkap, sehingga
verifikasi TLS bisa gagal (`curl 60`) walau URL benar dan device online. `PostJson()`
sekarang memakai urutan: `CURL_CA_BUNDLE` / `SSL_CERT_FILE` (env) →
`/system/etc/security/cacerts` (direktori CA sistem Android) → default libcurl.

Kalau di device tertentu tetap `curl 60`, cara paling aman: taruh `cacert.pem` (bundle
Mozilla) di assets, salin ke `SessionDir()/cacert.pem` saat boot, lalu set
`CURL_CA_BUNDLE` ke path itu.

**Nonce.** Nonce dibuat dari `/dev/urandom` → base64url. Supaya hasil acak tidak pernah
kebetulan menyerupai hostname (mis. mengandung titik seperti `172-xxx`), nonce yang
"kotor" otomatis diganti bentuk hex 48 karakter (tetap lolos `^[A-Za-z0-9_-]{16,64}$`).

**Session.** `StorageDir()` sekarang memotong NUL pada `/proc/self/cmdline`. Sebelumnya
nama paket terbawa byte NUL sehingga direktori session tidak valid dan key harus ditempel
ulang setiap kali game dibuka.

---

## 6. HWID

`android:<sha512 hex 64 char>` dari gabungan:
`ANDROID_ID (Settings.Secure via JNI)` + MAC wifi + `ro.product.*` + boot id + info CPU stabil.
Hasilnya 16–128 karakter `[A-Za-z0-9_:.-]`, sesuai `HWID_REGEX` panel, dan tidak mengirim
identitas mentah ke server.

Kalau user ganti device / restore APK, HWID berubah → minta admin reset slot di
**HWID Manager** panel (`device_limit_exceeded`).

---

## 7. Tampilan kartu key

Kartu aktivasi sengaja dibuat **sesimpel mungkin** (tema dark glass + gold `#FFB800`):

```
Masuk dulu yuk                    <- judul
Tempel key kamu di bawah ini      <- satu kalimat bantuan
[ Tempel key di sini ] [mata] [PASTE]
(status singkat, hanya kalau ada: "Mengecek key...", "Key ini sudah diblokir", ...)
[            Masuk              ]
@FiturinEngine        Butuh key? Chat owner
```

- **Tidak ada istilah teknis di layar**: tanpa `XLOOP PANEL`, `Ed25519`, `HWID`,
  `format XLVP-XXXX-XXXX`, nama server, atau kode error mentah.
- Pesan server dipetakan ke bahasa biasa lewat `FriendlyMessage()` di `auth_ui.h`
  (`license_core.h` tetap menyimpan pesan teknis untuk log/`adb logcat`).
- Baris status hanya muncul kalau memang ada sesuatu (sedang mengecek / gagal / masa aktif).
  Saat aktif isinya `Aktif sampai 31-12-2026` (tanpa jam/WM).
- Enter di keyboard = submit. Isi clipboard dinormalisasi dulu (`NormalizeKey`) supaya key
  dengan spasi, newline, huruf kecil, tanpa strip, atau tercampur teks lain tetap sah.
- Tombol `Masuk` berubah jadi `Lanjut` saat key sudah aktif; toast melayang untuk error/sukses.

### Ketahanan di resolusi device

Semua ukuran (padding, gap, tinggi tombol/field, lebar kartu) diturunkan dari
`GetTextLineHeight()`, **bukan** dari konstanta 1080p, jadi tetap rapi saat font device
diperbesar (contoh nyata: 1565×810 dengan font ±2.4× lebih besar):

- lebar kartu = `max(isi terpanjang)` di-clamp ke `96%` lebar layar;
- kalau tinggi kartu > `94%` layar, jarak antar elemen dirapatkan otomatis
  (font tidak diperkecil);
- teks panjang dipotong dengan `...` lewat `FitText()`, jadi tidak ada baris yang
  menabrak elemen lain;
- footer (brand + tombol bantuan) turun satu baris kalau layar sempit.

### Kenapa tombol dulu tidak bisa ditekan (dan perbaikannya)

Versi sebelumnya memakai pola khas desktop: `hovered && io.MouseReleased[0]` di atas
`ButtonBehavior()`. Di layar sentuh pola ini rapuh:

1. kalau jari bergeser sedikit sebelum diangkat, item tidak lagi `hovered` saat UP →
   tombol tidak pernah terpicu;
2. ImGui juga membatalkan klik bila pointer bergerak melewati batas drag, sehingga
   semua tombol di kartu terasa "mati" (PASTE, tombol mata, bahkan `Masuk`);
3. field key memakai `InputText`, yang butuh keyboard virtual — tidak pernah muncul di
   overlay game, jadi field-nya juga tidak bisa dipakai.

Perbaikan (semuanya di `auth_ui.h`, tanpa mengubah `menu.h`):

- **Jalur tap sendiri**: hook sentuhan engine (`include/input.h`) memanggil
  `xloop::OnRawTouch(down, x, y)` langsung dari `nativeTouchesBegin/End`, lalu
  `TapInRect()` memutuskan tombol mana yang kena. Tidak bergantung pada state
  mouse/klik/fokus ImGui sama sekali.
- **Toleransi tap**: DOWN harus mendarat di tombol, UP boleh meleset sampai ±0.85 baris
  teks — jadi tap tetap terasa responsif di HP. Satu tap hanya memicu satu tombol.
- **Field key digambar manual** (bukan `InputText`): tap untuk memilih field, teks dari
  clipboard langsung tampil; keyboard fisik tetap didukung lewat input queue ImGui.
  Tidak ada lagi ketergantungan pada keyboard virtual.
- `Masuk` selalu bisa ditekan: kalau key masih kosong hasilnya pesan
  `Key kamu masih kosong` (bukan tombol mati).

### Tombol PASTE

Backend `imgui_impl_android` **tidak** menyediakan clipboard (`GetClipboardTextFn` tidak
pernah diisi), jadi `ImGui::GetClipboardText()` selalu kosong. Karena itu kartu memakai
provider sendiri:

```cpp
// main.cpp, sekali saat boot
xloop::SetClipboardProvider([]() -> std::string {
    JNIEnv* env = GetJNIEnv();
    return env ? getClipboard(env) : std::string();   // ClipboardManager via JNI
});
```

`xloop::ReadClipboard()` memakai provider itu lebih dulu, lalu fallback ke
`ImGui::GetClipboardText()` (berguna untuk harness desktop).

### Catatan window `Debug##Default`

ImGui membuat fallback window `Debug##Default` di `NewFrame()` untuk mencegah crash
kalau ada widget dipanggil di luar `Begin()/End()`. Window ini **tidak** memakai flag
`IsFallbackWindow` di jalur hit-test + render, sehingga:

1. muncul sebagai kotak "Debug" kosong di kiri atas, dan
2. ikut mengonsumsi tap sehingga tombol di kartu (mis. PASTE) tidak menerima sentuhan.

Perbaikan di `include/external/imgui/src/imgui.cpp`:

- `ImGui::Render()` melewati (`continue`) semua `IsFallbackWindow`;
- `EndFrame()` selalu menonaktifkan fallback window (`Active = false`, `Hidden = true`)
  walau ada kode yang menyentuhnya lewat `GetWindowDrawList()` setelah `End()` terakhir;
- `IsPointOverAnyWindow()` juga melewati fallback window.

Pratinjau render (headless ImGui, 1565×810, font diperbesar 2.4×) ada di `docs/preview/`:
`login-idle.png`, `login-pasted.png`, `login-checking.png`, `login-error.png`,
`login-banned.png`, `login-active.png`.

---

## 8. Uji cepat tanpa device

Signature dari panel bisa disimulasikan dengan `node:crypto` (format signature ed25519 Node
kompatibel dengan panel):

```js
const { generateKeyPairSync, sign } = require("crypto");
const { publicKey, privateKey } = generateKeyPairSync("ed25519");
const raw = publicKey.export({ type: "spki", format: "der" }).subarray(12); // 32 byte

const payload = JSON.stringify({
  valid: true, tool: "fiturin8bp", version: "1.0.0",
  expired_at: "2030-01-01T00:00:00.000Z", max_device: 2, current_devices: 1,
  hardware_id: "<HWID dari baris DEVICE>", nonce: "<nonce yang dikirim>",
  server_time: new Date().toISOString(), secret_payload: null
});

console.log("public key (base64 32B):", raw.toString("base64"));
console.log("payload:", Buffer.from(payload).toString("base64"));
console.log("signature:", sign(null, Buffer.from(payload), privateKey).toString("base64"));
```

Kasus yang sudah diverifikasi lolos/gagal sesuai harapan: signature valid, signature salah,
payload diubah setelah signing, nonce berbeda (replay), HWID berbeda, tool/version beda,
key expired, key banned, public key kosong/rusak, public key format PEM/hex.

---

## 9. Troubleshooting

| Pesan di kartu | Arti | Tindakan |
|---|---|---|
| `Key kamu masih kosong` | field belum diisi | tempel/ketik key |
| `Format key-nya belum pas` | format key salah | cek key dari reseller |
| `Key-nya nggak dikenali` | key tidak ada di panel | cek ejaan / key salah tool |
| `Key kamu sudah habis` | `expired_at` lewat | generate key baru |
| `Key ini sudah diblokir` | di-ban admin / auto-ban device | hubungi owner |
| `Key ini bukan untuk versi ini` | `tool_slug`/`tool_version` build ≠ panel | samakan di `license_core.h` |
| `Key sudah dipakai di device lain` | slot HWID penuh | reset di HWID Manager |
| `Key ini terikat di device lain` | payload `hardware_id` ≠ device ini | reset slot / pakai device asal |
| `Terlalu banyak percobaan, tunggu sebentar` | kena rate limit panel | tunggu 1 menit |
| `Koneksi bermasalah, coba lagi ya` | jaringan / URL salah | cek `XLOOP_PANEL_URL` + koneksi |
| `Belum berhasil, coba lagi ya` | signature/payload ditolak (public key build ≠ keypair panel) | salin ulang public key |
| `Fitur belum siap, hubungi admin` | `XLOOP_PUBLIC_KEY_B64` kosong/rusak | isi dulu |

> Pesan teknis detail (mis. `signature invalid`, `replay detected`, `invalid_request`) tetap
> ditulis ke log `XLOOP` (`adb logcat -s XLOOP`) untuk diagnosis, tapi **tidak** muncul di kartu.

---

## 10. Catatan build

Verifier Ed25519 berasal dari implementasi portabel ref10 (lisensi zlib, lihat
`include/external/ed25519/LICENSE.txt`). File yang dibangun di `Android.mk`:
`fe.c`, `ge.c`, `sc.c`, `sha512.c`, `verify.c` (tanpa `sign.c`/`keypair.c` — client tidak perlu
tanda tangan). Verifikasi satu request ± 1–2 ms di ARM64, jadi aman dipanggil dari thread UI.

Log tersedia dengan tag `XLOOP` (`adb logcat -s XLOOP`):
`license ok, expires=… devices=1/2`, `license failed: …`, `license gate blocked: esp`.
