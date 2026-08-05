module;
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDateTime>
#include <algorithm>

module Core.Database;

namespace ArtifactCore {

namespace {
void setError(DatabaseError* target, const QSqlError& error) {
    if (target) target->text = error.text();
}
QString connectionNameFor(const ArtifactDatabase::Config& config) {
    return config.connectionName.isEmpty()
        ? QStringLiteral("artifact-db-%1").arg(reinterpret_cast<quintptr>(&config), 0, 16)
        : config.connectionName;
}
}

PreparedQuery& PreparedQuery::bind(const QVariant& value) { query_.addBindValue(value); return *this; }
PreparedQuery& PreparedQuery::bind(const QString& value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bind(const QByteArray& value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bind(qint64 value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bind(int value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bind(double value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bind(bool value) { return bind(QVariant(value)); }
PreparedQuery& PreparedQuery::bindNull() { query_.addBindValue(QVariant()); return *this; }
bool PreparedQuery::exec() { return query_.exec(); }
bool PreparedQuery::execInsert() { return query_.exec(); }
bool PreparedQuery::next() { return query_.next(); }
QVariant PreparedQuery::value(int index) const { return query_.value(index); }
QVariant PreparedQuery::value(const QString& name) const { return query_.value(name); }
int PreparedQuery::columnCount() const { return query_.record().count(); }
QStringList PreparedQuery::columnNames() const {
    QStringList result;
    const auto record = query_.record();
    for (int i = 0; i < record.count(); ++i) result.append(record.fieldName(i));
    return result;
}
qint64 PreparedQuery::lastInsertId() const { return query_.lastInsertId().toLongLong(); }
int PreparedQuery::numRowsAffected() const { return query_.numRowsAffected(); }
QString PreparedQuery::lastError() const { return query_.lastError().text(); }

ArtifactDatabase::ArtifactDatabase(Config config) : config_(std::move(config)) {}
ArtifactDatabase::~ArtifactDatabase() { close(); }

std::unique_ptr<ArtifactDatabase> ArtifactDatabase::open(const Config& config, DatabaseError* error) {
    if (config.filePath.trimmed().isEmpty()) {
        if (error) error->text = QStringLiteral("Database file path is empty");
        return nullptr;
    }
    auto database = std::unique_ptr<ArtifactDatabase>(new ArtifactDatabase(config));
    if (!database->initialize()) {
        if (error) *error = database->lastError_;
        return nullptr;
    }
    return database;
}

bool ArtifactDatabase::initialize() {
    const QFileInfo info(config_.filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        lastError_.text = QStringLiteral("Unable to create database directory");
        return false;
    }
    const QString name = connectionNameFor(config_);
    db_ = QSqlDatabase::contains(name) ? QSqlDatabase::database(name) : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db_.setDatabaseName(config_.filePath);
    if (!db_.open()) { lastError_.text = db_.lastError().text(); return false; }
    if (!execute(QStringLiteral("PRAGMA busy_timeout=%1").arg(std::max(0, config_.busyTimeoutMs)))) return false;
    if (config_.foreignKeys && !execute(QStringLiteral("PRAGMA foreign_keys=ON"))) return false;
    if (config_.walMode && !execute(QStringLiteral("PRAGMA journal_mode=WAL"))) return false;
    return true;
}

void ArtifactDatabase::close() {
    if (!db_.isValid()) return;
    const QString name = db_.connectionName();
    db_.close();
    db_ = QSqlDatabase();
    if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
}
bool ArtifactDatabase::isOpen() const { return db_.isOpen(); }
bool ArtifactDatabase::beginTransaction() { return db_.transaction(); }
bool ArtifactDatabase::commit() { return db_.commit(); }
bool ArtifactDatabase::rollback() { return db_.rollback(); }
PreparedQuery ArtifactDatabase::prepare(const QString& sql) { QSqlQuery query(db_); query.prepare(sql); return PreparedQuery(std::move(query)); }
bool ArtifactDatabase::execute(const QString& sql) { QSqlQuery query(db_); if (query.exec(sql)) return true; lastError_.text = query.lastError().text(); return false; }
QStringList ArtifactDatabase::tables() const { return db_.tables(QSql::Tables); }
bool ArtifactDatabase::tableExists(const QString& name) const { return tables().contains(name); }

bool ArtifactDatabase::backupTo(const QString& path) {
    if (!isOpen() || path.trimmed().isEmpty()) return false;
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(path);
    if (query.exec()) return true;
    lastError_.text = query.lastError().text();
    return false;
}

bool MigrationRunner::runAll(ArtifactDatabase& db, const std::vector<Migration>& migrations) {
    if (!db.execute(QStringLiteral("CREATE TABLE IF NOT EXISTS artifact_schema_migrations (version INTEGER PRIMARY KEY, description TEXT NOT NULL, rollback_sql TEXT, applied_at_ms INTEGER NOT NULL)"))) return false;
    auto ordered = migrations;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.version < b.version; });
    for (const auto& migration : ordered) {
        if (migration.version <= currentVersion(db)) continue;
        if (!db.beginTransaction()) return false;
        bool ok = true;
        for (const auto& statement : migration.sql.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            if (!db.execute(statement.trimmed())) { ok = false; break; }
        }
        if (ok) {
            auto query = db.prepare(QStringLiteral("INSERT INTO artifact_schema_migrations(version, description, rollback_sql, applied_at_ms) VALUES (?, ?, ?, ?)"));
            query.bind(migration.version).bind(migration.description).bind(migration.rollbackSql).bind(QDateTime::currentMSecsSinceEpoch());
            ok = query.exec();
        }
        if (ok) ok = db.commit(); else db.rollback();
        if (!ok) return false;
    }
    return true;
}

int MigrationRunner::currentVersion(ArtifactDatabase& db) {
    if (!db.tableExists(QStringLiteral("artifact_schema_migrations"))) return 0;
    auto query = db.prepare(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM artifact_schema_migrations"));
    if (!query.exec() || !query.next()) return 0;
    return query.value(0).toInt();
}

bool MigrationRunner::rollbackLast(ArtifactDatabase& db) {
    if (!db.tableExists(QStringLiteral("artifact_schema_migrations"))) return false;
    auto query = db.prepare(QStringLiteral("SELECT version, rollback_sql FROM artifact_schema_migrations ORDER BY version DESC LIMIT 1"));
    if (!query.exec() || !query.next()) return false;
    const int version = query.value(0).toInt();
    const QString rollbackSql = query.value(1).toString();
    if (!rollbackSql.isEmpty() && !db.beginTransaction()) return false;
    if (!rollbackSql.isEmpty()) {
        for (const auto& statement : rollbackSql.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            if (!db.execute(statement.trimmed())) { db.rollback(); return false; }
        }
    }
    auto remove = db.prepare(QStringLiteral("DELETE FROM artifact_schema_migrations WHERE version = ?"));
    remove.bind(version);
    const bool removed = remove.exec();
    if (!rollbackSql.isEmpty()) return removed ? db.commit() : (db.rollback(), false);
    return removed;
}

}
