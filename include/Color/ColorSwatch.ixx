module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

export module Color.Swatch;

import Color.Float;
import Core.ArtifactString;

export namespace ArtifactCore {

/**
 * @brief 個別のカラー項目 (色 + 名前)
 */
struct LIBRARY_DLL_API SwatchEntry {
    FloatColor color;
    String name;

    SwatchEntry() = default;
    SwatchEntry(const FloatColor& c, const String& n = "") : color(c), name(n) {}
};

/**
 * @brief カラーパレット全体を管理するクラス
 * 
 * スウォッチファイルの読み込み/保存（.ase, .gpl, .json等）と
 * 色のコレクション管理を行います。
 */
class LIBRARY_DLL_API ColorSwatch {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    ColorSwatch();
    explicit ColorSwatch(const String& name);
    ~ColorSwatch();

    // コピー/ムーブ
    ColorSwatch(const ColorSwatch& other);
    ColorSwatch(ColorSwatch&& other) noexcept;
    ColorSwatch& operator=(const ColorSwatch& other);
    ColorSwatch& operator=(ColorSwatch&& other) noexcept;

    // 基本情報
    const String& getName() const;
    void setName(const String& name);

    // 要素アクセス
    size_t count() const;
    const SwatchEntry& at(size_t index) const;
    void addEntry(const SwatchEntry& entry);
    void addColor(const FloatColor& color, const String& name = "");
    void removeAt(size_t index);
    void clear();

    // 検索・操作
    int findColor(const FloatColor& color, float tolerance = 0.001f) const;
    void sort(); // 名前または色味でソート（将来拡張）

    // ファイルI/O
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;

    // よく使う形式へのエクスポート
    bool importGPL(const std::filesystem::path& path);
    bool exportGPL(const std::filesystem::path& path) const;
};

} // namespace ArtifactCore
