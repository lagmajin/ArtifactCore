module;

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <QtGlobal>

export module Project.MetadataCollector;

export namespace ArtifactCore {

enum class MetadataNodeKind {
    Project,
    Composition,
    Layer,
    Effect,
    Property,
};

struct MetadataNode {
    MetadataNodeKind kind = MetadataNodeKind::Project;
    QString id;
    QString name;
    QString type;
    int depth = 0;
    QVariant value;
};

class MetadataReport {
public:
    void add(const QString& key, const QVariant& value) {
        values_[key].push_back(value);
    }

    void addCount(const QString& key, qint64 amount = 1) {
        counts_[key] += amount;
    }

    QVariantList values(const QString& key) const {
        return values_.value(key);
    }

    qint64 count(const QString& key) const {
        return counts_.value(key, 0);
    }

    void merge(const MetadataReport& other) {
        for (auto it = other.counts_.cbegin(); it != other.counts_.cend(); ++it) {
            counts_[it.key()] += it.value();
        }
        for (auto it = other.values_.cbegin(); it != other.values_.cend(); ++it) {
            auto& destination = values_[it.key()];
            for (const auto& value : it.value()) {
                if (!destination.contains(value)) destination.push_back(value);
            }
        }
    }

    QJsonObject toJson() const {
        QJsonObject result;
        QJsonObject counts;
        for (auto it = counts_.cbegin(); it != counts_.cend(); ++it) {
            counts.insert(it.key(), it.value());
        }
        result.insert(QStringLiteral("counts"), counts);

        QJsonObject values;
        for (auto it = values_.cbegin(); it != values_.cend(); ++it) {
            QJsonArray items;
            for (const auto& value : it.value()) {
                items.append(QJsonValue::fromVariant(value));
            }
            values.insert(it.key(), items);
        }
        result.insert(QStringLiteral("values"), values);
        return result;
    }

    QString toCsv() const {
        const auto escapeCsv = [](const QString& input) {
            QString escaped = input;
            escaped.replace('"', QStringLiteral("\"\""));
            return QStringLiteral("\"%1\"").arg(escaped);
        };
        QStringList lines;
        lines << QStringLiteral("kind,key,value");
        for (auto it = counts_.cbegin(); it != counts_.cend(); ++it) {
            lines << QStringLiteral("count,%1,%2")
                         .arg(escapeCsv(it.key()))
                         .arg(it.value());
        }
        for (auto it = values_.cbegin(); it != values_.cend(); ++it) {
            for (const auto& value : it.value()) {
                lines << QStringLiteral("value,%1,%2")
                             .arg(escapeCsv(it.key()))
                             .arg(escapeCsv(value.toString()));
            }
        }
        return lines.join(QChar('\n'));
    }

private:
    QMap<QString, qint64> counts_;
    QMap<QString, QVariantList> values_;
};

class MetadataCollector {
public:
    virtual ~MetadataCollector() = default;
    virtual void reset() {}
    virtual void onProject(const MetadataNode&) {}
    virtual void onComposition(const MetadataNode&) {}
    virtual void onLayer(const MetadataNode&) {}
    virtual void onEffect(const MetadataNode&) {}
    virtual void onProperty(const MetadataNode&) {}
    virtual MetadataReport report() const = 0;
};

class MetadataCollectorDriver {
public:
    static void visit(const QVector<MetadataNode>& nodes,
                      const QVector<MetadataCollector*>& collectors) {
        for (auto* collector : collectors) {
            if (collector) collector->reset();
        }
        for (const auto& node : nodes) {
            for (auto* collector : collectors) {
                if (!collector) continue;
                switch (node.kind) {
                case MetadataNodeKind::Project: collector->onProject(node); break;
                case MetadataNodeKind::Composition: collector->onComposition(node); break;
                case MetadataNodeKind::Layer: collector->onLayer(node); break;
                case MetadataNodeKind::Effect: collector->onEffect(node); break;
                case MetadataNodeKind::Property: collector->onProperty(node); break;
                }
            }
        }
    }

    static MetadataReport collectReport(
        const QVector<MetadataNode>& nodes,
        const QVector<MetadataCollector*>& collectors) {
        visit(nodes, collectors);
        MetadataReport result;
        for (auto* collector : collectors) {
            if (collector) result.merge(collector->report());
        }
        return result;
    }
};

} // namespace ArtifactCore
