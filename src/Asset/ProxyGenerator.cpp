module;

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QUuid>

export module Asset.ProxyGenerator;

import Asset.ProxyGenerator;
import Asset.ProxySettings;

namespace ArtifactCore {

    // Impl クラス宣言
    class ProxyGenerator::Impl {
    public:
        // タスクキュー
        QMap<QString, ProxyTask> tasks_;
        
        // 処理中のタスク数
        int processingCount_ = 0;
        
        // キャンセルされたタスクID
        QSet<QString> cancelledTasks_;
        
        // ミューテックス
        mutable QMutex mutex_;
    };

    // ProxyGenerator コンストラクタ
    ProxyGenerator::ProxyGenerator(QObject* parent)
        : QObject(parent)
        , impl_(std::make_unique<Impl>())
    {
        // デフォルトFFmpegパス
        ffmpegPath_ = "ffmpeg";
        
        // デフォルト一時ディレクトリ
        tempDirectory_ = QDir::tempPath() + "/artifactcore_proxy";
        
        // 一時ディレクトリを作成
        QDir().mkpath(tempDirectory_);
    }

    // デストラクタ
    ProxyGenerator::~ProxyGenerator() = default;

    // FFmpegパスを設定
    void ProxyGenerator::setFFmpegPath(const QString& path)
    {
        ffmpegPath_ = path;
    }

    // 一時ディレクトリを設定
    void ProxyGenerator::setTempDirectory(const QString& path)
    {
        tempDirectory_ = path;
        QDir().mkpath(tempDirectory_);
    }

    // ハードウェアエンコードを有効にするか設定
    void ProxyGenerator::setHardwareAccelerationEnabled(bool enabled)
    {
        hardwareAcceleration_ = enabled;
    }

    // 同時実行タスク数を設定
    void ProxyGenerator::setMaxConcurrentTasks(int count)
    {
        maxConcurrentTasks_ = qMax(1, count);
    }

    // 優先度を設定
    void ProxyGenerator::setPriority(QThread::Priority priority)
    {
        priority_ = priority;
    }

    // プロキシ動画を生成（同期）
    bool ProxyGenerator::generateSync(const QString& sourcePath, const ProxySettings& settings)
    {
        // 出力パス生成
        QString outputPath = settings.generateProxyPath(sourcePath);
        
        // ターゲット解像度計算
        VideoInfo info = getVideoInfo(sourcePath);
        QSize targetSize = settings.calculateProxySize(info.width, info.height);
        
        return generateSync(sourcePath, outputPath, targetSize, 
                          calculateRecommendedBitrate(targetSize.width(), targetSize.height()),
                          static_cast<int>(info.frameRate));
    }

    // プロキシ動画を生成（同期、カスタム設定）
    bool ProxyGenerator::generateSync(const QString& sourcePath,
                                     const QString& outputPath,
                                     const QSize& targetSize,
                                     int bitrate,
                                     int framerate)
    {
        ProxyTask task;
        task.sourcePath = sourcePath;
        task.proxyPath = outputPath;
        task.targetSize = targetSize;
        task.targetBitrate = bitrate;
        task.targetFramerate = framerate;
        task.useHardwareAcceleration = hardwareAcceleration_;
        
        // 出力ディレクトリを作成
        QFileInfo fileInfo(outputPath);
        QDir().mkpath(fileInfo.absolutePath());
        
        // FFmpeg引数を構築
        QString args = buildFFmpegArgs(task);
        
        // FFmpegを実行
        bool success = executeFFmpeg(args.split(" "), "", [this](float progress) {
            Q_UNUSED(progress);
        });
        
        return success;
    }

    // プロキシ動画を生成（非同期）
    QString ProxyGenerator::generateAsync(const QString& sourcePath, const ProxySettings& settings)
    {
        // 出力パス生成
        QString outputPath = settings.generateProxyPath(sourcePath);
        
        // ターゲット解像度計算
        VideoInfo info = getVideoInfo(sourcePath);
        QSize targetSize = settings.calculateProxySize(info.width, info.height);
        
        return generateAsync(sourcePath, outputPath, targetSize,
                           calculateRecommendedBitrate(targetSize.width(), targetSize.height()),
                           static_cast<int>(info.frameRate));
    }

    // プロキシ動画を生成（非同期、カスタム設定）
    QString ProxyGenerator::generateAsync(const QString& sourcePath,
                                         const QString& outputPath,
                                         const QSize& targetSize,
                                         int bitrate,
                                         int framerate)
    {
        QMutexLocker locker(&impl_->mutex_);
        
        // タスクID生成
        QString taskId = generateTaskId();
        
        // タスク作成
        ProxyTask task;
        task.id = taskId;
        task.sourcePath = sourcePath;
        task.proxyPath = outputPath;
        task.targetSize = targetSize;
        task.targetBitrate = bitrate;
        task.targetFramerate = framerate;
        task.useHardwareAcceleration = hardwareAcceleration_;
        task.state = ProxyTaskState::Pending;
        
        impl_->tasks_[taskId] = task;
        
        // 非同期で処理を開始（Qtのシグナル/スロットを使用）
        QMetaObject::invokeMethod(this, "processTaskQueue", Qt::QueuedConnection);
        
        emit taskStarted(taskId);
        emit queueChanged(getAllTasks().count() - impl_->processingCount_, impl_->processingCount_);
        
        return taskId;
    }

    // タスクをキャンセル
    void ProxyGenerator::cancelTask(const QString& taskId)
    {
        QMutexLocker locker(&impl_->mutex_);
        
        if (impl_->tasks_.contains(taskId)) {
            impl_->cancelledTasks_.insert(taskId);
            impl_->tasks_[taskId].state = ProxyTaskState::Cancelled;
        }
    }

    // すべてのタスクをキャンセル
    void ProxyGenerator::cancelAllTasks()
    {
        QMutexLocker locker(&impl_->mutex_);
        
        for (auto& task : impl_->tasks_) {
            if (task.state == ProxyTaskState::Pending || 
                task.state == ProxyTaskState::Processing) {
                task.state = ProxyTaskState::Cancelled;
            }
        }
        impl_->cancelledTasks_.clear();
    }

    // タスクを削除
    void ProxyGenerator::removeTask(const QString& taskId)
    {
        QMutexLocker locker(&impl_->mutex_);
        impl_->tasks_.remove(taskId);
    }

    // タスク一覧を取得
    QList<ProxyTask> ProxyGenerator::getAllTasks() const
    {
        QMutexLocker locker(&impl_->mutex_);
        return impl_->tasks_.values();
    }

    // タスクを取得
    ProxyTask ProxyGenerator::getTask(const QString& taskId) const
    {
        QMutexLocker locker(&impl_->mutex_);
        return impl_->tasks_.value(taskId);
    }

    // タスクの状態を取得
    ProxyTaskState ProxyGenerator::getTaskState(const QString& taskId) const
    {
        QMutexLocker locker(&impl_->mutex_);
        return impl_->tasks_.value(taskId).state;
    }

    // タスクの進捗を取得
    float ProxyGenerator::getTaskProgress(const QString& taskId) const
    {
        QMutexLocker locker(&impl_->mutex_);
        return impl_->tasks_.value(taskId).progress;
    }

    // 動画情報を取得
    ProxyGenerator::VideoInfo ProxyGenerator::getVideoInfo(const QString& videoPath) const
    {
        VideoInfo info;
        
        QProcess process;
        QStringList args;
        args << "-i" << videoPath << "-hide_banner";
        
        process.start(ffmpegPath_, args);
        process.waitForFinished(5000);
        
        QString output = QString::fromUtf8(process.readAllStandardError());
        
        // 幅と高さ
        QRegularExpression resRegex(R"((\d{2,5})x(\d{2,5}))");
        QRegularExpressionMatch match = resRegex.match(output);
        if (match.hasMatch()) {
            info.width = match.captured(1).toInt();
            info.height = match.capturedMatchAt(0).captured(2).toInt();
        }
        
        // フレームレート
        QRegularExpression fpsRegex(R"((\d+(?:\.\d+)?)\s*fps)");
        match = fpsRegex.match(output);
        if (match.hasMatch()) {
            info.frameRate = match.captured(1).toDouble();
        }
        
        // ビットレート
        QRegularExpression brRegex(R"((\d+)\s*kb/s)");
        match = brRegex.match(output);
        if (match.hasMatch()) {
            info.bitrate = match.captured(1).toInt();
        }
        
        // コーデック
        QRegularExpression codecRegex(R"(Video:\s*(\w+))");
        match = codecRegex.match(output);
        if (match.hasMatch()) {
            info.codec = match.captured(1);
        }
        
        // ファイルサイズ
        QFileInfo fileInfo(videoPath);
        info.fileSize = fileInfo.size();
        
        // ストリーム判定
        info.hasVideo = output.contains("Video:");
        info.hasAudio = output.contains("Audio:");
        
        return info;
    }

    // FFmpegが利用可能か確認
    bool ProxyGenerator::isFFmpegAvailable() const
    {
        QProcess process;
        process.start(ffmpegPath_, {"-version"});
        return process.waitForFinished(3000) && process.exitCode() == 0;
    }

    // 推奨ビットレートを計算
    int ProxyGenerator::calculateRecommendedBitrate(int width, int height) const
    {
        int pixels = width * height;
        
        // 解像度に基づく推奨ビットレート（kbps）
        if (pixels >= 3840 * 2160) return 20000;      // 4K
        if (pixels >= 2560 * 1440) return 10000;      // 1440p
        if (pixels >= 1920 * 1080) return 5000;       // 1080p
        if (pixels >= 1280 * 720) return 2500;        // 720p
        if (pixels >= 854 * 480) return 1500;         // 480p
        return 1000;                                   // 以下
    }

    // タスクIDを生成
    QString ProxyGenerator::generateTaskId()
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    // FFmpeg引数を構築
    QString ProxyGenerator::buildFFmpegArgs(const ProxyTask& task) const
    {
        QStringList args;
        
        // 入力
        args << "-i" << task.sourcePath;
        
        // キャンセルシグナル監視
        args << "-nostdin";
        
        // ビデオ编码
        if (task.useHardwareAcceleration) {
            // NVENC優先
            args << "-c:v" << "h264_nvenc";
        } else {
            args << "-c:v" << "libx264";
        }
        
        // プロファイル（互換性のためbaseline）
        args << "-profile:v" << "baseline";
        
        // ピクセルフォーマット
        args << "-pix_fmt" << "yuv420p";
        
        // 解像度
        if (task.targetSize.isValid()) {
            args << "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2")
                    .arg(task.targetSize.width())
                    .arg(task.targetSize.height());
        }
        
        // ビットレート
        if (task.targetBitrate > 0) {
            args << "-b:v" << QString("%1k").arg(task.targetBitrate);
        } else {
            // CRFモード（品質ベース）
            args << "-crf" << "23";
        }
        
        // フレームレート
        if (task.targetFramerate > 0) {
            args << "-r" << QString::number(task.targetFramerate);
        }
        
        //  аудио（プロキシには音声不要の場合が多い）
        args << "-an";
        
        // 出力
        args << "-y" << task.proxyPath;
        
        return args.join(" ");
    }

    // FFmpegを実行
    bool ProxyGenerator::executeFFmpeg(const QStringList& args, const QString& taskId,
                                       std::function<void(float)> progressCallback)
    {
        QProcess process;
        
        // 進捗監視のためのタイマー
        QObject::connect(&process, &QProcess::readyReadStandardError, [&]() {
            QString output = QString::fromUtf8(process.readAllStandardError());
            
            // 進捗解析
            QRegularExpression timeRegex(R"(time=(\d{2}):(\d{2}):(\d{2})\.(\d{2}))");
            QRegularExpressionMatch match = timeRegex.match(output);
            
            if (match.hasMatch()) {
                int hours = match.captured(1).toInt();
                int minutes = match.captured(2).toInt();
                int seconds = match.captured(3).toInt();
                int centisecs = match.captured(4).toInt();
                
                int currentTimeMs = (hours * 3600 + minutes * 60 + seconds) * 1000 + centisecs * 10;
                
                // タスクの進捗を更新
                if (!taskId.isEmpty()) {
                    QMutexLocker locker(&impl_->mutex_);
                    if (impl_->tasks_.contains(taskId)) {
                        // ここで進捗率を計算するには総時間が必要
                        impl_->tasks_[taskId].progress = 0.5f; // 仮
                    }
                }
                
                if (progressCallback) {
                    progressCallback(0.5f);
                }
            }
        });
        
        // プロセス開始
        process.start(ffmpegPath_, args);
        
        // 完了待機
        bool finished = process.waitForFinished(-1); // 無限待機
        
        if (!finished) {
            process.kill();
            return false;
        }
        
        int exitCode = process.exitCode();
        
        if (exitCode != 0) {
            QString error = QString::fromUtf8(process.readAllStandardError());
            qWarning() << "FFmpeg error:" << error;
            return false;
        }
        
        if (progressCallback) {
            progressCallback(1.0f);
        }
        
        return true;
    }

    // タスクキューを処理
    void ProxyGenerator::processTaskQueue()
    {
        QMutexLocker locker(&impl_->mutex_);
        
        // 処理中のタスク数が上限に達している場合はスキップ
        if (impl_->processingCount_ >= maxConcurrentTasks_) {
            return;
        }
        
        // 待機中のタスクを検索
        for (auto& task : impl_->tasks_) {
            if (task.state == ProxyTaskState::Pending) {
                // キャンセルされていないか確認
                if (impl_->cancelledTasks_.contains(task.id)) {
                    task.state = ProxyTaskState::Cancelled;
                    continue;
                }
                
                // 処理中に変更
                task.state = ProxyTaskState::Processing;
                task.startTime = QDateTime::currentMSecsSinceEpoch();
                impl_->processingCount_++;
                
                QString taskId = task.id;
                QString sourcePath = task.sourcePath;
                QString proxyPath = task.proxyPath;
                QSize targetSize = task.targetSize;
                int bitrate = task.targetBitrate;
                int framerate = task.targetFramerate;
                bool hwAccel = task.useHardwareAcceleration;
                
                locker.unlock();
                
                // 非同期で実行
                QtConcurrent::run([this, taskId, sourcePath, proxyPath, targetSize, bitrate, framerate, hwAccel]() {
                    ProxyTask task;
                    task.sourcePath = sourcePath;
                    task.proxyPath = proxyPath;
                    task.targetSize = targetSize;
                    task.targetBitrate = bitrate;
                    task.targetFramerate = framerate;
                    task.useHardwareAcceleration = hwAccel;
                    
                    // 出力ディレクトリを作成
                    QFileInfo fileInfo(proxyPath);
                    QDir().mkpath(fileInfo.absolutePath());
                    
                    // FFmpeg引数を構築
                    QString argsStr = buildFFmpegArgs(task);
                    
                    // FFmpegを実行
                    bool success = executeFFmpeg(argsStr.split(" "), taskId, [this, taskId](float progress) {
                        emit taskProgress(taskId, progress, "Encoding...");
                    });
                    
                    // 結果を更新
                    QMutexLocker locker(&impl_->mutex_);
                    if (impl_->tasks_.contains(taskId)) {
                        if (success) {
                            impl_->tasks_[taskId].state = ProxyTaskState::Completed;
                            impl_->tasks_[taskId].progress = 1.0f;
                        } else {
                            impl_->tasks_[taskId].state = ProxyTaskState::Failed;
                            impl_->tasks_[taskId].errorMessage = "FFmpeg encoding failed";
                        }
                        impl_->tasks_[taskId].endTime = QDateTime::currentMSecsSinceEpoch();
                    }
                    
                    impl_->processingCount_--;
                    
                    emit taskCompleted(taskId, success, impl_->tasks_[taskId].errorMessage);
                    emit queueChanged(getAllTasks().count() - impl_->processingCount_, impl_->processingCount_);
                    
                    // 次のタスクを処理
                    QMetaObject::invokeMethod(this, "processTaskQueue", Qt::QueuedConnection);
                });
                
                return;
            }
        }
    }

    // 解像度から名称を取得
    QString resolutionToName(int width, int height)
    {
        if (width >= 3840 || height >= 2160) return "4K";
        if (width >= 2560 || height >= 1440) return "1440p";
        if (width >= 1920 || height >= 1080) return "1080p";
        if (width >= 1280 || height >= 720) return "720p";
        if (width >= 854 || height >= 480) return "480p";
        return QString("%1x%2").arg(width).arg(height);
    }

    // ビットレートから品質ラベルを取得
    QString bitrateToQualityLabel(int bitrate)
    {
        if (bitrate >= 20000) return "High (4K)";
        if (bitrate >= 10000) return "High (1440p)";
        if (bitrate >= 5000) return "High (1080p)";
        if (bitrate >= 2500) return "Medium (720p)";
        if (bitrate >= 1500) return "Low (480p)";
        return "Very Low";
    }

} // namespace ArtifactCore
