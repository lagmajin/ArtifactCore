module;
#include <QString>
#include <QProcess>
#include <QFileInfo>
#include <QDir>

module Utils.ExplorerUtils;

import Utils.String.UniString;

namespace ArtifactCore {

void openInExplorer(const UniString& path, bool select) {
    QString qpath = path.toQString();
    QFileInfo info(qpath);
#if defined(_WIN32)
    QString arg;
    if (select && info.exists() && info.isFile()) {
        arg = "/select," + QDir::toNativeSeparators(qpath);
    } else {
        arg = QDir::toNativeSeparators(info.isDir() ? qpath : info.absolutePath());
    }
    QProcess::startDetached("explorer.exe", QStringList() << arg);
#elif defined(__APPLE__)
    QStringList args;
    if (select && info.exists()) {
        args << "-R" << qpath;
    } else {
        args << (info.isDir() ? qpath : info.absolutePath());
    }
    QProcess::startDetached("open", args);
#else // Linux/Unix
    // xdg-openはファイル選択未対応
    QString target = info.isDir() ? qpath : info.absolutePath();
    QProcess::startDetached("xdg-open", QStringList() << target);
#endif
}

} // namespace ArtifactCore
