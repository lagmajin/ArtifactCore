module;
#include <QAbstractItemModel>
#include "../Define/DllExportMacro.hpp"

export module Render.JobModel;


export namespace ArtifactCore {

 class LIBRARY_DLL_API RenderJobModel :public QAbstractItemModel{
 private:

 public:
  RenderJobModel(QObject*parent=nullptr);
  ~RenderJobModel();
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
 };




};

