module;
#include <utility>
#include <cstddef>
#include "../Define/DllExportMacro.hpp"
#include <QString>
#include <QAbstractItemModel>
#include <QtWidgets/qheaderview.h>

export module Render.JobModel;

import Utils.Id;

export namespace ArtifactCore {

  enum class RenderJobStatus {
    Queued,
    Rendering,
    Done,
    Error,
    Canceled,
    Paused
  };

 class LIBRARY_DLL_API RenderJob
 {
 public:
  RenderJob();
  ~RenderJob();

  ArtifactCore::Id compositionId;
  QString compositionName;
  RenderJobStatus status = RenderJobStatus::Queued;
  float progress = 0.0f; // 0.0 to 1.0
  QString outputPath;
  QString statusMessage;
  int startFrame = 0;
  int endFrame = 299;
  int frameStep = 1;
  bool multiFrameEnabled = true;
  int mfrConcurrentFrames = 0;
  std::size_t mfrMemoryLimitMB = 8192;
  int mfrRetryBackoffMs = 0;
  // ... future: startTime, endTime
 };

 class RenderJobHeaderView :public QHeaderView
 {
 private:
  class Impl;
  Impl* impl_;
 protected:
	void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
 public:
  explicit RenderJobHeaderView(Qt::Orientation orientation,QWidget*parent=nullptr);
  ~RenderJobHeaderView();

 };

 class LIBRARY_DLL_API RenderJobModel :public QAbstractItemModel {
 private:
  class Impl;
  Impl* impl_;
 protected:
	
 public:
  RenderJobModel(QObject* parent = nullptr);
  ~RenderJobModel();
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;


  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   
  // Job Management
  void addJob(const ArtifactCore::Id& compositionId, const QString& name);
  void addJob(const ArtifactCore::Id& compositionId, const QString& name,
              int startFrame, int endFrame, int frameStep = 1);
  void addJob(const QString& name, const QString& status, int progress, const QString& outputPath);
  void removeJob(int row);
  void clearJobs();
  RenderJob* jobAt(int row);

  void setJobProgress(int row, float progress);
  void setJobStatus(int row, RenderJobStatus status);
  bool setJobFrameRange(int row, int startFrame, int endFrame, int frameStep = 1);
  bool setJobMfrSettings(int row, bool enabled, int maxConcurrentFrames,
                         std::size_t memoryLimitMB, int retryBackoffMs = 0);
 };




};
