module;
#include <QDirIterator>
#include <QImage>
#include <QCoreApplication>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "../../Define/DllExportMacro.hpp"

export module SearchImage;


export namespace ArtifactCore
{
LIBRARY_DLL_API cv::Mat loadImageFromExePath(const QString& image_filename, int target_cv_format, bool recursive = true)
 {
  QString appDirPath = QCoreApplication::applicationDirPath();
  qDebug() << "アプリケーションディレクトリ:" << appDirPath;

  // 画像ファイルを検索
  // QDirIterator を使用して、指定されたディレクトリ（とサブディレクトリ）内のファイルを探索します。
  QDirIterator it(appDirPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

  QString foundPath;
  while (it.hasNext()) {
   QString currentPath = it.next();
   QFileInfo fileInfo(currentPath);

   // ファイルであること、およびファイル名が一致することを確認（大文字小文字を区別しない）
   if (fileInfo.isFile() && fileInfo.fileName().toLower() == image_filename.toLower()) {
	foundPath = currentPath;
	break; // 見つかったらループを終了
   }
  }

  if (foundPath.isEmpty()) {
   qWarning() << "画像が見つかりませんでした:" << image_filename;
   return cv::Mat(); // 画像が見つからなかった場合は空のMatを返す
  }

  qDebug() << "画像が見つかりました:" << foundPath;

  // QImageで画像をロード
  QImage qImage(foundPath);
  if (qImage.isNull()) {
   qWarning() << "QImageのロードに失敗しました:" << foundPath;
   return cv::Mat();
  }

  // QImageのフォーマットを cv::Mat に扱いやすい Format_ARGB32 に変換します。
  // QImage::Format_ARGB32 は、通常リトルエンディアンシステムで 0xAARRGGBB の形式で格納され、
  // これはメモリ上ではバイト順が B, G, R, A となります。
  // そのため、cv::Mat の CV_8UC4 に直接マップすると、BGRA 順の画像として適切に解釈されます。
  if (qImage.format() != QImage::Format_ARGB32) {
   qImage = qImage.convertToFormat(QImage::Format_ARGB32);
   if (qImage.isNull()) {
	qWarning() << "QImageからFormat_ARGB32への変換に失敗しました。";
	return cv::Mat();
   }
  }

  // QImageの生データポインタとストライドを使って cv::Mat を構築します。
  // QImage::bits() は画像データの先頭ポインタ、bytesPerLine() は1行あたりのバイト数です。
  // CV_8UC4 (符号なし8ビット、4チャンネル) で直接 QImage のデータにアクセスします。
  cv::Mat cvMat_8UC4(qImage.height(), qImage.width(), CV_8UC4, (void*)qImage.bits(), qImage.bytesPerLine());

  cv::Mat finalCvMat;

  // 目標のOpenCVフォーマット (CV_32FC4を想定) に変換します。
  // CV_8UC4 (0-255の範囲) から CV_32FC4 (0.0-1.0の範囲) へのスケーリングと型変換を同時に行います。
  if (cvMat_8UC4.type() != target_cv_format) {
   cvMat_8UC4.convertTo(finalCvMat, target_cv_format, 1.0 / 255.0); // 0-255 -> 0.0-1.0
  }
  else {
   // すでに目標フォーマットの場合、QImageの内部データへの参照を避けるためにコピーを作成します。
   cvMat_8UC4.copyTo(finalCvMat);
  }

  // QImage::Format_ARGB32 から CV_8UC4 へは論理的にBGRA順になるため、
  // その後の convertTo(CV_32FC4) もBGRA順を維持します。
  // したがって、cv::cvtColor による明示的なチャネルスワップは不要です。

  return finalCvMat;
 }

LIBRARY_DLL_API cv::Mat findAndLoadImageInAppDir(const QString& image_filename, int target_cv_format)
{
 QString appDirPath = QCoreApplication::applicationDirPath();
 qDebug() << "アプリケーションディレクトリ:" << appDirPath;
 qDebug() << "アプリケーションディレクトリ以下のすべてのファイルから '" << image_filename << "' を検索中...";

 // QDirIterator を使用して、アプリケーションディレクトリとそのすべてのサブディレクトリ内のファイルを探索します。
 // QDir::Files と QDir::NoDotAndDotDot を指定し、QDirIterator::Subdirectories で再帰検索を有効にします。
 QDirIterator it(appDirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

 QString foundPath;
 while (it.hasNext()) {
  QString currentPath = it.next();
  QFileInfo fileInfo(currentPath);

  // ファイル名が指定された image_filename と一致することを確認（大文字小文字を区別しない）
  if (fileInfo.fileName().toLower() == image_filename.toLower()) {
   foundPath = currentPath;
   break; // 見つかったらループを終了
  }
 }

 if (foundPath.isEmpty()) {
  qWarning() << "target image'" << image_filename << "' はアプリケーションディレクトリ以下で見つかりませんでした。";
  return cv::Mat(); // 画像が見つからなかった場合は空のMatを返す
 }

 qDebug() << "image founded:" << foundPath;

 cv::Mat loadedCvMat = cv::imread(foundPath.toStdString());
 if (loadedCvMat.empty()) {
  qWarning() << "OpenCVでの画像ロードに失敗しました:" << foundPath;
  return cv::Mat();
 }

 cv::Mat intermediateCvMat;

 // 読み込んだMatのチャンネル数を確認し、必要であれば4チャンネルに変換
 if (loadedCvMat.channels() == 3) {
  // 3チャンネル (BGR) の場合、4チャンネル (BGRA) に変換（アルファチャンネルを1.0で埋める）
  cv::cvtColor(loadedCvMat, intermediateCvMat, cv::COLOR_BGR2BGRA);
 }
 else if (loadedCvMat.channels() == 4) {
  // 既に4チャンネル (BGRA) の場合
  loadedCvMat.copyTo(intermediateCvMat); // 生データを直接使わずコピーする方が安全
 }
 else {
  qWarning() << "サポートされていないチャンネル数の画像です (OpenCV):" << loadedCvMat.channels();
  return cv::Mat();
 }

 cv::Mat finalCvMat;

 // 目標のOpenCVフォーマット (CV_32FC4を想定) に変換します。
 // CV_8UC4 (0-255の範囲) から CV_32FC4 (0.0-1.0の範囲) へとスケーリングと型変換を同時に行います。
 if (intermediateCvMat.type() != target_cv_format) {
  // convertToのscale引数により、0-255の整数値が0.0-1.0の浮動小数点数に正規化されます。
  intermediateCvMat.convertTo(finalCvMat, target_cv_format, 1.0 / 255.0);
 }
 else {
  // すでに目標フォーマットの場合
  intermediateCvMat.copyTo(finalCvMat);
 }

 // OpenCVの cv::imread は通常 BGR/BGRA 順で読み込み、
 // convertTo もその順序を維持します。
 // Diligent EngineのTEX_FORMAT_RGBA32_FLOATは内部的にRGBAとして扱われますが、
 // シェーダーでID.xyアクセスを行う際に、CPU側のBGRA順と合うように調整します。
 // この関数からはBGRA順のCV_32FC4が返されるので、シェーダー側の解釈に注意してください。

 return finalCvMat;
}













};