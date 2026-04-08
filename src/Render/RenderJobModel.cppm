module;
#include <utility>
#include <vector>
#include <memory>

#include <algorithm>
#include <QString>
#include <QAbstractItemModel>
#include <QtWidgets/qheaderview.h>

module Render.JobModel;


namespace ArtifactCore {
  RenderJob::RenderJob() = default;
  RenderJob::~RenderJob() = default;

  class RenderJobModel::Impl {
  public:
    std::vector<std::unique_ptr<RenderJob>> jobs;
  };
 class RenderJobHeaderView::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };
 RenderJobHeaderView::Impl::Impl()
 {

 }

 RenderJobHeaderView::Impl::~Impl()
 {

 }

 RenderJobHeaderView::RenderJobHeaderView(Qt::Orientation orientation, QWidget* parent/*=nullptr*/) :QHeaderView(orientation,parent)
 {

 }

 RenderJobHeaderView::~RenderJobHeaderView()
 {

 }

 void RenderJobHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
 {

 }

  RenderJobModel::RenderJobModel(QObject* parent) : QAbstractItemModel(parent), impl_(new Impl()) {}
  RenderJobModel::~RenderJobModel() { delete impl_; }

 


 QVariant RenderJobModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
 {
  if (role != Qt::DisplayRole)
   return QVariant();

  if (orientation == Qt::Horizontal) {
    switch (section) {
    case 0: return QString("Composition");
    case 1: return QString("Status");
    case 2: return QString("Progress");
    case 3: return QString("Output Path");
    default: return QVariant();
    }
   }
   return QVariant();
  }

  int RenderJobModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return (int)impl_->jobs.size();
  }

  int RenderJobModel::columnCount(const QModelIndex& parent) const {
    return 4;
  }

  QVariant RenderJobModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= (int)impl_->jobs.size()) return QVariant();
    
    if (role == Qt::DisplayRole) {
      const auto& job = impl_->jobs[index.row()];
      switch (index.column()) {
        case 0: return job->compositionName;
        case 1: {
          switch (job->status) {
            case RenderJobStatus::Queued: return "Queued";
            case RenderJobStatus::Rendering: return "Rendering";
            case RenderJobStatus::Done: return "Done";
            case RenderJobStatus::Error: return "Error";
            case RenderJobStatus::Canceled: return "Canceled";
            case RenderJobStatus::Paused: return "Paused";
            default: return "Unknown";
          }
        }
        case 2: return QString("%1%").arg((int)(job->progress * 100));
        case 3: return job->outputPath;
        default: break;
      }
    }
    return QVariant();
  }

  QModelIndex RenderJobModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return QModelIndex();
    return createIndex(row, column, (void*)nullptr);
  }

  QModelIndex RenderJobModel::parent(const QModelIndex& child) const { return QModelIndex(); }

  void RenderJobModel::addJob(const Id& compositionId, const QString& name) {
    beginInsertRows(QModelIndex(), (int)impl_->jobs.size(), (int)impl_->jobs.size());
    auto job = std::make_unique<RenderJob>();
    job->compositionId = compositionId;
    job->compositionName = name;
    impl_->jobs.push_back(std::move(job));
    endInsertRows();
  }

  namespace {
    RenderJobStatus parseRenderJobStatus(const QString& status)
    {
      const QString normalized = status.trimmed().toLower();
      if (normalized == "rendering" || normalized == "running") return RenderJobStatus::Rendering;
      if (normalized == "completed" || normalized == "done") return RenderJobStatus::Done;
      if (normalized == "failed" || normalized == "error") return RenderJobStatus::Error;
      if (normalized == "paused") return RenderJobStatus::Paused;
      if (normalized == "canceled" || normalized == "cancelled") return RenderJobStatus::Canceled;
      return RenderJobStatus::Queued;
    }
  }

  void RenderJobModel::addJob(const QString& name, const QString& status, int progress, const QString& outputPath) {
    beginInsertRows(QModelIndex(), (int)impl_->jobs.size(), (int)impl_->jobs.size());
    auto job = std::make_unique<RenderJob>();
    job->compositionId = Id::Nil();
    job->compositionName = name;
    job->status = parseRenderJobStatus(status);
    job->progress = std::clamp(progress, 0, 100) / 100.0f;
    job->outputPath = outputPath;
    impl_->jobs.push_back(std::move(job));
    endInsertRows();
  }

  void RenderJobModel::removeJob(int row) {
    if (row < 0 || row >= (int)impl_->jobs.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    impl_->jobs.erase(impl_->jobs.begin() + row);
    endRemoveRows();
  }

  void RenderJobModel::clearJobs() {
    beginResetModel();
    impl_->jobs.clear();
    endResetModel();
  }

  RenderJob* RenderJobModel::jobAt(int row) {
    if (row < 0 || row >= (int)impl_->jobs.size()) return nullptr;
    return impl_->jobs[row].get();
  }

  void RenderJobModel::setJobProgress(int row, float progress) {
    if (row < 0 || row >= (int)impl_->jobs.size()) return;
    impl_->jobs[row]->progress = progress;
    emit dataChanged(index(row, 2), index(row, 2), {Qt::DisplayRole});
  }

  void RenderJobModel::setJobStatus(int row, RenderJobStatus status) {
    if (row < 0 || row >= (int)impl_->jobs.size()) return;
    impl_->jobs[row]->status = status;
    emit dataChanged(index(row, 1), index(row, 1), {Qt::DisplayRole});
  }


};
