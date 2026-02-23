module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.FinalUtilitiesDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// Logger Description
// ============================================================================

class LoggerDescription : public IDescribable {
public:
    QString className() const override { return "Logger"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Centralized logging system with levels, categories, and output targets.",
            "レベル、カテゴリ、出力ターゲットを持つ集中ロギングシステム。",
            "具有级别、类别和输出目标的集中日志系统。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"logLevel", loc("Minimum level to log", "ログの最小レベル", "日志最小级别"), "LogLevel", "Info"},
            {"logToFile", loc("Write to log file", "ログファイルに書き込み", "写入日志文件"), "bool", "true"},
            {"logToConsole", loc("Print to console", "コンソールに出力", "输出到控制台"), "bool", "true"},
            {"maxFileSize", loc("Max log file size (MB)", "最大ログファイルサイズ（MB）", "最大日志文件大小（MB）"), "int", "10"},
            {"maxFileCount", loc("Number of log files to keep", "保持するログファイル数", "保留的日志文件数"), "int", "5"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"debug", loc("Log debug message", "デバッグメッセージをログ", "记录调试消息"), 
             "void", {"QString"}, {"message"}},
            {"info", loc("Log info message", "情報メッセージをログ", "记录信息消息"), 
             "void", {"QString"}, {"message"}},
            {"warning", loc("Log warning message", "警告メッセージをログ", "记录警告消息"), 
             "void", {"QString"}, {"message"}},
            {"error", loc("Log error message", "エラーメッセージをログ", "记录错误消息"), 
             "void", {"QString"}, {"message"}},
            {"clear", loc("Clear log file", "ログファイルをクリア", "清除日志文件"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AuditLog", "PerformanceMonitor"};
    }
};

// ============================================================================
// PerformanceMonitor Description
// ============================================================================

class PerformanceMonitorDescription : public IDescribable {
public:
    QString className() const override { return "PerformanceMonitor"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Real-time performance monitoring for CPU, GPU, and memory usage.",
            "CPU、GPU、メモリ使用量のリアルタイムパフォーマンス監視。",
            "实时监控CPU、GPU和内存使用情况的性能监视器。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"cpuUsage", loc("Current CPU usage (%)", "現在のCPU使用率（%）", "当前CPU使用率（%）"), "float"},
            {"gpuUsage", loc("Current GPU usage (%)", "現在のGPU使用率（%）", "当前GPU使用率（%）"), "float"},
            {"memoryUsage", loc("Current memory usage (MB)", "現在のメモリ使用量（MB）", "当前内存使用量（MB）"), "float"},
            {"fps", loc("Frames per second", "毎秒フレーム数", "每秒帧数"), "float"},
            {"frameTime", loc("Frame render time (ms)", "フレームレンダリング時間（ms）", "帧渲染时间（ms）"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"startProfiling", loc("Begin performance profiling", "パフォーマンスプロファイリングを開始", "开始性能分析"), 
             "void", {"QString"}, {"sessionName"}},
            {"stopProfiling", loc("End profiling session", "プロファイリングセッションを終了", "结束分析会話"), 
             "ProfileResult"},
            {"enableGPUProfiling", loc("Enable GPU timing", "GPUタイミングを有効化", "启用GPU计时"), 
             "void", {"bool"}, {"enabled"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Logger", "ArtifactFrameCache"};
    }
};

// ============================================================================
// Preferences Description
// ============================================================================

class PreferencesDescription : public IDescribable {
public:
    QString className() const override { return "Preferences"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Application-wide user preferences and settings storage.",
            "アプリケーション全体のユーザー設定と設定ストレージ。",
            "应用程序范围的用户首选项和设置存储。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"autoSave", loc("Enable auto-save", "自動保存を有効化", "启用自动保存"), "bool", "true"},
            {"autoSaveInterval", loc("Auto-save interval (minutes)", "自動保存間隔（分）", "自动保存间隔（分钟）"), "int", "5"},
            {"undoLimit", loc("Maximum undo history", "最大アンドゥ履歴", "最大撤销历史"), "int", "100"},
            {"previewQuality", loc("Preview resolution scale", "プレビュー解像度スケール", "预览分辨率缩放"), "float", "1.0"},
            {"defaultDuration", loc("Default clip duration (seconds)", "デフォルトクリップ長（秒）", "默认片段时长（秒）"), "float", "5.0"},
            {"language", loc("Interface language", "インターフェース言語", "界面语言"), "QString", "en"},
            {"theme", loc("UI theme name", "UIテーマ名", "UI主题名称"), "QString", "dark"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"load", loc("Load preferences from disk", "設定をディスクから読み込み", "从磁盘加载首选项"), "void"},
            {"save", loc("Save preferences to disk", "設定をディスクに保存", "将首选项保存到磁盘"), "void"},
            {"reset", loc("Reset to defaults", "デフォルトにリセット", "重置为默认值"), "void"},
            {"setValue", loc("Set preference value", "設定値を設定", "设置首选项值"), 
             "void", {"QString", "QVariant"}, {"key", "value"}},
            {"value", loc("Get preference value", "設定値を取得", "获取首选项值"), 
             "QVariant", {"QString", "QVariant"}, {"key", "defaultValue"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ProjectSettings", "ArtifactProject"};
    }
};

// ============================================================================
// ThreadPool Description
// ============================================================================

class ThreadPoolDescription : public IDescribable {
public:
    QString className() const override { return "ThreadPool"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages worker threads for parallel task execution.",
            "並列タスク実行のためのワーカースレッドを管理します。",
            "管理用于并行任务执行的工作线程。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"threadCount", loc("Number of worker threads", "ワーカースレッド数", "工作线程数"), "int"},
            {"activeTasks", loc("Currently running tasks", "現在実行中のタスク", "当前运行的任务"), "int"},
            {"pendingTasks", loc("Tasks waiting in queue", "キューで待機中のタスク", "队列中等待的任务"), "int"},
            {"maxTasks", loc("Maximum queued tasks", "最大キュータスク数", "最大排队任务数"), "int", "100"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"submit", loc("Submit task for execution", "タスクを実行のために送信", "提交任务执行"), 
             "QFuture<T>", {"std::function<T()>"}, {"task"}},
            {"waitForDone", loc("Wait for all tasks to complete", "全タスクの完了を待機", "等待所有任务完成"), "void"},
            {"clear", loc("Cancel all pending tasks", "全保留タスクをキャンセル", "取消所有待处理任务"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactRenderController", "BackgroundTask"};
    }
};

// ============================================================================
// BackgroundTask Description
// ============================================================================

class BackgroundTaskDescription : public IDescribable {
public:
    QString className() const override { return "BackgroundTask"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for long-running operations with progress reporting.",
            "進捗報告付きの長時間実行操作の基底クラス。",
            "带有进度报告的长时间运行操作的基类。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Task display name", "タスク表示名", "任务显示名称"), "QString"},
            {"progress", loc("Current progress 0-100", "現在の進捗 0-100", "当前进度0-100"), "int"},
            {"status", loc("Task status (Running, Paused, Done)", "タスクステータス（実行中、一時停止、完了）", "任务状态（运行中、已暂停、已完成）"), "TaskStatus"},
            {"canCancel", loc("Whether task can be cancelled", "タスクがキャンセル可能か", "任务是否可取消"), "bool", "true"},
            {"canPause", loc("Whether task can be paused", "タスクが一時停止可能か", "任务是否可暂停"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"start", loc("Begin task execution", "タスク実行を開始", "开始任务执行"), "void"},
            {"pause", loc("Pause task", "タスクを一時停止", "暂停任务"), "void"},
            {"resume", loc("Resume paused task", "一時停止したタスクを再開", "恢复暂停的任务"), "void"},
            {"cancel", loc("Cancel task execution", "タスク実行をキャンセル", "取消任务执行"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ThreadPool", "ArtifactRenderQueue"};
    }
};

// ============================================================================
// NetworkManager Description
// ============================================================================

class NetworkManagerDescription : public IDescribable {
public:
    QString className() const override { return "NetworkManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles network operations for cloud sync and online features.",
            "クラウド同期とオンライン機能のためのネットワーク操作を処理します。",
            "处理用于云同步和在线功能的网络操作。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"isConnected", loc("Network connectivity status", "ネットワーク接続状態", "网络连接状态"), "bool"},
            {"proxyEnabled", loc("Use proxy server", "プロキシサーバーを使用", "使用代理服务器"), "bool", "false"},
            {"proxyAddress", loc("Proxy server address", "プロキシサーバーアドレス", "代理服务器地址"), "QString"},
            {"timeout", loc("Request timeout (seconds)", "リクエストタイムアウト（秒）", "请求超时（秒）"), "int", "30"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"download", loc("Download file from URL", "URLからファイルをダウンロード", "从URL下载文件"), 
             "BackgroundTask*", {"QString", "QString"}, {"url", "destination"}},
            {"upload", loc("Upload file to URL", "ファイルをURLにアップロード", "将文件上传到URL"), 
             "BackgroundTask*", {"QString", "QString"}, {"filePath", "url"}},
            {"checkForUpdates", loc("Check for application updates", "アプリケーション更新を確認", "检查应用程序更新"), 
             "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"BackgroundTask", "CloudSync"};
    }
};

// ============================================================================
// RecentFiles Description
// ============================================================================

class RecentFilesDescription : public IDescribable {
public:
    QString className() const override { return "RecentFiles"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages list of recently opened projects and files.",
            "最近開いたプロジェクトとファイルのリストを管理します。",
            "管理最近打开的项目和文件列表。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"fileCount", loc("Number of recent files", "最近使ったファイル数", "最近文件数"), "int"},
            {"maxFiles", loc("Maximum files to store", "保存する最大ファイル数", "存储的最大文件数"), "int", "10"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addFile", loc("Add file to recent list", "ファイルを最近使ったリストに追加", "将文件添加到最近列表"), 
             "void", {"QString"}, {"filePath"}},
            {"removeFile", loc("Remove file from list", "リストからファイルを削除", "从列表中移除文件"), 
             "void", {"QString"}, {"filePath"}},
            {"clear", loc("Clear all recent files", "全最近使ったファイルをクリア", "清除所有最近文件"), "void"},
            {"getFiles", loc("Get list of recent files", "最近使ったファイル一覧を取得", "获取最近文件列表"), 
             "QStringList"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Preferences", "ArtifactProject"};
    }
};

// ============================================================================
// Notification Description
// ============================================================================

class NotificationDescription : public IDescribable {
public:
    QString className() const override { return "Notification"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "System for displaying user notifications and alerts.",
            "ユーザー通知とアラートを表示するシステム。",
            "用于显示用户通知和警报的系统。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"title", loc("Notification title", "通知タイトル", "通知标题"), "QString"},
            {"message", loc("Notification content", "通知内容", "通知内容"), "QString"},
            {"type", loc("Info, Warning, Error, or Success", "情報、警告、エラー、または成功", "信息、警告、错误或成功"), "NotificationType"},
            {"duration", loc("Display duration (ms)", "表示時間（ms）", "显示时长（ms）"), "int", "5000"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"show", loc("Display notification", "通知を表示", "显示通知"), "void"},
            {"dismiss", loc("Close notification", "通知を閉じる", "关闭通知"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Logger", "RenderDialog"};
    }
};

// ============================================================================
// BlendModes Description
// ============================================================================

class BlendModesDescription : public IDescribable {
public:
    QString className() const override { return "BlendModes"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Collection of blend mode algorithms for layer compositing.",
            "レイヤーコンポジット用のブレンドモードアルゴリズムのコレクション。",
            "用于图层合成的混合模式算法集合。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "BlendModes provides static methods for all standard compositing operations "
            "including Normal, Multiply, Screen, Overlay, Add, Subtract, and many more. "
            "Used internally by the render system for layer blending.",
            "BlendModesは、Normal、Multiply、Screen、Overlay、Add、Subtractなど、"
            "すべての標準コンポジット操作の静的メソッドを提供します。"
            "レンダーシステムがレイヤーブレンドのために内部的に使用します。",
            "BlendModes为所有标准合成操作提供静态方法，"
            "包括Normal、Multiply、Screen、Overlay、Add、Subtract等。"
            "渲染系统内部用于图层混合。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"normal", loc("Standard alpha blending", "標準アルファブレンド", "标准Alpha混合"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}},
            {"multiply", loc("Multiply blend mode", "乗算ブレンドモード", "正片叠底混合模式"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}},
            {"screen", loc("Screen blend mode", "スクリーンブレンドモード", "滤色混合模式"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}},
            {"overlay", loc("Overlay blend mode", "オーバーレイブレンドモード", "叠加混合模式"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}},
            {"add", loc("Additive blend mode", "加算ブレンドモード", "相加混合模式"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}},
            {"difference", loc("Difference blend mode", "差分ブレンドモード", "差值混合模式"), 
             "QColor", {"QColor", "QColor", "float"}, {"src", "dst", "opacity"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactAbstractEffect"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<LoggerDescription> _reg_Logger("Logger");
static AutoRegisterDescribable<PerformanceMonitorDescription> _reg_PerformanceMonitor("PerformanceMonitor");
static AutoRegisterDescribable<PreferencesDescription> _reg_Preferences("Preferences");
static AutoRegisterDescribable<ThreadPoolDescription> _reg_ThreadPool("ThreadPool");
static AutoRegisterDescribable<BackgroundTaskDescription> _reg_BackgroundTask("BackgroundTask");
static AutoRegisterDescribable<NetworkManagerDescription> _reg_NetworkManager("NetworkManager");
static AutoRegisterDescribable<RecentFilesDescription> _reg_RecentFiles("RecentFiles");
static AutoRegisterDescribable<NotificationDescription> _reg_Notification("Notification");
static AutoRegisterDescribable<BlendModesDescription> _reg_BlendModes("BlendModes");

} // namespace ArtifactCore