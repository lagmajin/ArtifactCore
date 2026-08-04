module;
#include <utility>
#include <QString>
#include <QObject>
#include <QDebug>
#include <QFileInfo>
#include <wobjectimpl.h>

module Video.PlaybackManager;

namespace ArtifactCore {

W_OBJECT_IMPL(PlaybackManager)

class PlaybackManager::Impl {
public:
    QString filePath_;
    bool opened_ = false;
    bool playing_ = false;
};

PlaybackManager::PlaybackManager(QObject* parent /*= nullptr*/)
    : QObject(parent)
    , impl_(new Impl())
{
}

PlaybackManager::~PlaybackManager()
{
    delete impl_;
    impl_ = nullptr;
}

bool PlaybackManager::open(const QString& filePath)
{
    if (!impl_) {
        return false;
    }

    const QFileInfo info(filePath);
    if (filePath.trimmed().isEmpty() || !info.exists() || !info.isFile()) {
        qWarning() << "PlaybackManager::open failed:" << filePath;
        impl_->filePath_.clear();
        impl_->opened_ = false;
        impl_->playing_ = false;
        return false;
    }

    impl_->filePath_ = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    impl_->opened_ = true;
    impl_->playing_ = false;
    qDebug() << "PlaybackManager opened:" << impl_->filePath_;
    return true;
}

void PlaybackManager::play()
{
    if (!impl_ || !impl_->opened_) {
        qWarning() << "PlaybackManager::play ignored: no opened file";
        return;
    }
    impl_->playing_ = true;
    qDebug() << "PlaybackManager play:" << impl_->filePath_;
}

void PlaybackManager::pause()
{
    if (!impl_ || !impl_->opened_) {
        return;
    }
    impl_->playing_ = false;
    qDebug() << "PlaybackManager pause:" << impl_->filePath_;
}

void PlaybackManager::stop()
{
    if (!impl_) {
        return;
    }
    impl_->playing_ = false;
    qDebug() << "PlaybackManager stop:" << impl_->filePath_;
}

} // namespace ArtifactCore
