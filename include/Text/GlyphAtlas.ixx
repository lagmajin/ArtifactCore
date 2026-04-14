module;
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <QFont>
#include <QImage>
#include <QRawFont>

export module Text.GlyphAtlas;

import Text.Style;

export namespace ArtifactCore {

/// Glyph atlas への登録キー（フォント設定 + コードポイント）
struct GlyphKey {
    char32_t    codePoint   = 0;
    float       fontSize    = 0.0f;
    uint32_t    styleFlags  = 0; // bit0=bold, bit1=italic
    std::string fontFamily;      // UTF-8

    bool operator==(const GlyphKey& o) const noexcept {
        return codePoint == o.codePoint
            && fontSize  == o.fontSize
            && styleFlags == o.styleFlags
            && fontFamily == o.fontFamily;
    }
};

} // namespace ArtifactCore

// std::hash specialization — global namespace
template <>
struct std::hash<ArtifactCore::GlyphKey> {
    std::size_t operator()(const ArtifactCore::GlyphKey& k) const noexcept {
        std::size_t h = std::hash<char32_t>{}(k.codePoint);
        h ^= std::hash<float>{}(k.fontSize)      + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>{}(k.styleFlags) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.fontFamily) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

export namespace ArtifactCore {

/// atlas 内でのグリフの位置・メトリクス
struct GlyphRect {
    int         atlasX      = 0;
    int         atlasY      = 0;
    int         width       = 0;
    int         height      = 0;
    float       bearingX    = 0.0f; ///< ペンから左端までのオフセット
    float       bearingY    = 0.0f; ///< ペンから上端までのオフセット（上が正）
    float       advance     = 0.0f; ///< 次の文字へのペン送り量
    bool        valid       = false;

    // UV 計算ヘルパ（atlas サイズ必要）
    float u0(int atlasW) const { return atlasW > 0 ? float(atlasX)         / float(atlasW) : 0.0f; }
    float v0(int atlasH) const { return atlasH > 0 ? float(atlasY)         / float(atlasH) : 0.0f; }
    float u1(int atlasW) const { return atlasW > 0 ? float(atlasX + width) / float(atlasW) : 0.0f; }
    float v1(int atlasH) const { return atlasH > 0 ? float(atlasY + height)/ float(atlasH) : 0.0f; }
};

/// CPU 側 glyph atlas 管理クラス
///
/// QRawFont でラスタライズし、RGBA8 QImage に pack する。
/// QImage は "dirty" フラグで管理し、GPU upload は呼び出し元 (PrimitiveRenderer2D) が行う。
/// atlas が満杯になったら clear() してリビルドする単純方針。
class GlyphAtlas {
public:
    static constexpr int kAtlasSize = 2048;
    static constexpr int kPadding   = 2; ///< グリフ間のピクセルパディング

    GlyphAtlas();
    ~GlyphAtlas();

    GlyphAtlas(const GlyphAtlas&)            = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    /// グリフを登録し GlyphRect を返す。既にキャッシュされていればそれを返す。
    /// 満杯の場合は atlas を clear して再登録する（この場合 isDirtyFull() == true）。
    GlyphRect acquire(const GlyphKey& key, const QFont& font);

    /// atlas テクスチャ画像（RGBA8, kAtlasSize × kAtlasSize）
    const QImage& atlasImage() const { return atlasImage_; }

    /// GPU への再アップロードが必要かどうか
    bool isDirty() const { return dirty_; }
    void clearDirty()    { dirty_ = false; }

    /// atlas を完全にリセット（GPUテクスチャも再アップロード必要）
    void clear();

    int width()  const { return kAtlasSize; }
    int height() const { return kAtlasSize; }

private:
    bool packGlyph(int w, int h, int& outX, int& outY);

    QImage atlasImage_;
    std::unordered_map<GlyphKey, GlyphRect> cache_;

    // shelf packing state
    int currentShelfX_   = 0;
    int currentShelfY_   = 0;
    int currentShelfH_   = 0;
    bool dirty_          = false;
};

} // namespace ArtifactCore
