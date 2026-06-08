module;
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <utility>
#include <functional>
#include <optional>

export module Coordinate.Profile;

export namespace ArtifactCore {

export enum class CoordinateUnit {
    Pixels,
    Percent,
    Normalized,
    Grid,
    SafeArea
};

export struct CoordinateValue {
    double value = 0.0;
    CoordinateUnit unit = CoordinateUnit::Pixels;
    QString expression;

    double resolve(double canvasSize, double safeSize, double gridSize) const;
    QString toString() const;
    static CoordinateValue fromString(const QString& str);

    QJsonObject toJson() const;
    static CoordinateValue fromJson(const QJsonObject& obj);
};

export enum class CoordinateSpaceKind {
    Canvas,
    SafeArea,
    TileGrid,
    World
};

export struct CoordinateSpace {
    CoordinateSpaceKind kind = CoordinateSpaceKind::Canvas;
    QString name;
    double canvasWidth = 1920.0;
    double canvasHeight = 1080.0;
    double safeLeft = 0.0;
    double safeTop = 0.0;
    double safeRight = 1920.0;
    double safeBottom = 1080.0;
    double gridSizeX = 100.0;
    double gridSizeY = 100.0;

    double resolveX(const CoordinateValue& val) const;
    double resolveY(const CoordinateValue& val) const;
};

export struct CoordinateProfile {
    QString id;
    QString name;
    CoordinateUnit baseUnit = CoordinateUnit::Pixels;
    CoordinateUnit displayUnit = CoordinateUnit::Pixels;
    bool enablePercent = true;
    bool enableNormalized = true;
    bool enableGrid = true;

    QJsonObject toJson() const;
    static CoordinateProfile fromJson(const QJsonObject& obj);
    static CoordinateProfile defaultPixels();
    static CoordinateProfile defaultPercent();
    static CoordinateProfile defaultNormalized();
};

export class CoordinateResolver {
public:
    CoordinateResolver() = default;
    explicit CoordinateResolver(const CoordinateProfile& profile);

    void setProfile(const CoordinateProfile& profile);
    const CoordinateProfile& profile() const { return profile_; }

    double resolve(const CoordinateValue& val, const CoordinateSpace& space) const;
    CoordinateValue convert(const CoordinateValue& val, CoordinateUnit targetUnit,
                            const CoordinateSpace& space) const;

    QString displayText(const CoordinateValue& val, const CoordinateSpace& space) const;

    static double resolveExpression(const QString& expr, const CoordinateSpace& space);

private:
    CoordinateProfile profile_;
};

} // namespace ArtifactCore
