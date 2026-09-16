# sysdiag — Windows Sistem Tanılama Aracı (CLI)

[![CI](https://github.com/VertexSoftwareDev/sysdiag/actions/workflows/ci.yml/badge.svg)](https://github.com/VertexSoftwareDev/sysdiag/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

[English](README.md) | **Türkçe**

`sysdiag`, bir Windows bilgisayarının durumunu gösteren, harici bağımlılığı olmayan küçük bir
komut satırı aracıdır. İşletim sistemi, işlemci, bellek, diskler, ekran kartları ve ağ
adaptörleri hakkında bilgi verir. İnsanlar için okunabilir bir rapor, script'ler için de
yapısı sabit bir JSON çıktısı üretir.

Tüm değerler çalışma anında işletim sisteminden okunur; hiçbir değer koda gömülü değildir.
Bir değer okunamazsa rapor bunu nedeniyle birlikte belirtir ve diğer tüm değerler yine
gösterilir.

```text
> sysdiag
sysdiag 1.0.0 - report generated 2026-01-02T03:04:05Z

Operating System
  Product               Windows 11 Pro
  Version               23H2 (build 22631.3880)
  Edition               Professional
  Kernel version        10.0
  Architecture          x64
  Computer name         DEV-PC
  Uptime                1d 2h 3m

CPU
  Name                  AMD Ryzen 7 5800X 8-Core Processor
  Vendor                AuthenticAMD
  Physical cores        8
  Logical processors    16
  Base frequency        3800 MHz
  Usage                 12.3% (sampled over 500 ms)

Memory
  Total                 32.0 GiB
  Used                  12.0 GiB (37.5%)
  Available             20.0 GiB

Disks
  C:\ [fixed, NTFS] "Windows"
    Capacity            500.0 GiB
    Used                375.0 GiB (75.0%)
    Free                125.0 GiB
  D:\ [optical] - not ready (no media or unavailable)

GPU
  [0] NVIDIA GeForce RTX 3070
    Vendor              NVIDIA
    PCI id              0x10DE:0x2484
    Dedicated VRAM      8.0 GiB
    Shared sys memory   16.0 GiB

Network
  Ethernet (Intel(R) Ethernet Connection)
    Type                ethernet
    MAC address         00-1A-2B-3C-4D-5E
    IPv4                192.168.1.10/24
    IPv6                fe80::1/64
    Gateway             192.168.1.1
    Link speed          1 Gbps
```
<sub>Yalnızca örnek düzendir. Gösterilen değerler formatlayıcının test verisinden gelir,
gerçek bir makineden alınmamıştır.</sub>

## İçindekiler

- [Özellikler](#özellikler)
- [Gereksinimler](#gereksinimler)
- [Derleme](#derleme)
- [Kullanım](#kullanım)
- [Hızlı test](#hızlı-test)
- [JSON çıktısı](#json-çıktısı)
- [Mimari](#mimari)
- [Hata yönetimi](#hata-yönetimi)
- [Testler](#testler)
- [Bağımlılıklar](#bağımlılıklar)
- [Bilinen sınırlamalar](#bilinen-sınırlamalar)
- [Olası geliştirmeler](#olası-geliştirmeler)
- [Lisans](#lisans)

## Özellikler

| Bölüm     | Bilgi | Kaynak |
|-----------|-------|--------|
| `os`      | Ürün adı, sürüm (ör. 23H2), edisyon, build + UBR, çekirdek sürümü, işletim sistemi ve süreç mimarisi, bilgisayar adı, açık kalma süresi | `RtlGetVersion`, registry, `IsWow64Process2`, `GetComputerNameExW`, `GetTickCount64` |
| `cpu`     | Model adı, üretici, soket, fiziksel çekirdek, mantıksal işlemci sayısı, temel frekans, kullanım yüzdesi | registry / `CPUID`, `GetLogicalProcessorInformationEx`, `GetActiveProcessorCount`, `GetSystemTimes` |
| `memory`  | Toplam, kullanılan ve kullanılabilir fiziksel bellek, kullanım yüzdesi | `GlobalMemoryStatusEx` |
| `disk`    | Sürücü harfi olan her birim: tür, etiket, dosya sistemi, kapasite, kullanılan, boş alan, kullanım yüzdesi | `GetLogicalDriveStringsW`, `GetDiskFreeSpaceExW`, `GetVolumeInformationW` |
| `gpu`     | Her donanım ekran kartı: ad, üretici, PCI kimlikleri, ayrılmış VRAM, ayrılmış/paylaşılan sistem belleği | DXGI (`IDXGIFactory1::EnumAdapters1`) |
| `network` | Açık (up) adaptörler: ad, açıklama, tür, MAC, IPv4/IPv6 (CIDR), ağ geçitleri, bağlantı hızı | `GetAdaptersAddresses` |

Diğer davranışlar:

- **İki çıktı biçimi.** Varsayılan olarak okunabilir metin, script'ler için `--json` ile JSON.
- **İstediğin bölümleri seç.** Herhangi bir bölüm kombinasyonu seçilebilir, ör. `sysdiag cpu memory`.
- **Kısmi sonuçlar.** Her bölüm kendi hatalarını kaydeder; bir bölümün başarısız olması diğerlerini durdurmaz.
- **Doğru Unicode desteği.** Argümanlar `wmain` ile UTF-16 olarak okunur. Konsol çıktısı `WriteConsoleW` ile yazılır, dosyaya yönlendirilen çıktı ise UTF-8'dir.
- **Terminal güvenliği.** İşletim sisteminden veya kullanıcıdan gelen metin terminale yazılmadan önce temizlenir; böylece bir cihaz adındaki kontrol veya kaçış dizileri terminalini etkileyemez.

## Gereksinimler

- Windows 10 veya Windows 11 (x64; ARM64'te çalışması beklenir ancak CI'da test edilmez)
- CMake ≥ 3.21
- C++20 destekli bir derleyici:
  - *Desktop development with C++* iş yükü kurulu Visual Studio 2022 veya daha yenisi (MSVC), **ya da**
  - MinGW-w64 GCC ≥ 13 / LLVM-MinGW (garanti edilmez)
- Yapılandırma sırasında internet bağlantısı, **yalnızca** testleri derliyorsan ve GoogleTest
  kurulu değilse gerekir (bkz. [Bağımlılıklar](#bağımlılıklar))

## Derleme

### Hazır derlenmiş program

Her [GitHub Release](https://github.com/VertexSoftwareDev/sysdiag/releases/latest) sürümüne
çalışmaya hazır bir `sysdiag.exe` (Windows x64) eklenir. Zip dosyasını indir, klasöre çıkar ve
`sysdiag.exe`'yi bir terminalden çalıştır. Sürümler, `v*` etiketi gönderildiğinde
`.github/workflows/release.yml` tarafından derlenir ve test edilir.

### Visual Studio 2022 veya daha yenisi (önerilen)

```powershell
git clone https://github.com/VertexSoftwareDev/sysdiag.git
cd sysdiag

cmake --preset msvc                       # yapılandır (kurulu en yeni Visual Studio, x64)
cmake --build --preset msvc-release       # veya msvc-debug
ctest --preset msvc-release               # birim testlerini çalıştır

.\build\msvc\Release\sysdiag.exe
```

`cmake` komutu tanınmıyorsa bu komutları **Developer PowerShell for VS** içinde çalıştır
(Visual Studio kendi CMake'ini orada PATH'e ekler). Klasörü Visual Studio'da
**Dosya → Aç → Klasör** ile de açabilirsin; Visual Studio `CMakePresets.json` dosyasını
otomatik olarak algılar.

### Ninja (Developer PowerShell / Developer Command Prompt)

```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release
.\build\ninja-release\sysdiag.exe
```

### CMake seçenekleri

| Seçenek | Varsayılan | Açıklama |
|---------|------------|----------|
| `SYSDIAG_BUILD_TESTS` | `ON` (ana proje ise) | Birim testlerini derle |
| `SYSDIAG_WARNINGS_AS_ERRORS` | `OFF` | Uyarıları hata say (CI'da açık) |
| `SYSDIAG_ENABLE_SANITIZERS` | `OFF` | GCC/Clang derlemelerinde ASan + UBSan |

Testler olmadan, internet bağlantısı gerektirmeden yalnızca programı derlemek için
`-DSYSDIAG_BUILD_TESTS=OFF` ver. Kurmak için
`cmake --install build/msvc --config Release --prefix <klasör>` kullan.

## Kullanım

```text
sysdiag [BÖLÜM...] [--json] [--sample-ms <ms>]
sysdiag --help | --version
```

| Argüman | Anlamı |
|---------|--------|
| *(yok)* / `all` | Tüm bölümleri göster |
| `os` | İşletim sistemi |
| `cpu` | İşlemci |
| `memory`, `ram` | Fiziksel bellek |
| `disk` | Birimler |
| `gpu` | Ekran kartları |
| `network`, `net` | Ağ adaptörleri |
| `--json` | Metin yerine JSON üret |
| `--sample-ms <ms>` / `--sample-ms=<ms>` | CPU ölçüm süresi, 100–5000 ms (varsayılan 500) |
| `-h`, `--help`, `/?` | Yardımı göster |
| `-V`, `--version` | Sürümü göster |

Bölüm adlarında büyük/küçük harf fark etmez ve birden fazla bölüm birlikte verilebilir.
Bölümler, hangi sırayla verildiklerinden bağımsız olarak her zaman yukarıdaki sabit sırayla
yazdırılır. Komut satırında herhangi bir yerde `--help` veya `--version` varsa, diğer
argümanlardan önce o işlenir.

```powershell
sysdiag                          # tam rapor
sysdiag cpu ram                  # yalnızca işlemci ve bellek
sysdiag disk --json              # diskler JSON olarak
sysdiag cpu --sample-ms 2000     # 2 saniyelik, daha dengeli CPU ölçümü
sysdiag --json > rapor.json      # anlık görüntüyü kaydet

# PowerShell: %90'dan fazla dolu birimleri listele
(sysdiag disk --json | ConvertFrom-Json).disk.volumes |
    Where-Object { $_.usage_percent -gt 90 } |
    Select-Object root, usage_percent
```

### Çıkış kodları

| Kod | Anlamı |
|-----|--------|
| `0` | Rapor üretildi. Bazı değerler yine de alınamamış olabilir; bölümlerdeki `errors` listesine bak. |
| `1` | Beklenmeyen iç hata veya çıktı yazılamadı (ör. pipe kapandı) |
| `2` | Geçersiz komut satırı; stderr'e bir hata mesajı yazılır |

```text
> sysdiag cpux
sysdiag: error: unknown section 'cpux' (valid: all, os, cpu, memory|ram, disk, gpu, network|net)
Run 'sysdiag --help' for usage.
```

Programın kendi mesajları İngilizcedir; Windows'tan gelen sistem hata mesajları ise işletim
sisteminin dilinde görünür.

## Hızlı test

Derlemeden sonra her şeyi uçtan uca kontrol etmenin en hızlı yolu smoke-test script'idir.
Gerçek programı 20'yi aşkın farklı şekilde çalıştırır ve her kontrol için bir `PASS`/`FAIL`
satırı yazar. Kontrol ettikleri:

- Çıkış kodları
- Metin çıktısındaki bölümler
- JSON'un geçerliliği ve değerlerin mantıklı olması
- Bölüm seçimi
- Dosyaya yönlendirilen çıktı
- Hatalı girişlerin reddedilmesi

```powershell
# Proje klasöründen çalıştır. -ExecutionPolicy Bypass yalnızca Windows script'leri varsayılan
# olarak engellediği için gerekli (sadece bu çalıştırma için geçerlidir).
powershell -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1

# Başka bir derlemeyi test et
powershell -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -Exe .\build\msvc\Debug\sysdiag.exe
```

Beklenen sonuç: son satırda `All 21 checks passed.` yazar ve script 0 koduyla çıkar.

Elle denemek için (PowerShell, proje klasöründen):

```powershell
Set-Alias sysdiag "$PWD\build\msvc\Release\sysdiag.exe"   # bu oturum için kısayol

# Normal kullanım - çıkış kodu 0 olmalı
sysdiag                               # tam rapor
sysdiag cpu ram                       # seçili bölümler (takma adlar çalışır)
sysdiag GPU Net                       # büyük/küçük harf fark etmez
sysdiag cpu --sample-ms 2000          # daha uzun CPU ölçümü
sysdiag --version; $LASTEXITCODE

# JSON
$r = sysdiag --json | ConvertFrom-Json
$r.cpu.usage_percent
$r.memory | Format-List
$r.gpu.adapters | Select-Object name, vendor, dedicated_video_memory_bytes
$r.disk.volumes | Where-Object { $_.usage_percent -gt 90 } | Select-Object root, usage_percent
sysdiag memory disk --json > rapor.json

# Hatalı kullanım - her biri "sysdiag: error: ..." yazmalı ve çıkış kodu 2 olmalı
sysdiag cpux;            $LASTEXITCODE
sysdiag --verbose;       $LASTEXITCODE
sysdiag --sample-ms 1;   $LASTEXITCODE
sysdiag --sample-ms abc; $LASTEXITCODE
sysdiag --json --json;   $LASTEXITCODE

# --help her zaman önceliklidir - çıkış kodu 0
sysdiag saçma --help;    $LASTEXITCODE
```

CPU ölçümünün yüke tepki verdiğini görmek için bir pencerede şunu çalıştır:

```powershell
1..4 | ForEach-Object { Start-Job { while ($true) {} } }   # meşgul döngüler başlat
```

Ardından başka bir pencerede `sysdiag cpu` çalıştır; kullanım yüzdesi belirgin şekilde
artmalı. İşin bitince döngüleri `Get-Job | Stop-Job; Get-Job | Remove-Job` ile durdur.

Birim testleri:

```powershell
ctest --preset msvc-release                    # tüm testler
ctest --preset msvc-release -R Cli             # yalnızca adında "Cli" geçen testler
.\build\msvc\tests\Release\sysdiag_tests.exe --gtest_filter=Units.*
```

## JSON çıktısı

JSON belgesi aşağıdaki kurallara uyar; bu sayede script'ler yapısına güvenebilir:

- **Sabit anahtar sırası.** Anahtarlar her zaman aynı sırada gelir ve çıktı geçerli RFC 8259 JSON'dur (UTF-8, girintili).
- **İstenmeyen bölümler.** İstemediğin bölümler çıktıda hiç yer almaz.
- **Eksik değerler `null`.** Alınamayan bir değer `null` olur; asla `0` veya `""` yazılmaz.
- **Ham birimler.** Boyutlar **byte** cinsinden tam sayıdır, hızlar **bit/saniye**, açık kalma süresi **saniye** cinsindendir.
- **Yüzdeler** 0 ile 100 arasında, bir ondalık basamağa yuvarlanmış sayılardır.
- **Hata listeleri.** Her bölümde `{operation, code, message}` nesnelerinden oluşan bir `errors` dizisi vardır. `code`, Win32 hata kodu veya HRESULT'tır; kod yoksa `null` olur.
- **Şema sürümü.** Biçim uyumsuz şekilde değiştiğinde `schema_version` artırılır.

<details>
<summary>Örnek (<code>sysdiag --json</code>)</summary>

```json
{
  "schema_version": 1,
  "tool": {
    "name": "sysdiag",
    "version": "1.0.0"
  },
  "generated_at": "2026-01-02T03:04:05Z",
  "os": {
    "product_name": "Windows 11 Pro",
    "display_version": "23H2",
    "edition": "Professional",
    "major_version": 10,
    "minor_version": 0,
    "build_number": 22631,
    "update_revision": 3880,
    "os_architecture": "x64",
    "process_architecture": "x64",
    "computer_name": "DEV-PC",
    "uptime_seconds": 93784,
    "errors": []
  },
  "cpu": {
    "name": "AMD Ryzen 7 5800X 8-Core Processor",
    "vendor": "AuthenticAMD",
    "packages": 1,
    "physical_cores": 8,
    "logical_processors": 16,
    "base_frequency_mhz": 3800,
    "usage_percent": 12.3,
    "sample_interval_ms": 500,
    "errors": []
  },
  "memory": {
    "total_bytes": 34359738368,
    "used_bytes": 12884901888,
    "available_bytes": 21474836480,
    "usage_percent": 37.5,
    "errors": []
  },
  "disk": {
    "volumes": [
      {
        "root": "C:\\",
        "drive_type": "fixed",
        "ready": true,
        "label": "Windows",
        "file_system": "NTFS",
        "total_bytes": 536870912000,
        "used_bytes": 402653184000,
        "free_bytes": 134217728000,
        "usage_percent": 75.0
      },
      {
        "root": "D:\\",
        "drive_type": "optical",
        "ready": false,
        "label": null,
        "file_system": null,
        "total_bytes": null,
        "used_bytes": null,
        "free_bytes": null,
        "usage_percent": null
      }
    ],
    "errors": []
  },
  "gpu": {
    "adapters": [
      {
        "name": "NVIDIA GeForce RTX 3070",
        "vendor": "NVIDIA",
        "vendor_id": 4318,
        "device_id": 9348,
        "dedicated_video_memory_bytes": 8589934592,
        "dedicated_system_memory_bytes": 0,
        "shared_system_memory_bytes": 17179869184
      }
    ],
    "errors": []
  },
  "network": {
    "adapters": [
      {
        "name": "Ethernet",
        "description": "Intel(R) Ethernet Connection",
        "type": "ethernet",
        "mac_address": "00-1A-2B-3C-4D-5E",
        "transmit_bps": 1000000000,
        "receive_bps": 1000000000,
        "ipv4_addresses": [
          "192.168.1.10/24"
        ],
        "ipv6_addresses": [
          "fe80::1/64"
        ],
        "gateways": [
          "192.168.1.1"
        ]
      }
    ],
    "errors": []
  }
}
```
</details>

Sorun yaşanan bir bölüm şöyle görünür:

```json
"memory": {
  "total_bytes": null,
  "used_bytes": null,
  "available_bytes": null,
  "usage_percent": null,
  "errors": [
    { "operation": "GlobalMemoryStatusEx", "code": 5, "message": "Access is denied" }
  ]
}
```

## Mimari

```text
src/
├── core/                  platformdan bağımsız, birim testli (kütüphane: sysdiag_core)
│   ├── model.hpp          sade veri yapıları; std::optional = "bilinmiyor"
│   ├── cli.*              argüman ayrıştırma -> Request | Error (G/Ç yok)
│   ├── report_builder.*   istenen toplayıcıları çalıştırır, hataları izole eder
│   ├── cpu_usage.*        iki zaman ölçümünden kullanım hesabı
│   ├── units.*            taşmaya karşı güvenli aritmetik, byte/yüzde/hız biçimlendirme
│   ├── hw_names.*         üretici kimlikleri, makine türleri, MAC/IPv4 biçimlendirme, ...
│   ├── json_writer.*      kendini doğrulayan küçük akış tabanlı JSON yazıcı
│   ├── json_formatter.cpp / text_formatter.cpp
│   └── text.*             UTF-8 doğrulama, terminal için güvenli temizleme
├── platform/windows/      yalnızca Win32 kodu (kütüphane: sysdiag_windows)
│   ├── *_collector.cpp    her bölüm için bir dosya
│   ├── win_util.*         UTF-16<->UTF-8, hata mesajları, registry, RAII yardımcıları
│   └── console.*          WriteConsoleW / WriteFile çıktısı
└── app/main.cpp           wmain: ayrıştır -> topla -> biçimlendir -> yaz
scripts/smoke-test.ps1     derlenmiş exe için uçtan uca kontroller
tests/                     sysdiag_core için GoogleTest test paketi
```

**Tasarım kararları**

- **Çekirdek ve platform ayrı.** Windows başlık dosyaları olmadan yazılabilen her şey
  `sysdiag_core` içindedir: ayrıştırma, hesaplamalar, biçimlendirme ve orkestrasyon. Bu kod her
  işletim sisteminde derlenir ve test edilir; CI'da Linux üzerinde sanitizer'larla da çalışır.
  Windows katmanı yalnızca API sonuçlarını veri modeline çevirir.
- **Test sınırı veri modelidir.** Toplayıcılar sade struct'ları doldurur, formatlayıcılar
  onları okur. `build_report` toplayıcıları `std::function` olarak alır; böylece testler hiçbir
  Win32 API'sini taklit etmeden sahte toplayıcılar (hata fırlatanlar dahil) verebilir. Sırf
  test edilebilirlik için bir sınıf hiyerarşisi yoktur.
- **"Bilinmiyor" açıkça ifade edilir.** Değerler `std::optional`'dır; hatalar ise ilgili bölüme
  eklenen veridir (`CollectionError`). Toplayıcılar işletim sistemi hataları için exception
  fırlatmaz; yine de `build_report` son savunma hattı olarak exception'ları bölüm bazında yakalar.
- **Yalnızca gereken Win32 API'leri:**
  - **CPU kullanımı:** `--sample-ms` aralıklı iki `GetSystemTimes` ölçümü. PDH sayaç
    yolları dile göre değişir, WMI ise yavaştır ve COM gerektirir. İlk ölçüm tek başına değer
    üretmez; iki ölçüm arasında süre geçmemişse sonuç %0 değil "bilinmiyor" olur.
  - **Fiziksel çekirdek sayısı:** `GetSystemInfo` yerine `GetLogicalProcessorInformationEx`;
    `GetSystemInfo` 64'ten fazla mantıksal işlemcisi olan sistemlerde yanlış sonuç verir.
  - **GPU belleği:** WMI yerine DXGI, çünkü WMI'nin `AdapterRAM` değeri 32 bittir ve 4 GiB'ta
    takılır.
  - **Windows sürümü:** `GetVersionEx` yerine `RtlGetVersion`; `GetVersionEx`, manifest'i
    olmayan uygulamalara yanlış sürüm bildirir. Registry Windows 11'de hâlâ "Windows 10"
    yazdığı için build numarası (≥ 22000) ile düzeltilir.
  - **API uygunluğu:** `IsWow64Process2` çalışma anında aranır; böylece program eski
    Windows 10 sürümlerinde de açılır.
- **JSON kütüphanesi yok.** Araç JSON'u yalnızca *yazar*. İç içe yapıyı kendisi denetleyen,
  kaçış karakterlerini işleyen ve UTF-8 doğrulayan 300 satırdan kısa bir yazıcı, bir
  bağımlılıktan daha ucuzdur.
- **Her yerde RAII.** Registry tamponları, `LocalFree` belleği, COM işaretçileri ve thread
  hata modu nesnelere aittir. Elle temizlik yolu ve global durum yoktur.

## Hata yönetimi

- **Hatalar bağlamıyla raporlanır.** Her işletim sistemi hatası; başarısız olan işlem, hata
  kodu ve sistem mesajı ile birlikte gösterilir (mesaj `FormatMessageW`'den gelir, bu yüzden
  işletim sisteminin dilindedir):

  ```text
  Memory
    Total                 n/a
    ...
    ! GlobalMemoryStatusEx failed: Access is denied (error 5)
  ```
- **Beklenen durumlar hata sayılmaz.** Boş bir kart okuyucu veya DVD sürücüsü *not ready*
  olarak gösterilir; hiç ağ adaptörü olmaması sadece boş bir listedir.
- **Boyutu değişebilen tamponlar yeniden denenir.** Boyut sorgulayan bir API'nin verdiği
  tampon boyutu ikinci çağrıya kadar eskimişse, çağrı sınırlı sayıda tekrarlanır.
- **Girdiye güvenilmez.** Komut satırı değerleri `std::from_chars` ve açık aralık kontrolleriyle
  ayrıştırılır; hata mesajında geri yazılan kullanıcı metni temizlenir ve kısaltılır.
- **Aritmetik taşamaz, sıfıra bölme olmaz.** Çıkarmalar sıfırın altına inmez, toplamı sıfır
  olan yüzdeler `null` olur, sonuçlar 0–100 aralığına sıkıştırılır ve tüm boyutlar 64 bittir.

## Testler

Birim testleri şunları kapsar:

- Uç durumlar ve kötü niyetli girdiler dahil CLI ayrıştırma
- Byte, yüzde ve hız dönüşümleri ile sınır değerleri
- CPU kullanımı hesabı
- JSON kaçış karakterleri ve yazıcının hatalı kullanımı
- Testler için yazılmış bağımsız bir RFC 8259 doğrulayıcı ile JSON geçerliliği
- Formatlayıcı çıktısı
- `build_report` içinde kısmi hata yönetimi

```powershell
ctest --preset msvc-release          # Windows
```

```bash
cmake --preset linux-sanitize        # yalnızca çekirdek kütüphane, ASan + UBSan ile
cmake --build --preset linux-sanitize
ctest --preset linux-sanitize
```

Win32 toplayıcıları birim testiyle sınanmaz, çünkü sonuçları makineye bağlıdır. Bunlar
[`scripts/smoke-test.ps1`](scripts/smoke-test.ps1) ile test edilir; bu script hem yerelde
(bkz. [Hızlı test](#hızlı-test)) hem de CI'da PowerShell 7 ve Windows PowerShell 5.1 ile çalışır.

**CI** (`.github/workflows/ci.yml`) iki iş çalıştırır:

1. **Windows, MSVC (Debug ve Release, uyarılar hata sayılır):**
   - Derler ve birim testlerini çalıştırır.
   - Gerçek programı `scripts/smoke-test.ps1` ile test eder.
   - Release `sysdiag.exe` dosyasını derleme çıktısı (artifact) olarak yükler.
2. **Linux, GCC:** çekirdek kütüphaneyi ASan ve UBSan ile derler ve birim testlerini çalıştırır.

## Bağımlılıklar

| Bağımlılık | Kullanım yeri | Neden |
|------------|---------------|-------|
| Windows SDK (`dxgi`, `iphlpapi`, `ws2_32`) | Toplayıcılar | Sistem API'leri; Windows ile birlikte gelir |
| [GoogleTest](https://github.com/google/googletest) 1.15.2 | **Yalnızca testler** | Yaygın bilinir, CTest ve Visual Studio ile uyumludur. Kuruluysa o kopya kullanılır (`find_package`); değilse `FetchContent` ile indirilir. `sysdiag.exe` ile linklenmez. |

Programın kendisinin hiçbir üçüncü taraf bağımlılığı yoktur.

## Bilinen sınırlamalar

- **Yalnızca Windows.** Diğer platformlarda sadece çekirdek kütüphane derlenir; henüz Linux
  veya macOS toplayıcıları yoktur.
- **CPU:**
  - Kullanım, ölçüm süresi boyunca tek bir sistem geneli ortalamadır. Çekirdek bazında yük
    gösterilmez.
  - 64'ten fazla mantıksal işlemcisi olan makinelerde eski Windows sürümleri
    `GetSystemTimes` değerlerini yalnızca çağıran thread'in işlemci grubu için verebilir.
  - Hibrit işlemcilerdeki performans (P) ve verimlilik (E) çekirdekleri ayırt edilmez.
  - *Base frequency*, firmware'in kaydettiği nominal değerdir; anlık saat hızı değildir.
- **GPU:**
  - Gösterilen bellek DXGI'nin bildirdiği değerdir. Dahili GPU'lar genellikle küçük bir
    ayrılmış ve büyük bir paylaşılan bellek gösterir.
  - Sıcaklık, kullanım oranı ve sürücü sürümü raporlanmaz.
- **Disk:**
  - Yalnızca sürücü harfi olan birimler listelenir; klasöre bağlanmış birimler listelenmez.
  - Bağlantısı kopmuş bir ağ sürücüsü disk bölümünün geç yanıt vermesine neden olabilir.
  - SMART ve disk sağlığı verileri toplanmaz.
- **Ağ:**
  - Yalnızca *açık* (up) adaptörler gösterilir ve loopback hariç tutulur. Sanal adaptörler
    (Hyper-V, VPN, WSL) dahildir.
  - Tünel adaptörlerinde (ör. Teredo) MAC adresi anlamsız olduğu için gösterilmez.
  - IPv6 link-local adresleri zone index olmadan gösterilir.
- **Açık kalma süresi** `GetTickCount64`'ten gelir. Windows *Hızlı Başlangıç* açıksa süre,
  son "kapat" işleminden değil son tam açılıştan itibaren sayılır.
- **Sensör verisi yok.** Sıcaklık, fan hızı ve pil durumu okunmaz; Windows'ta bunlar için
  güvenilir, üreticiden bağımsız bir API yoktur.
- **JSON'da büyük sayılar.** Değerler tam 64 bit tamsayıdır. Tüm sayıları double olarak
  saklayan ayrıştırıcılar (ör. JavaScript) 2^53 byte'ın (yaklaşık 8 PiB) üzerinde hassasiyet
  kaybeder.
- **MinGW derlemeleri** garanti edilmez; CI yalnızca MSVC'yi kapsar.

## Olası geliştirmeler

- Çekirdek bazında CPU kullanımı ve `--watch <saniye>` yenileme modu
- Pil ve güç bilgisi (`GetSystemPowerStatus`)
- Mümkün olduğunda üreticiden bağımsız API'lerle sürücü sürümü ve GPU kullanımı
- Sürücü harfi olmayan birimler (`FindFirstVolumeW`)
- Mevcut veri modeli üzerinde Linux toplayıcıları (`/proc`, `sysfs`)
- GitHub Releases'e eklenen imzalı derlemeler

## Lisans

[MIT](LICENSE)
