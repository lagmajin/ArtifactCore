module;
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QList>

export module Core.Database.Types;

export namespace ArtifactCore {

struct DatabaseError {
    QString text;
    int nativeCode = 0;
    bool isValid() const { return !text.isEmpty(); }
};

struct DatabaseRow {
    QStringList columns;
    QList<QVariant> values;
    QVariant value(int index) const {
        return index >= 0 && index < values.size() ? values.at(index) : QVariant();
    }
};

}
