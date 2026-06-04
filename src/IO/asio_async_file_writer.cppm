module;
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

module asio_async_file_writer;

namespace ArtifactCore {

namespace {

WriteResult makeFailure(const QString& path, const QString& message)
{
    return WriteResult(false, path, 0, message);
}

WriteResult writeBytesToFile(const QString& filePath, const QByteArray& data)
{
    if (filePath.isEmpty()) {
        return makeFailure(filePath, QStringLiteral("File path is empty."));
    }

    QFileInfo info(filePath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return makeFailure(filePath, QStringLiteral("Failed to create parent directory: %1").arg(dir.absolutePath()));
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return makeFailure(filePath, QStringLiteral("Failed to open file for writing: %1").arg(file.errorString()));
    }

    const qint64 written = file.write(data);
    if (written != data.size()) {
        return makeFailure(filePath, QStringLiteral("Short write: %1 of %2 bytes.").arg(written).arg(data.size()));
    }

    if (!file.commit()) {
        return makeFailure(filePath, QStringLiteral("Failed to commit file: %1").arg(file.errorString()));
    }

    return WriteResult(true, filePath, static_cast<std::size_t>(written), {});
}

} // namespace

class AsioAsyncFileWriterManager::Impl {
private:
    boost::asio::io_context m_ioContext;
    boost::asio::thread_pool m_threadPool;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
    std::atomic<bool> m_stopping{false};
    AsioAsyncFileWriterManager* m_parent;

public:
    explicit Impl(int threadPoolSize, AsioAsyncFileWriterManager* parent);
    ~Impl();

    void enqueueBytes(const QString& filePath, QByteArray data, WriteCompletionCallback callback);
};

AsioAsyncFileWriterManager::Impl::Impl(int threadPoolSize, AsioAsyncFileWriterManager* parent)
    : m_threadPool(std::max(1, threadPoolSize))
    , m_workGuard(boost::asio::make_work_guard(m_ioContext))
    , m_parent(parent)
{
    qDebug() << "AsioAsyncFileWriterManager::Impl initialized with" << std::max(1, threadPoolSize) << "threads.";
}

AsioAsyncFileWriterManager::Impl::~Impl()
{
    m_stopping.store(true);
    m_workGuard.reset();
    m_ioContext.stop();
    m_threadPool.join();
    qDebug() << "AsioAsyncFileWriterManager::Impl shut down.";
}

void AsioAsyncFileWriterManager::Impl::enqueueBytes(const QString& filePath, QByteArray data, WriteCompletionCallback callback)
{
    if (m_stopping.load() || filePath.isEmpty()) {
        if (callback) {
            callback(makeFailure(filePath, QStringLiteral("Writer is stopping or file path is empty.")));
        }
        return;
    }

    boost::asio::post(m_threadPool, [this, filePath, data = std::move(data), callback = std::move(callback)]() mutable {
        if (m_stopping.load()) {
            if (callback) {
                callback(makeFailure(filePath, QStringLiteral("Writer is shutting down.")));
            }
            return;
        }

        const WriteResult result = writeBytesToFile(filePath, data);
        if (callback) {
            callback(result);
        }
        Q_UNUSED(m_parent);
    });
}

AsioAsyncFileWriterManager::AsioAsyncFileWriterManager()
    : QObject()
    , impl_(new Impl(std::thread::hardware_concurrency() > 0 ? static_cast<int>(std::thread::hardware_concurrency()) : 1, this))
{
}

AsioAsyncFileWriterManager::~AsioAsyncFileWriterManager()
{
    delete impl_;
    impl_ = nullptr;
}

void AsioAsyncFileWriterManager::writeFileAsync(const QString& filePath, const QByteArray& data, WriteCompletionCallback callback)
{
    if (!impl_) {
        if (callback) {
            callback(makeFailure(filePath, QStringLiteral("Writer manager is not initialized.")));
        }
        return;
    }
    impl_->enqueueBytes(filePath, data, std::move(callback));
}

void AsioAsyncFileWriterManager::writeFileAsync(const QString& filePath, const QVector<unsigned char>& data, WriteCompletionCallback callback)
{
    QByteArray bytes(reinterpret_cast<const char*>(data.constData()), static_cast<qsizetype>(data.size()));
    writeFileAsync(filePath, bytes, std::move(callback));
}

void AsioAsyncFileWriterManager::writeFileAsync(const QString& filePath, const std::vector<unsigned char>& data, WriteCompletionCallback callback)
{
    QByteArray bytes(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
    writeFileAsync(filePath, bytes, std::move(callback));
}

} // namespace ArtifactCore
