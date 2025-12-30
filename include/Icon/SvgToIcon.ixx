module;
#include <QIcon>
#include <QPixmap>
#include <QtSVG/QSvgRenderer>
#include <QPainter>

#include "../Define/DllExportMacro.hpp"
export module Icon.SvgToIcon;

export namespace ArtifactCore
{
 LIBRARY_DLL_API QIcon svgToQIcon(const QString& svgPath, const QSize& size = QSize(32, 32))
 {
  QSvgRenderer renderer(svgPath);
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);  // ”wŒi‚ð“§–¾‚É

  QPainter painter(&pixmap);
  renderer.render(&painter);
  painter.end();

  return QIcon(pixmap);
 }
	
	

};