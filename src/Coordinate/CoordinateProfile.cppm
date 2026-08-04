module;
#include <QString>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>
#include <cmath>

module Coordinate.Profile;

namespace ArtifactCore {

double CoordinateValue::resolve(double canvasSize, double safeSize, double gridSize) const {
    switch (unit) {
    case CoordinateUnit::Percent:
        return value * canvasSize / 100.0;
    case CoordinateUnit::Normalized:
        return value * canvasSize;
    case CoordinateUnit::Grid:
        return value * gridSize;
    case CoordinateUnit::SafeArea:
        return safeSize + value;
    case CoordinateUnit::Pixels:
    default:
        return value;
    }
}

QString CoordinateValue::toString() const {
    switch (unit) {
    case CoordinateUnit::Pixels: return QString::number(value, 'f', 2) + "px";
    case CoordinateUnit::Percent: return QString::number(value, 'f', 2) + "%";
    case CoordinateUnit::Normalized: return QString::number(value, 'f', 4);
    case CoordinateUnit::Grid: return QStringLiteral("grid[") + QString::number(value, 'f', 1) + "]";
    case CoordinateUnit::SafeArea: return "safe + " + QString::number(value, 'f', 2) + "px";
    default: return QString::number(value);
    }
}

CoordinateValue CoordinateValue::fromString(const QString& str) {
    const QString trimmed = str.trimmed();
    CoordinateValue val;

    if (trimmed.endsWith("px", Qt::CaseInsensitive)) {
        val.value = trimmed.chopped(2).trimmed().toDouble();
        val.unit = CoordinateUnit::Pixels;
    } else if (trimmed.endsWith("%")) {
        val.value = trimmed.chopped(1).trimmed().toDouble();
        val.unit = CoordinateUnit::Percent;
    } else if (trimmed.startsWith("grid[", Qt::CaseInsensitive) && trimmed.endsWith("]")) {
        val.value = trimmed.mid(5, trimmed.length() - 6).trimmed().toDouble();
        val.unit = CoordinateUnit::Grid;
    } else if (trimmed.startsWith("safe", Qt::CaseInsensitive)) {
        QString clean = trimmed.mid(4).trimmed();
        if (clean.startsWith("+")) clean = clean.mid(1).trimmed();
        val.value = clean.toDouble();
        val.unit = CoordinateUnit::SafeArea;
    } else {
        val.value = trimmed.toDouble();
        val.unit = CoordinateUnit::Normalized;
    }
    val.expression = str;
    return val;
}

QJsonObject CoordinateValue::toJson() const {
    QJsonObject obj;
    obj["value"] = value;
    obj["unit"] = static_cast<int>(unit);
    obj["expression"] = expression;
    return obj;
}

CoordinateValue CoordinateValue::fromJson(const QJsonObject& obj) {
    CoordinateValue val;
    val.value = obj["value"].toDouble();
    val.unit = static_cast<CoordinateUnit>(obj.value("unit").toInt(static_cast<int>(CoordinateUnit::Pixels)));
    val.expression = obj["expression"].toString();
    return val;
}

double CoordinateSpace::resolveX(const CoordinateValue& val) const {
    switch (val.unit) {
    case CoordinateUnit::Percent:
        return val.value * canvasWidth / 100.0;
    case CoordinateUnit::Normalized:
        return val.value * canvasWidth;
    case CoordinateUnit::Grid:
        return val.value * gridSizeX;
    case CoordinateUnit::SafeArea:
        return safeLeft + val.value;
    case CoordinateUnit::Pixels:
    default:
        return val.value;
    }
}

double CoordinateSpace::resolveY(const CoordinateValue& val) const {
    switch (val.unit) {
    case CoordinateUnit::Percent:
        return val.value * canvasHeight / 100.0;
    case CoordinateUnit::Normalized:
        return val.value * canvasHeight;
    case CoordinateUnit::Grid:
        return val.value * gridSizeY;
    case CoordinateUnit::SafeArea:
        return safeTop + val.value;
    case CoordinateUnit::Pixels:
    default:
        return val.value;
    }
}

QJsonObject CoordinateProfile::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["baseUnit"] = static_cast<int>(baseUnit);
    obj["displayUnit"] = static_cast<int>(displayUnit);
    obj["enablePercent"] = enablePercent;
    obj["enableNormalized"] = enableNormalized;
    obj["enableGrid"] = enableGrid;
    return obj;
}

CoordinateProfile CoordinateProfile::fromJson(const QJsonObject& obj) {
    CoordinateProfile profile;
    profile.id = obj["id"].toString();
    profile.name = obj["name"].toString();
    profile.baseUnit = static_cast<CoordinateUnit>(obj.value("baseUnit").toInt(static_cast<int>(CoordinateUnit::Pixels)));
    profile.displayUnit = static_cast<CoordinateUnit>(obj.value("displayUnit").toInt(static_cast<int>(CoordinateUnit::Pixels)));
    profile.enablePercent = obj.value("enablePercent").toBool(true);
    profile.enableNormalized = obj.value("enableNormalized").toBool(true);
    profile.enableGrid = obj.value("enableGrid").toBool(true);
    return profile;
}

CoordinateProfile CoordinateProfile::defaultPixels() {
    return {"coord.px", "Pixels", CoordinateUnit::Pixels, CoordinateUnit::Pixels, true, true, true};
}

CoordinateProfile CoordinateProfile::defaultPercent() {
    return {"coord.pct", "Percent", CoordinateUnit::Percent, CoordinateUnit::Percent, true, true, true};
}

CoordinateProfile CoordinateProfile::defaultNormalized() {
    return {"coord.norm", "Normalized", CoordinateUnit::Normalized, CoordinateUnit::Normalized, true, true, true};
}

CoordinateResolver::CoordinateResolver(const CoordinateProfile& profile)
    : profile_(profile) {}

void CoordinateResolver::setProfile(const CoordinateProfile& profile) {
    profile_ = profile;
}

double CoordinateResolver::resolve(const CoordinateValue& val, const CoordinateSpace& space) const {
    return val.resolve(
        profile_.baseUnit == CoordinateUnit::Percent ? 100.0 : space.canvasWidth,
        space.safeLeft,
        space.gridSizeX
    );
}

CoordinateValue CoordinateResolver::convert(const CoordinateValue& val, CoordinateUnit targetUnit,
                                              const CoordinateSpace& space) const {
    double rawPixels = space.resolveX(val);
    CoordinateValue result;
    result.unit = targetUnit;

    switch (targetUnit) {
    case CoordinateUnit::Percent:
        result.value = space.canvasWidth > 0.0 ? rawPixels * 100.0 / space.canvasWidth : 0.0;
        break;
    case CoordinateUnit::Normalized:
        result.value = space.canvasWidth > 0.0 ? rawPixels / space.canvasWidth : 0.0;
        break;
    case CoordinateUnit::Grid:
        result.value = space.gridSizeX > 0.0 ? rawPixels / space.gridSizeX : 0.0;
        break;
    case CoordinateUnit::SafeArea:
        result.value = rawPixels - space.safeLeft;
        break;
    case CoordinateUnit::Pixels:
    default:
        result.value = rawPixels;
        break;
    }
    return result;
}

QString CoordinateResolver::displayText(const CoordinateValue& val, const CoordinateSpace& space) const {
    CoordinateValue display = convert(val, profile_.displayUnit, space);
    return display.toString();
}

double CoordinateResolver::resolveExpression(const QString& expr, const CoordinateSpace& space) {
    QString clean = expr.trimmed();

    if (clean.endsWith("px", Qt::CaseInsensitive)) {
        return clean.chopped(2).trimmed().toDouble();
    }
    if (clean.endsWith("%")) {
        double pct = clean.chopped(1).trimmed().toDouble();
        return pct * space.canvasWidth / 100.0;
    }
    if (clean.contains('w', Qt::CaseInsensitive) || clean.contains('h', Qt::CaseInsensitive)) {
        double result = 0.0;
        QString term;
        char lastOp = '+';
        for (const QChar ch : clean) {
            if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
                if (!term.isEmpty()) {
                    double t = term.trimmed().toDouble();
                    if (lastOp == '+') result += t;
                    else if (lastOp == '-') result -= t;
                    else if (lastOp == '*') result *= t;
                    else if (lastOp == '/' && t != 0.0) result /= t;
                }
                lastOp = ch.toLatin1();
                term.clear();
            } else if (ch == 'w' || ch == 'W') {
                double t = space.canvasWidth;
                if (lastOp == '+') result += t;
                else if (lastOp == '-') result -= t;
                else if (lastOp == '*') result *= t;
                else if (lastOp == '/' && t != 0.0) result /= t;
                lastOp = '+';
            } else if (ch == 'h' || ch == 'H') {
                double t = space.canvasHeight;
                if (lastOp == '+') result += t;
                else if (lastOp == '-') result -= t;
                else if (lastOp == '*') result *= t;
                else if (lastOp == '/' && t != 0.0) result /= t;
                lastOp = '+';
            } else {
                term += ch;
            }
        }
        if (!term.isEmpty()) {
            double t = term.trimmed().toDouble();
            if (lastOp == '+') result += t;
            else if (lastOp == '-') result -= t;
            else if (lastOp == '*') result *= t;
            else if (lastOp == '/' && t != 0.0) result /= t;
        }
        return result;
    }

    bool ok = false;
    double v = clean.toDouble(&ok);
    return ok ? v : 0.0;
}

} // namespace ArtifactCore
