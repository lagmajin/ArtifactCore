module;

#include <memory>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

export module Color.OCIOConfig;

export namespace ArtifactCore {

/// Color space role - maps a logical role to a concrete color space.
/// AE/OCIO convention: scene_linear, compositing_log, color_timing, texture_paint,
/// matte_paint, default_byte, default_float, etc.
struct OCIORole {
    QString roleName;
    QString colorSpaceName;
};

/// Named color space definition.
struct OCIOColorSpace {
    QString name;
    QString family;
    QString description;
    QStringList aliases;
    bool isData = false;
};

/// View transform (display + view) for preview/output.
struct OCIDisplayView {
    QString display;
    QString view;
    QString colorspace;
    QString looks;
};

/// Color space conversion matrix (simplified - for mock fallback only).
/// When real OCIO library is unavailable, these provide basic transforms.
struct OCIOColorSpaceTransform {
    QString from;
    QString to;
    // 3x3 matrix + offset for linear conversions
    float matrix[9] = {1,0,0, 0,1,0, 0,0,1};
    float offset[3] = {0,0,0};
};

/// OCIO Config wrapper. Loads an OCIO config file or falls back to built-in ACES.
/// No external dependency on OpenColorIO library - uses built-in presets when
/// the real library is unavailable.
class OCIOConfig {
public:
    OCIOConfig();
    explicit OCIOConfig(const QString& configFilePath);
    ~OCIOConfig();

    // Load / save
    bool loadFromFile(const QString& path);
    bool loadFromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool isValid() const;

    // Built-in presets (no external OCIO needed)
    static OCIOConfig createACESConfig();
    static OCIOConfig createSRGBConfig();
    static OCIOConfig createRec709Config();
    static OCIOConfig createRec2020Config();

    // Role queries
    QString colorSpaceForRole(const QString& role) const;
    void setRole(const QString& role, const QString& colorSpace);
    QVector<OCIORole> roles() const;

    // Color space queries
    const OCIOColorSpace* colorSpace(const QString& name) const;
    QVector<OCIOColorSpace> colorSpaces() const;
    void addColorSpace(const OCIOColorSpace& cs);

    // Display / View
    QVector<OCIDisplayView> displayViews() const;
    void addDisplayView(const OCIDisplayView& dv);
    QStringList displays() const;
    QStringList viewsForDisplay(const QString& display) const;

    // Active configuration
    QString activeDisplay() const;
    void setActiveDisplay(const QString& display);
    QString activeView() const;
    void setActiveView(const QString& view);
    QString activeLooks() const;
    void setActiveLooks(const QString& looks);

    // Working space
    QString workingSpace() const;
    void setWorkingSpace(const QString& cs);

    // Config path
    QString configFilePath() const;
    void setConfigFilePath(const QString& path);

    // Error / warning
    QString lastError() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
