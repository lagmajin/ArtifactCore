module;

#include <QString>
#include <QAbstractItemModel>

module Render.JobModel;


namespace ArtifactCore {

 RenderJobModel::RenderJobModel(QObject* parent) :QAbstractItemModel(parent)
 {

 }

 RenderJobModel::~RenderJobModel()
 {

 }

 

 QVariant RenderJobModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
 {
  if (role != Qt::DisplayRole)
   return QVariant();

  if (orientation == Qt::Horizontal) {
   switch (section) {
   case 0:
	return QString("■レンダリング"); // 0列目のヘッダ
   case 1:
	return QString("■ステータス");   // 1列目のヘッダ
   case 2:
	return QString("開始");
   case 3:
	return QString("レンダリング時間");
   	default:
	break;
   }
  }
  // 垂直ヘッダ（行番号など）が必要な場合はここに実装
  // if (orientation == Qt::Vertical) {
  //     return section + 1; // 1から始まる行番号
  // }

  return QVariant();
 }

 int RenderJobModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  return 0;
 }

 int RenderJobModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
 {
  return 3;
 }


};