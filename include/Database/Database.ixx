module;
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include <cstdint>
#include <utility>

export module Core.Database;

import Core.Database.Types;

export namespace ArtifactCore {

class PreparedQuery {
public:
    PreparedQuery() = default;
    explicit PreparedQuery(QSqlQuery query) : query_(std::move(query)) {}

    PreparedQuery& bind(const QVariant& value);
    PreparedQuery& bind(const QString& value);
    PreparedQuery& bind(const QByteArray& value);
    PreparedQuery& bind(qint64 value);
    PreparedQuery& bind(int value);
    PreparedQuery& bind(double value);
    PreparedQuery& bind(bool value);
    PreparedQuery& bindNull();
    bool exec();
    bool execInsert();
    bool next();
    QVariant value(int columnIndex) const;
    QVariant value(const QString& columnName) const;
    template<typename T> T get(int columnIndex) const { return value(columnIndex).template value<T>(); }
    template<typename T> T get(const QString& columnName) const { return value(columnName).template value<T>(); }
    int columnCount() const;
    QStringList columnNames() const;
    qint64 lastInsertId() const;
    int numRowsAffected() const;
    QString lastError() const;

private:
    QSqlQuery query_;
};

class ArtifactDatabase {
public:
    struct Config {
        QString filePath;
        QString connectionName;
        int busyTimeoutMs = 5000;
        bool walMode = true;
        bool foreignKeys = true;
    };

    static std::unique_ptr<ArtifactDatabase> open(const Config& config,
                                                   DatabaseError* error = nullptr);
    ~ArtifactDatabase();
    ArtifactDatabase(const ArtifactDatabase&) = delete;
    ArtifactDatabase& operator=(const ArtifactDatabase&) = delete;

    void close();
    bool isOpen() const;
    const Config& config() const { return config_; }
    bool beginTransaction();
    bool commit();
    bool rollback();
    PreparedQuery prepare(const QString& sql);
    bool execute(const QString& sql);
    QStringList tables() const;
    bool tableExists(const QString& name) const;
    QSqlDatabase& raw() { return db_; }
    const DatabaseError& lastError() const { return lastError_; }
    bool backupTo(const QString& path);

private:
    explicit ArtifactDatabase(Config config);
    bool initialize();
    Config config_;
    QSqlDatabase db_;
    DatabaseError lastError_;
};

class MigrationRunner {
public:
    struct Migration {
        int version = 0;
        QString description;
        QString sql;
        QString rollbackSql;
    };
    static bool runAll(ArtifactDatabase& db, const std::vector<Migration>& migrations);
    static int currentVersion(ArtifactDatabase& db);
    static bool rollbackLast(ArtifactDatabase& db);
};

}
