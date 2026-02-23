module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.ExportImportDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// VideoExporter Description
// ============================================================================

class VideoExporterDescription : public IDescribable {
public:
    QString className() const override { return "VideoExporter"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles video file export with codec and format options.",
            "コーデックとフォーマットオプションで動画ファイルエクスポートを処理します。",
            "使用编解码器和格式选项处理视频文件导出。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"format", loc("Output format (MP4, MOV, AVI, etc)", "出力フォーマット（MP4、MOV、AVI等）", "输出格式（MP4、MOV、AVI等）"), "QString", "MP4"},
            {"codec", loc("Video codec (H.264, H.265, ProRes, etc)", "動画コーデック（H.264、H.265、ProRes等）", "视频编解码器（H.264、H.265、ProRes等）"), "QString", "H.264"},
            {"quality", loc("Encoding quality (1-100)", "エンコード品質（1-100）", "编码质量（1-100）"), "int", "80", "1", "100"},
            {"bitrate", loc("Target bitrate (kbps)", "ターゲットビットレート（kbps）", "目标比特率（kbps）"), "int", "8000"},
            {"keyframeInterval", loc("Keyframe interval (frames)", "キーフレーム間隔（フレーム）", "关键帧间隔（帧）"), "int", "30"},
            {"audioCodec", loc("Audio codec (AAC, PCM, etc)", "オーディオコーデック（AAC、PCM等）", "音频编解码器（AAC、PCM等）"), "QString", "AAC"},
            {"audioBitrate", loc("Audio bitrate (kbps)", "オーディオビットレート（kbps）", "音频比特率（kbps）"), "int", "192"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"exportComposition", loc("Export composition to file", "コンポジションをファイルにエクスポート", "将合成导出到文件"), 
             "bool", {"ArtifactComposition*", "QString"}, {"composition", "filePath"}},
            {"setRange", loc("Set export frame range", "エクスポートフレーム範囲を設定", "设置导出帧范围"), 
             "void", {"int64_t", "int64_t"}, {"startFrame", "endFrame"}},
            {"getEstimatedSize", loc("Estimate output file size", "出力ファイルサイズを推定", "估计输出文件大小"), 
             "qint64"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ImageSequenceExporter", "ArtifactRenderController"};
    }
};

// ============================================================================
// ImageSequenceExporter Description
// ============================================================================

class ImageSequenceExporterDescription : public IDescribable {
public:
    QString className() const override { return "ImageSequenceExporter"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Exports composition as a sequence of individual image files.",
            "コンポジションを個別の画像ファイルのシーケンスとしてエクスポートします。",
            "将合成导出为单独的图像文件序列。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"format", loc("Image format (PNG, JPEG, EXR, etc)", "画像フォーマット（PNG、JPEG、EXR等）", "图像格式（PNG、JPEG、EXR等）"), "QString", "PNG"},
            {"padding", loc("Frame number digit padding", "フレーム番号の桁数", "帧号位数"), "int", "5"},
            {"colorDepth", loc("Bit depth (8, 16, or 32)", "ビット深度（8、16、または32）", "位深度（8、16或32）"), "int", "8"},
            {"compression", loc("Compression level (0-100)", "圧縮レベル（0-100）", "压缩级别（0-100）"), "int", "90"},
            {"includeAlpha", loc("Include alpha channel", "アルファチャンネルを含める", "包含Alpha通道"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"VideoExporter", "ArtifactRenderController"};
    }
};

// ============================================================================
// ProjectSerializer Description
// ============================================================================

class ProjectSerializerDescription : public IDescribable {
public:
    QString className() const override { return "ProjectSerializer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles saving and loading project files in JSON or binary format.",
            "JSONまたはバイナリ形式でのプロジェクトファイルの保存と読み込みを処理します。",
            "处理JSON或二进制格式的项目文件保存和加载。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"format", loc("Serialization format (JSON or Binary)", "シリアライズ形式（JSONまたはバイナリ）", "序列化格式（JSON或二进制）"), "SerializeFormat", "JSON"},
            {"compress", loc("Compress output file", "出力ファイルを圧縮", "压缩输出文件"), "bool", "true"},
            {"includeThumbnails", loc("Save asset thumbnails", "アセットサムネイルを保存", "保存素材缩略图"), "bool", "true"},
            {"version", loc("File format version", "ファイルフォーマットバージョン", "文件格式版本"), "QString"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"save", loc("Save project to file", "プロジェクトをファイルに保存", "将项目保存到文件"), 
             "bool", {"ArtifactProject*", "QString"}, {"project", "filePath"}},
            {"load", loc("Load project from file", "ファイルからプロジェクトを読み込み", "从文件加载项目"), 
             "ArtifactProject*", {"QString"}, {"filePath"}},
            {"exportToJSON", loc("Export project as JSON string", "プロジェクトをJSON文字列としてエクスポート", "将项目导出为JSON字符串"), 
             "QByteArray", {"ArtifactProject*"}, {"project"}},
            {"importFromJSON", loc("Import project from JSON string", "JSON文字列からプロジェクトをインポート", "从JSON字符串导入项目"), 
             "ArtifactProject*", {"QByteArray"}, {"jsonData"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactProject", "ArtifactAssetManager"};
    }
};

// ============================================================================
// AEPImporter Description
// ============================================================================

class AEPImporterDescription : public IDescribable {
public:
    QString className() const override { return "AEPImporter"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Imports Adobe After Effects project files (.aep) for compatibility.",
            "互換性のためにAdobe After Effectsプロジェクトファイル（.aep）をインポートします。",
            "导入Adobe After Effects项目文件（.aep）以实现兼容性。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "AEPImporter parses After Effects project files and converts them to Artifact "
            "format. Supports layers, effects, keyframes, compositions, and most common "
            "properties with automatic conversion.",
            "AEPImporterはAfter Effectsプロジェクトファイルを解析し、Artifact形式に変換します。"
            "レイヤー、エフェクト、キーフレーム、コンポジション、および一般的なプロパティを"
            "自動変換でサポートします。",
            "AEPImporter解析After Effects项目文件并将其转换为Artifact格式。"
            "支持图层、效果、关键帧、合成和大多数常见属性的自动转换。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"importFootage", loc("Import linked media files", "リンクされたメディアファイルをインポート", "导入链接的媒体文件"), "bool", "true"},
            {"convertEffects", loc("Attempt effect conversion", "エフェクト変換を試行", "尝试效果转换"), "bool", "true"},
            {"preserveTiming", loc("Keep original timing", "元のタイミングを保持", "保持原始时序"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"import", loc("Import AEP file", "AEPファイルをインポート", "导入AEP文件"), 
             "ArtifactProject*", {"QString"}, {"filePath"}},
            {"getSupportedEffects", loc("List convertible effects", "変換可能なエフェクト一覧", "可转换效果列表"), 
             "QStringList"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ProjectSerializer", "ArtifactProject"};
    }
};

// ============================================================================
// ScriptEngine Description
// ============================================================================

class ScriptEngineDescription : public IDescribable {
public:
    QString className() const override { return "ScriptEngine"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "JavaScript engine for scripting automation and extensions.",
            "スクリプト自動化と拡張のためのJavaScriptエンジン。",
            "用于脚本自动化和扩展的JavaScript引擎。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ScriptEngine provides a JavaScript runtime for automating tasks, creating "
            "custom tools, and extending Artifact functionality. Scripts can access the "
            "project API, manipulate layers, and create custom UI panels.",
            "ScriptEngineは、タスクの自動化、カスタムツールの作成、Artifact機能の拡張のための"
            "JavaScriptランタイムを提供します。スクリプトはプロジェクトAPIにアクセスし、"
            "レイヤーを操作し、カスタムUIパネルを作成できます。",
            "ScriptEngine提供用于自动化任务、创建自定义工具和扩展Artifact功能的JavaScript运行时。"
            "脚本可以访问项目API、操作图层并创建自定义UI面板。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"enabled", loc("Whether scripting is enabled", "スクリプティングが有効か", "是否启用脚本"), "bool", "true"},
            {"apiVersion", loc("Scripting API version", "スクリプティングAPIバージョン", "脚本API版本"), "QString"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"evaluate", loc("Execute JavaScript code", "JavaScriptコードを実行", "执行JavaScript代码"), 
             "QVariant", {"QString"}, {"code"}},
            {"loadScript", loc("Load and execute script file", "スクリプトファイルを読み込んで実行", "加载并执行脚本文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"registerFunction", loc("Register native function to script", "ネイティブ関数をスクリプトに登録", "将原生函数注册到脚本"), 
             "void", {"QString", "QJSValue"}, {"name", "function"}},
            {"createPanel", loc("Create custom UI panel", "カスタムUIパネルを作成", "创建自定义UI面板"), 
             "QWidget*", {"QString"}, {"title"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Expression", "PluginManager"};
    }
};

// ============================================================================
// PluginManager Description
// ============================================================================

class PluginManagerDescription : public IDescribable {
public:
    QString className() const override { return "PluginManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages plugin loading, lifecycle, and inter-plugin communication.",
            "プラグインの読み込み、ライフサイクル、プラグイン間通信を管理します。",
            "管理插件加载、生命周期和插件间通信。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"pluginCount", loc("Number of loaded plugins", "読み込み済みプラグイン数", "已加载插件数"), "int"},
            {"pluginDirectory", loc("Plugin search directory", "プラグイン検索ディレクトリ", "插件搜索目录"), "QString"},
            {"autoLoad", loc("Auto-load plugins on startup", "起動時にプラグインを自動読み込み", "启动时自动加载插件"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"loadPlugin", loc("Load plugin from file", "ファイルからプラグインを読み込み", "从文件加载插件"), 
             "bool", {"QString"}, {"filePath"}},
            {"unloadPlugin", loc("Unload plugin by name", "名前でプラグインをアンロード", "按名称卸载插件"), 
             "bool", {"QString"}, {"pluginName"}},
            {"getPlugins", loc("Get list of loaded plugins", "読み込み済みプラグイン一覧を取得", "获取已加载插件列表"), 
             "QStringList"},
            {"callPlugin", loc("Call plugin method", "プラグインメソッドを呼び出し", "调用插件方法"), 
             "QVariant", {"QString", "QString", "QVariantList"}, {"pluginName", "method", "args"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ScriptEngine", "EffectPlugin"};
    }
};

// ============================================================================
// CommandManager Description
// ============================================================================

class CommandManagerDescription : public IDescribable {
public:
    QString className() const override { return "CommandManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Implements undo/redo system with command pattern for all operations.",
            "全操作にコマンドパターンでアンドゥ/リドゥシステムを実装します。",
            "使用命令模式为所有操作实现撤销/重做系统。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "CommandManager provides unlimited undo/redo capability for all project "
            "modifications. Commands are grouped into transactions for atomic operations, "
            "and the history can be cleared or saved with the project.",
            "CommandManagerは、全てのプロジェクト変更に対して無制限のアンドゥ/リドゥ機能を提供します。"
            "コマンドは原子操作のためにトランザクションにグループ化され、履歴はクリアまたは"
            "プロジェクトと共に保存できます。",
            "CommandManager为所有项目修改提供无限的撤销/重做功能。"
            "命令被分组到事务中以进行原子操作，历史记录可以清除或与项目一起保存。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"canUndo", loc("Whether undo is available", "アンドゥが可能か", "是否可撤销"), "bool"},
            {"canRedo", loc("Whether redo is available", "リドゥが可能か", "是否可重做"), "bool"},
            {"undoCount", loc("Number of undo steps available", "利用可能なアンドゥステップ数", "可用的撤销步骤数"), "int"},
            {"redoCount", loc("Number of redo steps available", "利用可能なリドゥステップ数", "可用的重做步骤数"), "int"},
            {"maxHistorySize", loc("Maximum history entries", "最大履歴エントリ数", "最大历史条目数"), "int", "100"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"execute", loc("Execute command with undo support", "アンドゥサポートでコマンドを実行", "使用撤销支持执行命令"), 
             "void", {"Command*"}, {"command"}},
            {"undo", loc("Undo last command", "最後のコマンドをアンドゥ", "撤销上一个命令"), "void"},
            {"redo", loc("Redo last undone command", "最後にアンドゥしたコマンドをリドゥ", "重做上一个撤销的命令"), "void"},
            {"beginMacro", loc("Start command group", "コマンドグループを開始", "开始命令组"), 
             "void", {"QString"}, {"name"}},
            {"endMacro", loc("End command group", "コマンドグループを終了", "结束命令组"), "void"},
            {"clear", loc("Clear all history", "全履歴をクリア", "清除所有历史"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactProject", "ArtifactComposition"};
    }
};

// ============================================================================
// ClipboardManager Description
// ============================================================================

class ClipboardManagerDescription : public IDescribable {
public:
    QString className() const override { return "ClipboardManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles copy/paste of layers, keyframes, and effects between compositions.",
            "コンポジション間のレイヤー、キーフレーム、エフェクトのコピー/ペーストを処理します。",
            "处理合成之间的图层、关键帧和效果的复制/粘贴。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"hasContent", loc("Whether clipboard has content", "クリップボードに内容があるか", "剪贴板是否有内容"), "bool"},
            {"contentType", loc("Type of clipboard content", "クリップボード内容のタイプ", "剪贴板内容类型"), "ClipboardType"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"copyLayers", loc("Copy selected layers", "選択レイヤーをコピー", "复制选中的图层"), 
             "void", {"QList<ArtifactAbstractLayer*>"}, {"layers"}},
            {"pasteLayers", loc("Paste layers to composition", "コンポジションにレイヤーをペースト", "将图层粘贴到合成"), 
             "QList<ArtifactAbstractLayer*>", {"ArtifactComposition*"}, {"composition"}},
            {"copyKeyframes", loc("Copy selected keyframes", "選択キーフレームをコピー", "复制选中的关键帧"), 
             "void", {"QList<Keyframe*>"}, {"keyframes"}},
            {"copyEffects", loc("Copy layer effects", "レイヤーエフェクトをコピー", "复制图层效果"), 
             "void", {"QList<ArtifactAbstractEffect*>"}, {"effects"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"CommandManager", "ArtifactAbstractLayer"};
    }
};

// ============================================================================
// AuditLog Description
// ============================================================================

class AuditLogDescription : public IDescribable {
public:
    QString className() const override { return "AuditLog"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Records project history and user actions for review and recovery.",
            "レビューと復旧のためにプロジェクト履歴とユーザーアクションを記録します。",
            "记录项目历史和用户操作以供审查和恢复。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"enabled", loc("Whether logging is active", "ロギングが有効か", "是否启用日志"), "bool", "true"},
            {"entryCount", loc("Number of log entries", "ログエントリ数", "日志条目数"), "int"},
            {"maxEntries", loc("Maximum stored entries", "最大保存エントリ数", "最大存储条目数"), "int", "10000"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"log", loc("Add log entry", "ログエントリを追加", "添加日志条目"), 
             "void", {"QString", "QString"}, {"action", "details"}},
            {"getEntries", loc("Get log entries in range", "範囲内のログエントリを取得", "获取范围内的日志条目"), 
             "QList<AuditEntry>", {"int", "int"}, {"start", "count"}},
            {"exportLog", loc("Export log to file", "ログをファイルにエクスポート", "将日志导出到文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"clear", loc("Clear all log entries", "全ログエントリをクリア", "清除所有日志条目"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"CommandManager", "ProjectSerializer"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<VideoExporterDescription> _reg_VideoExporter("VideoExporter");
static AutoRegisterDescribable<ImageSequenceExporterDescription> _reg_ImageSequenceExporter("ImageSequenceExporter");
static AutoRegisterDescribable<ProjectSerializerDescription> _reg_ProjectSerializer("ProjectSerializer");
static AutoRegisterDescribable<AEPImporterDescription> _reg_AEPImporter("AEPImporter");
static AutoRegisterDescribable<ScriptEngineDescription> _reg_ScriptEngine("ScriptEngine");
static AutoRegisterDescribable<PluginManagerDescription> _reg_PluginManager("PluginManager");
static AutoRegisterDescribable<CommandManagerDescription> _reg_CommandManager("CommandManager");
static AutoRegisterDescribable<ClipboardManagerDescription> _reg_ClipboardManager("ClipboardManager");
static AutoRegisterDescribable<AuditLogDescription> _reg_AuditLog("AuditLog");

} // namespace ArtifactCore