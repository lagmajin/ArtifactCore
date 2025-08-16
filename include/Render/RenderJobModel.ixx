module;
#include <QAbstractItemModel>
#include <QtWidgets/qheaderview.h>

#include "../Define/DllExportMacro.hpp"

export module Render.JobModel;


export namespace ArtifactCore {

 class RenderJob
 {
 private:

 public:
  RenderJob();
  ~RenderJob();
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
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override { return QVariant(); }
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override { return QModelIndex(); }
  QModelIndex parent(const QModelIndex& child) const override { return QModelIndex(); }


  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  void addJob();
 };




};

