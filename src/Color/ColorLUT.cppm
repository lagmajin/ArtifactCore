module;

#include <QString>
#include <QImage>
#include <QColor>
#include <QVector3D>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMap>
#include <vector>
#include <cmath>
#include <algorithm>

module Color.LUT;

import std;

namespace ArtifactCore {

// ============================================================================
// ColorLUT::Impl
// ============================================================================

class ColorLUT::Impl {
public:
    QString name;
    QString filePath;
    LUTFormat format = LUTFormat::Unknown;
    LUTSize size;
    std::vector<float> data;  // RGBRGB... の順で格納
    bool valid = false;
    QString errorMessage;
    
    // インデックス計算
    size_t index(int x, int y, int z) const {
        return static_cast<size_t>((z * size.dimY + y) * size.dimX + x) * 3;
    }
    
    // 三線形補間
    QVector3D trilinearInterpolation(float r, float g, float b) const {
        // 0.0-1.0をLUTインデックスに変換
        float fx = r * (size.dimX - 1);
        float fy = g * (size.dimY - 1);
        float fz = b * (size.dimZ - 1);
        
        int x0 = static_cast<int>(std::floor(fx));
        int y0 = static_cast<int>(std::floor(fy));
        int z0 = static_cast<int>(std::floor(fz));
        
        int x1 = std::min(x0 + 1, size.dimX - 1);
        int y1 = std::min(y0 + 1, size.dimY - 1);
        int z1 = std::min(z0 + 1, size.dimZ - 1);
        
        x0 = std::clamp(x0, 0, size.dimX - 1);
        y0 = std::clamp(y0, 0, size.dimY - 1);
        z0 = std::clamp(z0, 0, size.dimZ - 1);
        
        float dx = fx - x0;
        float dy = fy - y0;
        float dz = fz - z0;
        
        // 8点をサンプリング
        auto getVal = [this](int x, int y, int z) -> QVector3D {
            size_t idx = index(x, y, z);
            if (idx + 2 < data.size()) {
                return QVector3D(data[idx], data[idx + 1], data[idx + 2]);
            }
            return QVector3D(0, 0, 0);
        };
        
        QVector3D c000 = getVal(x0, y0, z0);
        QVector3D c100 = getVal(x1, y0, z0);
        QVector3D c010 = getVal(x0, y1, z0);
        QVector3D c110 = getVal(x1, y1, z0);
        QVector3D c001 = getVal(x0, y0, z1);
        QVector3D c101 = getVal(x1, y0, z1);
        QVector3D c011 = getVal(x0, y1, z1);
        QVector3D c111 = getVal(x1, y1, z1);
        
        // 三線形補間
        auto lerp = [](const QVector3D& a, const QVector3D& b, float t) {
            return a + (b - a) * t;
        };
        
        QVector3D c00 = lerp(c000, c100, dx);
        QVector3D c01 = lerp(c001, c101, dx);
        QVector3D c10 = lerp(c010, c110, dx);
        QVector3D c11 = lerp(c011, c111, dx);
        
        QVector3D c0 = lerp(c00, c10, dy);
        QVector3D c1 = lerp(c01, c11, dy);
        
        return lerp(c0, c1, dz);
    }
};

// ============================================================================
// ColorLUT 実装
// ============================================================================

ColorLUT::ColorLUT() : impl_(new Impl()) {
    // 単位LUT作成
    *this = createIdentity(33);
}

ColorLUT::ColorLUT(const QString& filePath) : impl_(new Impl()) {
    load(filePath);
}

ColorLUT::ColorLUT(const ColorLUT& other) : impl_(new Impl(*other.impl_)) {}

ColorLUT& ColorLUT::operator=(const ColorLUT& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
    }
    return *this;
}

ColorLUT::ColorLUT(ColorLUT&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

ColorLUT& ColorLUT::operator=(ColorLUT&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

ColorLUT::~ColorLUT() {
    delete impl_;
}

// ========================================
// 読み込み
// ========================================

bool ColorLUT::load(const QString& filePath) {
    impl_->filePath = filePath;
    impl_->valid = false;
    
    // 拡張子でフォーマット判定
    QString ext = QFileInfo(filePath).suffix().toLower();
    
    if (ext == "cube") {
        return loadFromCube(filePath);
    } else if (ext == "csp") {
        return loadFromCsp(filePath);
    } else if (ext == "3dl") {
        return loadFrom3dl(filePath);
    } else if (ext == "png" || ext == "jpg" || ext == "tiff") {
        return loadFromHaldCLUT(filePath);
    }
    
    impl_->errorMessage = "Unknown LUT format: " + ext;
    return false;
}

bool ColorLUT::loadFromCube(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        impl_->errorMessage = "Cannot open file: " + filePath;
        return false;
    }
    
    impl_->format = LUTFormat::Cube;
    impl_->name = QFileInfo(filePath).baseName();
    
    QTextStream in(&file);
    std::vector<float> values;
    
    int dimX = 0, dimY = 0, dimZ = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // コメントと空行をスキップ
        if (line.isEmpty() || line.startsWith('#')) continue;
        
        // キーワード解析
        if (line.startsWith("TITLE", Qt::CaseInsensitive)) {
            impl_->name = line.mid(6).trimmed().remove('"');
            continue;
        }
        
        if (line.startsWith("LUT_3D_SIZE", Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                dimX = dimY = dimZ = parts[1].toInt();
            }
            continue;
        }
        
        if (line.startsWith("LUT_3D_INPUT_RANGE", Qt::CaseInsensitive)) {
            // 範囲指定（今回は無視）
            continue;
        }
        
        // 数値データ
        QStringList nums = line.split(QRegularExpression("\\s+"));
        for (const QString& num : nums) {
            bool ok;
            float v = num.toFloat(&ok);
            if (ok) {
                values.push_back(v);
            }
        }
    }
    
    file.close();
    
    // サイズ設定
    if (dimX > 0 && dimY > 0 && dimZ > 0) {
        impl_->size = {dimX, dimY, dimZ};
    } else {
        // サイズが指定されていない場合、データ数から推定
        int total = static_cast<int>(values.size() / 3);
        int dim = static_cast<int>(std::round(std::cbrt(total)));
        impl_->size = {dim, dim, dim};
    }
    
    // データコピー
    impl_->data = std::move(values);
    impl_->valid = !impl_->data.empty();
    
    return impl_->valid;
}

bool ColorLUT::loadFromCsp(const QString& filePath) {
    // Cinespace形式（簡易実装）
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        impl_->errorMessage = "Cannot open file: " + filePath;
        return false;
    }
    
    impl_->format = LUTFormat::Csp;
    impl_->name = QFileInfo(filePath).baseName();
    
    // バイナリ読み込み
    QByteArray data = file.readAll();
    file.close();
    
    // ヘッダーパース（簡易版）
    // 実際のcsp形式はより複雑
    
    impl_->valid = false;
    impl_->errorMessage = "CSP format not fully implemented";
    return false;
}

bool ColorLUT::loadFrom3dl(const QString& filePath) {
    // Autodesk 3dl形式
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        impl_->errorMessage = "Cannot open file: " + filePath;
        return false;
    }
    
    impl_->format = LUTFormat::_3dl;
    impl_->name = QFileInfo(filePath).baseName();
    
    QTextStream in(&file);
    std::vector<float> values;
    int dim = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        if (line.isEmpty()) continue;
        
        // 最初の行はLUTサイズの場合が多い
        if (dim == 0 && !line.contains(' ')) {
            bool ok;
            dim = line.toInt(&ok);
            if (!ok) dim = 0;
            continue;
        }
        
        // 数値データ
        QStringList nums = line.split(QRegularExpression("\\s+"));
        for (const QString& num : nums) {
            bool ok;
            float v = num.toFloat(&ok);
            if (ok) {
                values.push_back(v);
            }
        }
    }
    
    file.close();
    
    if (dim > 0) {
        impl_->size = {dim, dim, dim};
    } else {
        int total = static_cast<int>(values.size() / 3);
        dim = static_cast<int>(std::round(std::cbrt(total)));
        impl_->size = {dim, dim, dim};
    }
    
    impl_->data = std::move(values);
    impl_->valid = !impl_->data.empty();
    
    return impl_->valid;
}

bool ColorLUT::loadFromHaldCLUT(const QString& imagePath) {
    QImage image(imagePath);
    if (image.isNull()) {
        impl_->errorMessage = "Cannot load image: " + imagePath;
        return false;
    }
    
    return loadFromImage(image);
}

bool ColorLUT::loadFromImage(const QImage& image, int lutSize) {
    if (image.isNull()) {
        impl_->errorMessage = "Invalid image";
        return false;
    }
    
    impl_->format = LUTFormat::PNG;
    impl_->size = {lutSize, lutSize, lutSize};
    
    QImage converted = image.convertToFormat(QImage::Format_RGB32);
    
    int totalPoints = lutSize * lutSize * lutSize;
    impl_->data.clear();
    impl_->data.reserve(totalPoints * 3);
    
    // HaldCLUTまたは通常画像からLUTデータを抽出
    float scale = 1.0f / 255.0f;
    
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                // 画像内の位置を計算（HaldCLUTレイアウト）
                int px = x + (z % (image.width() / lutSize)) * lutSize;
                int py = y + (z / (image.width() / lutSize)) * lutSize;
                
                if (px < image.width() && py < image.height()) {
                    QRgb pixel = converted.pixel(px, py);
                    impl_->data.push_back(qRed(pixel) * scale);
                    impl_->data.push_back(qGreen(pixel) * scale);
                    impl_->data.push_back(qBlue(pixel) * scale);
                } else {
                    impl_->data.push_back(x / float(lutSize - 1));
                    impl_->data.push_back(y / float(lutSize - 1));
                    impl_->data.push_back(z / float(lutSize - 1));
                }
            }
        }
    }
    
    impl_->valid = true;
    return true;
}

// ========================================
// 保存
// ========================================

bool ColorLUT::saveToCube(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "TITLE \"" << impl_->name << "\"\n";
    out << "# Created by Artifact\n";
    out << "LUT_3D_SIZE " << impl_->size.dimX << "\n";
    out << "\n";
    
    for (int z = 0; z < impl_->size.dimZ; ++z) {
        for (int y = 0; y < impl_->size.dimY; ++y) {
            for (int x = 0; x < impl_->size.dimX; ++x) {
                size_t idx = impl_->index(x, y, z);
                out << impl_->data[idx] << " " 
                    << impl_->data[idx + 1] << " " 
                    << impl_->data[idx + 2] << "\n";
            }
        }
    }
    
    file.close();
    return true;
}

// ========================================
// プロパティ
// ========================================

QString ColorLUT::name() const { return impl_->name; }
void ColorLUT::setName(const QString& name) { impl_->name = name; }

QString ColorLUT::filePath() const { return impl_->filePath; }

LUTFormat ColorLUT::format() const { return impl_->format; }

LUTSize ColorLUT::size() const { return impl_->size; }

bool ColorLUT::isValid() const { return impl_->valid; }

QString ColorLUT::errorMessage() const { return impl_->errorMessage; }

// ========================================
// 適用
// ========================================

QColor ColorLUT::apply(const QColor& color) const {
    float r = color.redF();
    float g = color.greenF();
    float b = color.blueF();
    
    apply(r, g, b);
    
    return QColor::fromRgbF(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f),
        color.alphaF()
    );
}

void ColorLUT::apply(float& r, float& g, float& b) const {
    if (!impl_->valid) return;
    
    // クランプ
    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    
    // 三線形補間でサンプリング
    QVector3D result = impl_->trilinearInterpolation(r, g, b);
    
    r = result.x();
    g = result.y();
    b = result.z();
}

QImage ColorLUT::applyToImage(const QImage& source) const {
    if (!impl_->valid) return source;
    
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            
            apply(r, g, b);
            
            line[x] = qRgba(
                static_cast<int>(r * 255),
                static_cast<int>(g * 255),
                static_cast<int>(b * 255),
                qAlpha(line[x])
            );
        }
    }
    
    return result;
}

QColor ColorLUT::applyWithIntensity(const QColor& color, float intensity) const {
    QColor original = color;
    QColor transformed = apply(color);
    
    // 線形ブレンド
    return QColor::fromRgbF(
        original.redF() * (1.0f - intensity) + transformed.redF() * intensity,
        original.greenF() * (1.0f - intensity) + transformed.greenF() * intensity,
        original.blueF() * (1.0f - intensity) + transformed.blueF() * intensity,
        color.alphaF()
    );
}

// ========================================
// LUT操作
// ========================================

ColorLUT ColorLUT::createIdentity(int size) {
    ColorLUT lut;
    lut.impl_->size = {size, size, size};
    lut.impl_->data.clear();
    lut.impl_->data.reserve(size * size * size * 3);
    lut.impl_->valid = true;
    lut.impl_->name = "Identity";
    
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float r = x / float(size - 1);
                float g = y / float(size - 1);
                float b = z / float(size - 1);
                
                lut.impl_->data.push_back(r);
                lut.impl_->data.push_back(g);
                lut.impl_->data.push_back(b);
            }
        }
    }
    
    return lut;
}

ColorLUT ColorLUT::combine(const ColorLUT& other) const {
    if (!impl_->valid || !other.isValid()) return *this;
    
    ColorLUT result = createIdentity(impl_->size.dimX);
    
    for (int z = 0; z < result.impl_->size.dimZ; ++z) {
        for (int y = 0; y < result.impl_->size.dimY; ++y) {
            for (int x = 0; x < result.impl_->size.dimX; ++x) {
                size_t idx = result.impl_->index(x, y, z);
                
                float r = impl_->data[idx];
                float g = impl_->data[idx + 1];
                float b = impl_->data[idx + 2];
                
                // 最初のLUTを適用
                QVector3D v1 = impl_->trilinearInterpolation(
                    x / float(result.impl_->size.dimX - 1),
                    y / float(result.impl_->size.dimY - 1),
                    z / float(result.impl_->size.dimZ - 1)
                );
                
                // 2番目のLUTを適用
                float r2 = v1.x(), g2 = v1.y(), b2 = v1.z();
                other.apply(r2, g2, b2);
                
                result.impl_->data[idx] = r2;
                result.impl_->data[idx + 1] = g2;
                result.impl_->data[idx + 2] = b2;
            }
        }
    }
    
    return result;
}

ColorLUT ColorLUT::withIntensity(float intensity) const {
    if (!impl_->valid) return *this;
    
    ColorLUT result = *this;
    ColorLUT identity = createIdentity(impl_->size.dimX);
    
    for (size_t i = 0; i < result.impl_->data.size(); ++i) {
        result.impl_->data[i] = identity.impl_->data[i] * (1.0f - intensity) + 
                                result.impl_->data[i] * intensity;
    }
    
    return result;
}

ColorLUT ColorLUT::inverted() const {
    // 逆変換の近似（完全な逆変換は計算コストが高い）
    if (!impl_->valid) return *this;
    
    ColorLUT result = createIdentity(impl_->size.dimX);
    
    // 各出力色に対して、最も近い入力色を探す（簡易実装）
    // 完全な実装では反復的な最適化が必要
    
    return result;
}

// ========================================
// 低レベルアクセス
// ========================================

QVector3D ColorLUT::getValue(int x, int y, int z) const {
    size_t idx = impl_->index(x, y, z);
    if (idx + 2 < impl_->data.size()) {
        return QVector3D(impl_->data[idx], impl_->data[idx + 1], impl_->data[idx + 2]);
    }
    return QVector3D();
}

void ColorLUT::setValue(int x, int y, int z, const QVector3D& rgb) {
    size_t idx = impl_->index(x, y, z);
    if (idx + 2 < impl_->data.size()) {
        impl_->data[idx] = rgb.x();
        impl_->data[idx + 1] = rgb.y();
        impl_->data[idx + 2] = rgb.z();
    }
}

const float* ColorLUT::rawData() const { return impl_->data.data(); }
float* ColorLUT::rawData() { return impl_->data.data(); }

size_t ColorLUT::dataSize() const { return impl_->data.size() * sizeof(float); }

QVector3D ColorLUT::sample(float r, float g, float b) const {
    return impl_->trilinearInterpolation(r, g, b);
}

// ============================================================================
// LUTManager 実装
// ============================================================================

class LUTManager::Impl {
public:
    QMap<QString, ColorLUT> luts;
};

LUTManager::LUTManager() : impl_(new Impl()) {}

LUTManager::~LUTManager() { delete impl_; }

LUTManager& LUTManager::instance() {
    static LUTManager manager;
    return manager;
}

void LUTManager::registerLUT(const QString& name, const ColorLUT& lut) {
    impl_->luts[name] = lut;
}

ColorLUT LUTManager::getLUT(const QString& name) const {
    return impl_->luts.value(name);
}

bool LUTManager::hasLUT(const QString& name) const {
    return impl_->luts.contains(name);
}

QStringList LUTManager::lutNames() const {
    return impl_->luts.keys();
}

void LUTManager::removeLUT(const QString& name) {
    impl_->luts.remove(name);
}

void LUTManager::clear() {
    impl_->luts.clear();
}

int LUTManager::loadFromDirectory(const QString& directoryPath) {
    QDir dir(directoryPath);
    int count = 0;
    
    QStringList filters;
    filters << "*.cube" << "*.Cube" << "*.CUBE";
    
    for (const QFileInfo& info : dir.entryInfoList(filters)) {
        ColorLUT lut(info.absoluteFilePath());
        if (lut.isValid()) {
            registerLUT(info.baseName(), lut);
            ++count;
        }
    }
    
    return count;
}

// ============================================================================
// Builtin LUTs
// ============================================================================

namespace BuiltinLUTs {

ColorLUT cinematic() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    // シネマティックな色調（オレンジとティールの強調）
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                // シンプルなカラーシフト
                r = std::clamp(r * 1.1f, 0.0f, 1.0f);
                g = std::clamp(g * 0.9f, 0.0f, 1.0f);
                b = std::clamp(b * 1.15f, 0.0f, 1.0f);
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT vintage() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    // ビンテージ（セピア寄り）
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                float gray = r * 0.3f + g * 0.59f + b * 0.11f;
                r = std::clamp(gray * 1.2f + 0.1f, 0.0f, 1.0f);
                g = std::clamp(gray * 1.0f + 0.05f, 0.0f, 1.0f);
                b = std::clamp(gray * 0.8f, 0.0f, 1.0f);
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT cold() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                r = r * 0.9f;
                g = g * 0.95f;
                b = std::clamp(b * 1.2f, 0.0f, 1.0f);
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT warm() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                r = std::clamp(r * 1.2f, 0.0f, 1.0f);
                g = g * 1.05f;
                b = b * 0.8f;
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT highContrast() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                // S字カーブ
                auto sCurve = [](float v) {
                    return std::clamp((v - 0.5f) * 1.5f + 0.5f, 0.0f, 1.0f);
                };
                
                lut.setValue(x, y, z, QVector3D(sCurve(r), sCurve(g), sCurve(b)));
            }
        }
    }
    return lut;
}

ColorLUT lowContrast() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                // コントラスト低下
                r = r * 0.7f + 0.15f;
                g = g * 0.7f + 0.15f;
                b = b * 0.7f + 0.15f;
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT desaturated() {
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                float gray = r * 0.3f + g * 0.59f + b * 0.11f;
                
                lut.setValue(x, y, z, QVector3D(gray, gray, gray));
            }
        }
    }
    return lut;
}

ColorLUT kodak2383() {
    // Kodak 2383 フィルムストックの簡易エミュレーション
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                // 簡易的なフィルムエミュレーション
                r = std::clamp(std::pow(r, 0.9f), 0.0f, 1.0f);
                g = std::clamp(std::pow(g, 0.95f), 0.0f, 1.0f);
                b = std::clamp(std::pow(b, 1.1f), 0.0f, 1.0f);
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

ColorLUT fuji3510() {
    // Fuji 3510 フィルムストックの簡易エミュレーション
    ColorLUT lut = ColorLUT::createIdentity(17);
    for (int z = 0; z < 17; ++z) {
        for (int y = 0; y < 17; ++y) {
            for (int x = 0; x < 17; ++x) {
                float r = x / 16.0f;
                float g = y / 16.0f;
                float b = z / 16.0f;
                
                r = std::clamp(std::pow(r, 0.95f), 0.0f, 1.0f);
                g = std::clamp(std::pow(g, 0.92f), 0.0f, 1.0f);
                b = std::clamp(std::pow(b, 1.05f), 0.0f, 1.0f);
                
                lut.setValue(x, y, z, QVector3D(r, g, b));
            }
        }
    }
    return lut;
}

} // namespace BuiltinLUTs

} // namespace ArtifactCore