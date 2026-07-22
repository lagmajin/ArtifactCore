# Utility Patterns Reference (from Wicked Engine)

> Source: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiColor.h
>        https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiMath.h
>        https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiHelper.h
> Target: Utils module, Math module, Color module

## Color (wiColor.h) — Packed uint32_t RGBA

### Why Packed?
- 32 bits vs 128 bits for `FloatRGBA` (4× float)
- `constexpr` 対応でコンパイル時評価可能
- コピー/比較が単一レジスタ操作
- シリアライズが `archive << rgba` の1行
- 16進文字列からの `constexpr` パースで可読性維持

### Pattern

```cpp
struct PackedColor {
    uint32_t rgba = 0;

    constexpr PackedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : rgba(r | (g<<8) | (b<<16) | (a<<24)) {}

    // Compile-time hex parsing: Color("#FF8800") or Color("FF8800FF")
    constexpr PackedColor(const char* hex) { /* ... */ }

    constexpr uint8_t getR() const { return (rgba >> 0) & 0xFF; }
    constexpr uint8_t getG() const { return (rgba >> 8) & 0xFF; }
    constexpr uint8_t getB() const { return (rgba >> 16) & 0xFF; }
    constexpr uint8_t getA() const { return (rgba >> 24) & 0xFF; }

    constexpr XMFLOAT4 toFloat4() const {
        return { getR()/255.0f, getG()/255.0f, getB()/255.0f, getA()/255.0f };
    }

    static constexpr PackedColor lerp(PackedColor a, PackedColor b, float t) {
        return fromFloat4(wi::math::Lerp(a.toFloat4(), b.toFloat4(), t));
    }

    // Named constants (all constexpr)
    static constexpr PackedColor Red()   { return {255,0,0,255}; }
    static constexpr PackedColor Green() { return {0,255,0,255}; }
    static constexpr PackedColor Blue()  { return {0,0,255,255}; }
    static constexpr PackedColor Black() { return {0,0,0,255}; }
    static constexpr PackedColor White() { return {255,255,255,255}; }
    static constexpr PackedColor Transparent() { return {0,0,0,0}; }
    static constexpr PackedColor Warning() { return 0xFF66FFFF; }  // constexpr hex
    static constexpr PackedColor Error()   { return 0xFF6666FF; }
    static constexpr PackedColor Success() { return 0xFF94FF63; }
};
```

## Math (wiMath.h) — Missing from ArtifactCore

### Interpolation functions

```cpp
// Lerp overloads for all vector types
XMVECTOR Lerp(FXMVECTOR a, FXMVECTOR b, float t);
XMFLOAT2 Lerp(const XMFLOAT2& a, const XMFLOAT2& b, float t);
XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, float t);

// Smooth interpolation
XMVECTOR SmoothStep(FXMVECTOR a, FXMVECTOR b, float t);

// Cubic / CatmullRom
XMVECTOR CubicLerp(FXMVECTOR p0, FXMVECTOR p1, FXMVECTOR p2, FXMVECTOR p3, float t);
XMVECTOR CatmullRom(FXMVECTOR p0, FXMVECTOR p1, FXMVECTOR p2, FXMVECTOR p3, float t);

// Hermite spline tangent
XMVECTOR HermiteTangent(FXMVECTOR p0, FXMVECTOR p1, FXMVECTOR t0, FXMVECTOR t1, float t);
```

### Packing utilities

```cpp
// Pack float [0,1] into 16-bit UNORM (for GPU vertex buffers)
constexpr uint32_t pack_unorm16x2(float x, float y) {
    return (uint32_t(saturate(x) * 65535.0f)) |
           (uint32_t(saturate(y) * 65535.0f) << 16u);
}

constexpr XMUINT2 pack_unorm16x4(float x, float y, float z, float w) {
    return XMUINT2(pack_unorm16x2(x, y), pack_unorm16x2(z, w));
}
```

### Distance helpers

```cpp
float Length(const XMFLOAT2& v);
float Length(const XMFLOAT3& v);
float LengthSquared(const XMFLOAT2& v);
float LengthSquared(const XMFLOAT3& v);
float Distance(const XMFLOAT2& v1, const XMFLOAT2& v2);
float Distance(const XMFLOAT3& v1, const XMFLOAT3& v2);
float DistanceSquared(const XMFLOAT2& v1, const XMFLOAT2& v2);
float DistanceSquared(const XMFLOAT3& v1, const XMFLOAT3& v2);
```

### Ray intersection

```cpp
// Möller-Trumbore ray-triangle intersection with barycentrics
bool RayTriangleIntersects(
    FXMVECTOR Origin, FXMVECTOR Direction,
    FXMVECTOR V0, GXMVECTOR V1, HXMVECTOR V2,
    float& Dist, XMFLOAT2& bary,
    float TMin = 0, float TMax = FLT_MAX
);
```

## Helper (wiHelper.h) — General Utilities

```cpp
// Compile-time string hash
constexpr size_t string_hash(const char* input);

// Runtime hash combination (boost::hash_combine style)
template<class T>
constexpr void hash_combine(size_t& seed, const T& v) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// String utilities
std::string toUpper(const std::string& s);
std::string toLower(const std::string& s);
void SplitPath(const std::string& fullPath, std::string& dir, std::string& fileName);
std::string GetFileNameFromPath(const std::string& path);
std::string GetExtensionFromFileName(const std::string& fileName);

// Human-readable formatting
std::string GetMemorySizeText(size_t sizeInBytes);     // "1.5 GB"
std::string GetTimerDurationText(float seconds);        // "2.3 min"

// Platform dialogs
std::string FileDialog(const std::string& type, const std::string& filter, ...);
std::string FolderDialog(const std::string& title, ...);

// Memory
struct MemoryUsage {
    uint64_t total_physical, total_virtual, process_physical, process_virtual;
};
MemoryUsage GetMemoryUsage();

// Compression
bool Compress(const uint8_t* src, size_t srcSize, vector<uint8_t>& dst, int level = 0);
bool Decompress(const uint8_t* src, size_t srcSize, vector<uint8_t>& dst);

// Clipboard
std::wstring GetClipboardText();
void SetClipboardText(const std::wstring& wstr);

// Debug
enum class DebugLevel { Normal, Warning, Error };
void DebugOut(const std::string& str, DebugLevel level = DebugLevel::Normal);

// Sleep helpers
void Sleep(float milliseconds);    // OS can overtake (power efficient)
void Spin(float milliseconds);     // Busy-wait (high precision)
void QuickSleep(float ms);         // Smart: Sleep if long, Spin if short
```

## ArtifactCore Adaptation Priority

| Priority | Pattern | Target Module | Effort |
|---|---|---|---|
| High | `hash_combine` + `string_hash` | Utils/Hash.ixx | Tiny |
| High | `GetMemorySizeText` + `GetTimerDurationText` | Utils/Format.ixx | Tiny |
| Medium | `Lerp`/`SmoothStep`/`CatmullRom` for float3 | Math/Interpolate.ixx | Small |
| Medium | `pack_unorm16x2`/`pack_unorm16x4` | Math/Pack.ixx | Tiny |
| Low | `PackedColor` (constexpr uint32_t) | Color/PackedColor.ixx | Small |
| Low | `SplitPath` / `GetFileNameFromPath` | Utils/Path.ixx | Tiny |

## Notes

- ArtifactCore already has `FloatRGBA` (128-bit) for GPU-path colors.
  `PackedColor` (32-bit) would be used for UI colors, serialization, and constants.
- `string_hash` + `hash_combine` enable compile-time hashing for switch-on-string
  and efficient runtime hashing of composite keys.
- All `constexpr` functions are header-only, no .cppm needed.