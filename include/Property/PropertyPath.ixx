module;

#include <QString>
#include <QStringList>
#include <QHash>
#include "../Define/DllExportMacro.hpp"

#include <functional>
#include <utility>
#include <optional>
#include <vector>
#include <string>

export module Property.Path;

export namespace ArtifactCore {

class LIBRARY_DLL_API PropertyPath {
public:
    PropertyPath() = default;
    PropertyPath(const PropertyPath&) = default;
    PropertyPath(PropertyPath&&) noexcept = default;
    PropertyPath& operator=(const PropertyPath&) = default;
    PropertyPath& operator=(PropertyPath&&) noexcept = default;

    explicit PropertyPath(const QString& path)
        : segments_(splitPath(path)), raw_(normalize(path)) {}

    PropertyPath(const QString& ownerPath, const QString& propertyName)
        : segments_(splitPath(joinParts(ownerPath, propertyName))),
          raw_(normalize(joinParts(ownerPath, propertyName))) {}

    static PropertyPath fromSegments(const QStringList& segments)
    {
        PropertyPath p;
        p.segments_ = segments;
        p.raw_ = segments.join(QLatin1Char('.'));
        return p;
    }

    bool isValid() const
    {
        return !segments_.isEmpty();
    }

    bool isEmpty() const
    {
        return segments_.isEmpty();
    }

    const QString& toString() const
    {
        return raw_;
    }

    operator const QString&() const
    {
        return raw_;
    }

    int depth() const
    {
        return segments_.size();
    }

    QString segment(int index) const
    {
        if (index < 0 || index >= segments_.size()) {
            return {};
        }
        return segments_[index];
    }

    QString ownerPath() const
    {
        if (segments_.size() <= 1) {
            return {};
        }
        return segments_.mid(0, segments_.size() - 1).join(QLatin1Char('.'));
    }

    QString propertyName() const
    {
        if (segments_.isEmpty()) {
            return {};
        }
        return segments_.last();
    }

    PropertyPath parent() const
    {
        if (segments_.size() <= 1) {
            return PropertyPath();
        }
        return fromSegments(segments_.mid(0, segments_.size() - 1));
    }

    PropertyPath appended(const QString& segment) const
    {
        if (segment.isEmpty()) {
            return *this;
        }
        QStringList next = segments_;
        next.append(segment);
        return fromSegments(next);
    }

    bool startsWith(const PropertyPath& prefix) const
    {
        if (prefix.segments_.size() > segments_.size()) {
            return false;
        }
        for (int i = 0; i < prefix.segments_.size(); ++i) {
            if (segments_[i] != prefix.segments_[i]) {
                return false;
            }
        }
        return true;
    }

    PropertyPath relativeTo(const PropertyPath& ancestor) const
    {
        if (!startsWith(ancestor)) {
            return PropertyPath();
        }
        return fromSegments(segments_.mid(ancestor.segments_.size()));
    }

    bool operator==(const PropertyPath& other) const
    {
        return raw_ == other.raw_;
    }

    bool operator!=(const PropertyPath& other) const
    {
        return raw_ != other.raw_;
    }

    bool operator<(const PropertyPath& other) const
    {
        return raw_ < other.raw_;
    }

    bool operator==(const QString& str) const
    {
        return raw_ == str;
    }

    struct Hash {
        size_t operator()(const PropertyPath& p) const noexcept
        {
            return qHash(p.raw_);
        }
    };

private:
    PropertyPath(QStringList segments, QString raw)
        : segments_(std::move(segments)), raw_(std::move(raw)) {}

    static QStringList splitPath(const QString& path)
    {
        if (path.isEmpty()) {
            return {};
        }
        QStringList parts = path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
        return parts;
    }

    static QString normalize(const QString& path)
    {
        return splitPath(path).join(QLatin1Char('.'));
    }

    static QString joinParts(const QString& ownerPath, const QString& propertyName)
    {
        if (ownerPath.isEmpty()) {
            return propertyName;
        }
        if (propertyName.isEmpty()) {
            return ownerPath;
        }
        return ownerPath + QLatin1Char('.') + propertyName;
    }

    QStringList segments_;
    QString raw_;
};

inline size_t qHash(const PropertyPath& path, size_t seed = 0) noexcept
{
    return qHash(path.toString(), seed);
}

} // namespace ArtifactCore

template <>
struct std::hash<ArtifactCore::PropertyPath> {
    size_t operator()(const ArtifactCore::PropertyPath& p) const noexcept
    {
        return ArtifactCore::PropertyPath::Hash{}(p);
    }
};
