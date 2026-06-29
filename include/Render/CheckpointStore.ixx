module;
#include <utility>
#include <memory>
#include <optional>
#include <QString>
#include <QStringList>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Checkpoint;

import Render.Farm.Types;

export namespace ArtifactCore {

class LIBRARY_DLL_API CheckpointStore {
public:
    CheckpointStore();
    ~CheckpointStore();

    CheckpointStore(const CheckpointStore&) = delete;
    CheckpointStore& operator=(const CheckpointStore&) = delete;

    void setBasePath(const QString& path);
    QString basePath() const;

    bool save(const CheckpointInfo& checkpoint);
    std::optional<CheckpointInfo> load(const QString& jobId);
    bool remove(const QString& jobId);
    QStringList listCheckpoints() const;

    bool checkpointExists(const QString& jobId) const;

    static QString defaultBasePath();
    static QString checkpointFilePath(const QString& basePath, const QString& jobId);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
