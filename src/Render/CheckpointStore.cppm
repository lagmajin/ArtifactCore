module;
#include <utility>
#include <memory>
#include <optional>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <QString>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

module Render.Farm.Checkpoint;

import Render.Farm.Types;

namespace ArtifactCore {

class CheckpointStore::Impl {
public:
    mutable std::mutex mutex_;
    QString basePath_;

    QString jobDir(const QString& jobId) const {
        return basePath_ + QDir::separator() + jobId;
    }

    QString filePath(const QString& jobId) const {
        return jobDir(jobId) + QDir::separator() + "checkpoint.json";
    }

    CheckpointInfo fromJson(const QJsonObject& obj) const {
        CheckpointInfo info;
        info.jobId = obj["jobId"].toString();
        info.completedUpToFrame = obj["completedUpToFrame"].toInt(-1);
        info.totalFrames = obj["totalFrames"].toInt(0);
        info.schemaVersion = obj["schemaVersion"].toInt(1);
        info.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        info.updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODate);

        QJsonArray fails = obj["failures"].toArray();
        for (const auto& f : fails) {
            QJsonObject fo = f.toObject();
            FailedFrameRecord rec;
            rec.frame = fo["frame"].toInt();
            rec.attempt = fo["attempt"].toInt();
            rec.errorMessage = fo["error"].toString();
            rec.held = fo["held"].toBool(false);
            info.failures.failedFrames.push_back(rec);
        }

        return info;
    }

    QJsonObject toJson(const CheckpointInfo& info) const {
        QJsonObject obj;
        obj["jobId"] = info.jobId;
        obj["completedUpToFrame"] = info.completedUpToFrame;
        obj["totalFrames"] = info.totalFrames;
        obj["schemaVersion"] = info.schemaVersion;
        obj["createdAt"] = info.createdAt.toString(Qt::ISODate);
        obj["updatedAt"] = info.updatedAt.toString(Qt::ISODate);

        QJsonArray fails;
        for (const auto& f : info.failures.failedFrames) {
            QJsonObject fo;
            fo["frame"] = f.frame;
            fo["attempt"] = f.attempt;
            fo["error"] = f.errorMessage;
            fo["held"] = f.held;
            fails.append(fo);
        }
        obj["failures"] = fails;

        return obj;
    }

    bool ensureDir(const QString& path) const {
        QDir dir(path);
        if (!dir.exists()) {
            return dir.mkpath(".");
        }
        return true;
    }
};

CheckpointStore::CheckpointStore()
    : impl_(std::make_unique<Impl>())
{
    impl_->basePath_ = defaultBasePath();
}

CheckpointStore::~CheckpointStore() = default;

void CheckpointStore::setBasePath(const QString& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->basePath_ = path;
}

QString CheckpointStore::basePath() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->basePath_;
}

bool CheckpointStore::save(const CheckpointInfo& checkpoint) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    QString dir = impl_->jobDir(checkpoint.jobId);
    if (!impl_->ensureDir(dir)) return false;

    QJsonObject obj = impl_->toJson(checkpoint);
    QFile file(impl_->filePath(checkpoint.jobId));
    if (!file.open(QIODevice::WriteOnly)) return false;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

std::optional<CheckpointInfo> CheckpointStore::load(const QString& jobId) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    QString path = impl_->filePath(jobId);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return std::nullopt;

    return impl_->fromJson(doc.object());
}

bool CheckpointStore::remove(const QString& jobId) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    QString path = impl_->filePath(jobId);
    return QFile::remove(path);
}

QStringList CheckpointStore::listCheckpoints() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    QDir dir(impl_->basePath_);
    if (!dir.exists()) return {};

    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList results;
    for (const auto& entry : entries) {
        if (QFile::exists(checkpointFilePath(impl_->basePath_, entry))) {
            results.append(entry);
        }
    }
    return results;
}

bool CheckpointStore::checkpointExists(const QString& jobId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return QFile::exists(impl_->filePath(jobId));
}

QString CheckpointStore::defaultBasePath() {
    return QDir::tempPath() + QDir::separator() + "ArtifactStudio" + QDir::separator() + "farm" + QDir::separator() + "checkpoints";
}

QString CheckpointStore::checkpointFilePath(const QString& basePath, const QString& jobId) {
    return basePath + QDir::separator() + jobId + QDir::separator() + "checkpoint.json";
}

}
