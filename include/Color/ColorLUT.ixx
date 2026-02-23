module;

#include <QString>
#include <QImage>
#include <QColor>
#include <QVector3D>
#include <vector>
#include <array>
#include <memory>

export module Color.LUT;

import std;

export namespace ArtifactCore {

/// 3D LUTサイズ
struct LUTSize {
    int dimX = 33;  ///< X方向の格子点数
    int dimY = 33;  ///< Y方向の格子点数
    int dimZ = 33;  ///< Z方向の格子点数
    
    int totalPoints() const { return dimX * dimY * dimZ; }
};

/// LUT形式
enum class LUTFormat {
    Cube,       ///< .cube ファイル（Adobe IRIDAS/Blackmagic）
    Csp,        ///< .csp ファイル（Cinespace）
    3dl,        ///< .3dl ファイル（Autodesk）
    Mga,        ///< .mga ファイル（Pandora）
    Look,       ///< .look ファイル（DaVinci Resolve）
    PNG,        ///< PNG画像（HaldCLUT）
    Unknown
};

/// 3Dカラールックアップテーブル
/// 
/// カラーグレーディング用の3D LUTを管理。
/// 様々な形式のLUTファイルを読み込み、画像に適用可能。
class ColorLUT {
public:
    /// デフォルトコンストラクタ（単位LUT作成）
    ColorLUT();
    
    /// ファイルから読み込み
    explicit ColorLUT(const QString& filePath);
    
    /// コピー・ムーブ
    ColorLUT(const ColorLUT& other);
    ColorLUT& operator=(const ColorLUT& other);
    ColorLUT(ColorLUT&& other) noexcept;
    ColorLUT& operator=(ColorLUT&& other) noexcept;
    ~ColorLUT();
    
    // ========================================
    // 読み込み
    // ========================================
    
    /// ファイルからLUTを読み込み
    bool load(const QString& filePath);
    
    /// CUBE形式から読み込み
    bool loadFromCube(const QString& filePath);
    
    /// Cinespace形式から読み込み
    bool loadFromCsp(const QString& filePath);
    
    /// 3dl形式から読み込み
    bool loadFrom3dl(const QString& filePath);
    
    /// HaldCLUT画像から読み込み
    bool loadFromHaldCLUT(const QString& imagePath);
    
    /// PNG/画像からLUTを抽出
    bool loadFromImage(const QImage& image, int lutSize = 33);
    
    // ========================================
    // 保存
    // ========================================
    
    /// CUBE形式で保存
    bool saveToCube(const QString& filePath) const;
    
    // ========================================
    // プロパティ
    // ========================================
    
    /// LUT名
    QString name() const;
    void setName(const QString& name);
    
    /// ファイルパス
    QString filePath() const;
    
    /// フォーマット
    LUTFormat format() const;
    
    /// サイズ
    LUTSize size() const;
    
    /// 有効かどうか
    bool isValid() const;
    
    /// 読み込みエラーメッセージ
    QString errorMessage() const;
    
    // ========================================
    // 適用
    // ========================================
    
    /// RGB値にLUTを適用（0.0-1.0範囲）
    QColor apply(const QColor& color) const;
    
    /// RGB値にLUTを適用（float版）
    void apply(float& r, float& g, float& b) const;
    
    /// 画像全体にLUTを適用
    QImage applyToImage(const QImage& source) const;
    
    /// 強度を指定して適用（0.0-1.0）
    QColor applyWithIntensity(const QColor& color, float intensity) const;
    
    // ========================================
    // LUT操作
    // ========================================
    
    /// 単位LUT（補正なし）を作成
    static ColorLUT createIdentity(int size = 33);
    
    /// 2つのLUTを合成
    ColorLUT combine(const ColorLUT& other) const;
    
    /// 強度を設定したコピーを作成
    ColorLUT withIntensity(float intensity) const;
    
    /// 逆変換LUTを作成
    ColorLUT inverted() const;
    
    // ========================================
    /// 低レベルアクセス
    // ========================================
    
    /// 指定位置のRGB値を取得
    QVector3D getValue(int x, int y, int z) const;
    
    /// 指定位置のRGB値を設定
    void setValue(int x, int y, int z, const QVector3D& rgb);
    
    /// 生データへのアクセス（3D配列として）
    const float* rawData() const;
    float* rawData();
    
    /// データサイズ（バイト）
    size_t dataSize() const;

private:
    class Impl;
    Impl* impl_;
    
    /// 三線形補間でLUTをサンプリング
    QVector3D sample(float r, float g, float b) const;
};

/// LUTマネージャ（複数LUTの管理）
class LUTManager {
public:
    static LUTManager& instance();
    
    /// LUTを登録
    void registerLUT(const QString& name, const ColorLUT& lut);
    
    /// LUTを取得
    ColorLUT getLUT(const QString& name) const;
    
    /// LUTが存在するか
    bool hasLUT(const QString& name) const;
    
    /// 登録済みLUT名一覧
    QStringList lutNames() const;
    
    /// LUTを削除
    void removeLUT(const QString& name);
    
    /// 全LUTをクリア
    void clear();
    
    /// ディレクトリ内のLUTを一括読み込み
    int loadFromDirectory(const QString& directoryPath);

private:
    LUTManager();
    ~LUTManager();
    
    class Impl;
    Impl* impl_;
};

/// ビルトインLUT（よく使われるルック）
namespace BuiltinLUTs {
    /// シネマティックルック
    ColorLUT cinematic();
    
    /// ビンテージ/フィルムルック
    ColorLUT vintage();
    
    /// コールド/ブルールック
    ColorLUT cold();
    
    /// ウォーム/オレンジルック
    ColorLUT warm();
    
    /// ハイコントラスト
    ColorLUT highContrast();
    
    /// ローコントラスト
    ColorLUT lowContrast();
    
    /// デサチュレート
    ColorLUT desaturated();
    
    /// フィルムストックエミュレーション（Kodak 2383）
    ColorLUT kodak2383();
    
    /// フィルムストックエミュレーション（Fuji 3510）
    ColorLUT fuji3510();
}

} // namespace ArtifactCore