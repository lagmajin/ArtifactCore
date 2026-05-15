export module Core.Thread.BackgroundTaskSamples;

import std;
import Core.Thread.BackgroundTaskRuntime;
import Core.Thread.BackgroundTaskWorkerPool;

namespace ArtifactCore {

// ============================================================
// Sample: Simple Background Task
// ============================================================

/// <summary>
/// 単純なバックグラウンドtaskのサンプル
/// </summary>
export class SimpleBackgroundTask : public IBackgroundTask {
public:
    SimpleBackgroundTask(QString name, int totalWorkItems)
        : name_(std::move(name))
        , totalWorkItems_(totalWorkItems) {}
    
    auto Execute(CancelToken& cancelToken, std::function<void(TaskProgress)> reportProgress) -> void override {
        for (int i = 0; i < totalWorkItems_; ++i) {
            // キャンセルチェック
            cancelToken.ThrowIfCancelled();
            
            // 実際の処理（サンプルではスリープ）
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // 進捗報告
            TaskProgress progress{
                .completed = static_cast<double>(i + 1),
                .total = static_cast<double>(totalWorkItems_),
                .message = QString("処理中... %1/%2").arg(i + 1).arg(totalWorkItems_)
            };
            reportProgress(progress);
        }
    }
    
    auto GetOptions() const -> TaskOptions override {
        return TaskOptions{
            .priority = TaskPriority::Normal,
            .maxConcurrency = 1,
            .allowsCancellation = true,
            .dependencies = {},
            .name = name_,
            .category = "sample"
        };
    }

private:
    QString name_;
    int totalWorkItems_;
};

// ============================================================
// Sample: Render Task
// ============================================================

/// <summary>
/// レンダリングtaskのサンプル
/// </summary>
export class RenderBackgroundTask : public IBackgroundTask {
public:
    struct RenderParams {
        int startFrame = 0;
        int endFrame = 100;
        QString outputPath;
        QString preset = "high";
    };
    
    explicit RenderBackgroundTask(RenderParams params)
        : params_(std::move(params)) {}
    
    auto Execute(CancelToken& cancelToken, std::function<void(TaskProgress)> reportProgress) -> void override {
        const int totalFrames = params_.endFrame - params_.startFrame + 1;
        
        for (int frame = params_.startFrame; frame <= params_.endFrame; ++frame) {
            // キャンセルチェック
            cancelToken.ThrowIfCancelled();
            
            // フレームレンダリング（サンプル）
            RenderFrame(frame);
            
            // 進捗報告
            const int completed = frame - params_.startFrame + 1;
            TaskProgress progress{
                .completed = static_cast<double>(completed),
                .total = static_cast<double>(totalFrames),
                .message = QString("レンダリング中... フレーム %1/%2").arg(frame).arg(params_.endFrame)
            };
            reportProgress(progress);
        }
    }
    
    auto GetOptions() const -> TaskOptions override {
        return TaskOptions{
            .priority = TaskPriority::High,
            .maxConcurrency = 1,  // レンダリングは逐次実行
            .allowsCancellation = true,
            .dependencies = {},
            .name = "Render Task",
            .category = "render"
        };
    }

private:
    void RenderFrame(int frame) {
        // 実際のレンダリング処理
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    RenderParams params_;
};

// ============================================================
// Sample: Proxy Generation Task
// ============================================================

/// <summary>
/// Proxy生成taskのサンプル
/// </summary>
export class ProxyGenerationTask : public IBackgroundTask {
public:
    struct ProxyParams {
        QString sourcePath;
        QString outputPath;
        QString resolution = "1280x720";
        QString codec = "h264";
    };
    
    explicit ProxyGenerationTask(ProxyParams params)
        : params_(std::move(params)) {}
    
    auto Execute(CancelToken& cancelToken, std::function<void(TaskProgress)> reportProgress) -> void override {
        // Proxy生成処理（サンプル）
        const int totalSteps = 10;
        
        for (int step = 0; step < totalSteps; ++step) {
            cancelToken.ThrowIfCancelled();
            
            // 実際のproxy生成処理
            GenerateProxyStep(step);
            
            TaskProgress progress{
                .completed = static_cast<double>(step + 1),
                .total = static_cast<double>(totalSteps),
                .message = QString("Proxy生成中... %1%").arg((step + 1) * 10)
            };
            reportProgress(progress);
        }
    }
    
    auto GetOptions() const -> TaskOptions override {
        return TaskOptions{
            .priority = TaskPriority::Normal,
            .maxConcurrency = 2,  // 並列実行可能
            .allowsCancellation = true,
            .dependencies = {},
            .name = "Proxy Generation",
            .category = "proxy"
        };
    }

private:
    void GenerateProxyStep(int step) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ProxyParams params_;
};

// ============================================================
// Sample: Analysis Task
// ============================================================

/// <summary>
/// 解析taskのサンプル（waveform生成、AI解析など）
/// </summary>
export class AnalysisTask : public IBackgroundTask {
public:
    enum class AnalysisType {
        Waveform,
        AIDetection,
        ColorAnalysis,
        MetadataExtraction
    };
    
    AnalysisTask(AnalysisType type, QString targetPath)
        : type_(type)
        , targetPath_(std::move(targetPath)) {}
    
    auto Execute(CancelToken& cancelToken, std::function<void(TaskProgress)> reportProgress) -> void override {
        const int totalSteps = 20;
        
        for (int step = 0; step < totalSteps; ++step) {
            cancelToken.ThrowIfCancelled();
            
            // 実際の解析処理
            AnalyzeStep(step);
            
            QString typeName = GetAnalysisTypeName(type_);
            TaskProgress progress{
                .completed = static_cast<double>(step + 1),
                .total = static_cast<double>(totalSteps),
                .message = QString("%1解析中... %2%").arg(typeName).arg((step + 1) * 5)
            };
            reportProgress(progress);
        }
    }
    
    auto GetOptions() const -> TaskOptions override {
        return TaskOptions{
            .priority = TaskPriority::Low,  // 低い優先度
            .maxConcurrency = 1,
            .allowsCancellation = true,
            .dependencies = {},
            .name = GetAnalysisTypeName(type_),
            .category = "analysis"
        };
    }

private:
    auto GetAnalysisTypeName(AnalysisType type) const -> QString {
        switch (type) {
            case AnalysisType::Waveform: return "Waveform";
            case AnalysisType::AIDetection: return "AI Detection";
            case AnalysisType::ColorAnalysis: return "Color Analysis";
            case AnalysisType::MetadataExtraction: return "Metadata";
            default: return "Unknown";
        }
    }
    
    void AnalyzeStep(int step) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    AnalysisType type_;
    QString targetPath_;
};

// ============================================================
// Usage Example
// ============================================================

/// <summary>
/// 使用例
/// 
/// ```cpp
/// // Worker poolを作成
/// BackgroundTaskWorkerPool::Config config;
/// config.maxWorkers = 4;
/// BackgroundTaskWorkerPool pool(config);
/// pool.Start();
/// 
/// // Taskをsubmit
/// auto task = std::make_shared<RenderBackgroundTask>(RenderParams{
///     .startFrame = 0,
///     .endFrame = 100,
///     .outputPath = "output.mp4"
/// });
/// 
/// TaskId taskId = pool.SubmitTask(task);
/// 
/// // 必要に応じてキャンセル
/// // pool.CancelTask(taskId);
/// 
/// // 完了後にシャットダウン
/// pool.Shutdown();
/// ```
/// </summary>
export inline void BackgroundTaskRuntimeUsageExample() {
    // 何もしない（ドキュメント用）
}

} // namespace ArtifactCore
