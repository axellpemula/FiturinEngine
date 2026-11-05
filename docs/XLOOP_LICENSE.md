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

## 6. HWID

`android:<sha512 hex 64 char>` dari gabungan:
`ANDROID_ID (Settings.Secure via JNI)` + MAC wifi + `ro.product.*` + boot id + info CPU stabil.
Hasilnya 16–128 karakter `[A-Za-z0-9_:.-]`, sesuai `HWID_REGEX` panel, dan tidak mengirim
identitas mentah ke server.

Kalau user ganti device / restore APK, HWID berubah → minta admin reset slot di
**HWID Manager** panel (`device_limit_exceeded`).

---

## 7. Tampilan kartu key

Kartu aktivasi mengikuti tema menu (dark glass + gold `#FFB800`):

- **Header**: logo, wordmark `FITURIN ENGINE`, badge status (`LOCKED` gold / `ACTIVE` hijau),
  garis aksen bergerak di tepi atas.
- **Isi**: judul + hint, label `LICENSE KEY` + keterangan `Format: XLVP-XXXX-XXXX`,
  satu baris `[field key] [ikon mata] [PASTE]`, kotak status (info / spinner + progress /
  sukses hijau / error merah), tombol utama `ACTIVATE LICENSE` → `CONTINUE TO MENU`,
  baris `DEVICE <hwid>` (`Expires: YYYY-MM-DD` saat aktif), footer `@FiturinEngine` +
  `Belum punya key? Chat owner`.
- Enter di keyboard = submit. Isi clipboard dinormalisasi dulu (`NormalizeKey`) supaya
  key dengan spasi, newline, huruf kecil, tanpa strip, atau tercampur teks lain tetap sah.
- Toast melayang untuk error/sukses; animasi fade + slide saat kartu muncul/hilang.

### Ketahanan di resolusi device

Semua ukuran (padding, gap, tinggi tombol/field, lebar kartu) diturunkan dari
`GetTextLineHeight()`, **bukan** dari konstanta 1080p, jadi tetap rapi saat font device
diperbesar (contoh nyata: 1565×810 dengan font ±2.4× lebih besar):

- lebar kartu = `max(isi terpanjang)` di-clamp ke `96%` lebar layar;
- kalau tinggi kartu > `94%` layar, jarak antar elemen dirapatkan otomatis
  (font tidak diperkecil);
- teks panjang (HWID, pesan server) dipotong dengan `...` lewat `FitText()`,
  jadi tidak ada baris yang menabrak elemen lain;
- `DEVICE` + `Expires` dipisah kiri/kanan dan saling memotong ruang, footer turun
  satu baris kalau layar sempit.

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
| `Key format must be XLVP-XXXX-XXXX` | format key salah | cek key dari reseller |
| `Invalid key` | key tidak ada di panel | cek ejaan / key salah tool |
| `This key has expired` | `expired_at` lewat | generate key baru |
| `This key has been banned` | di-ban admin / auto-ban device | hubungi owner |
| `Key is not for this tool` | `tool_slug` build ≠ panel | samakan `XLOOP_TOOL_SLUG` |
| `Key does not match this tool version` | `tool_version` beda | samakan `XLOOP_TOOL_VERSION` |
| `Device limit reached…` | slot HWID penuh | reset di HWID Manager |
| `Too many attempts…` | kena rate limit panel | tunggu 1 menit |
| `Cannot reach license server` | jaringan / URL salah | cek `XLOOP_PANEL_URL` |
| `License signature is invalid` | public key build ≠ keypair panel | salin ulang public key |
| `Ed25519 public key is not configured` | `XLOOP_PUBLIC_KEY_B64` kosong | isi dulu |
| `Request rejected by server (…invalid_request)` | body ditolak panel (mis. slug/version kosong) | cek `tool_slug`/`tool_version` |

---

## 10. Catatan build

Verifier Ed25519 berasal dari implementasi portabel ref10 (lisensi zlib, lihat
`include/external/ed25519/LICENSE.txt`). File yang dibangun di `Android.mk`:
`fe.c`, `ge.c`, `sc.c`, `sha512.c`, `verify.c` (tanpa `sign.c`/`keypair.c` — client tidak perlu
tanda tangan). Verifikasi satu request ± 1–2 ms di ARM64, jadi aman dipanggil dari thread UI.

Log tersedia dengan tag `XLOOP` (`adb logcat -s XLOOP`):
`license ok, expires=… devices=1/2`, `license failed: …`, `license gate blocked: esp`.
