module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.DataAssetDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ArtifactAssetManager Description
// ============================================================================

class AssetManagerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAssetManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages all media assets including images, videos, audio, and fonts.",
            "画像、動画、オーディオ、フォントを含むすべてのメディアアセットを管理します。",
            "管理所有媒体素材，包括图像、视频、音频和字体。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactAssetManager provides a centralized location for all project assets. "
            "It handles importing, organizing, previewing, and linking assets to layers. "
            "Assets can be searched, tagged, and reused across multiple compositions.",
            "ArtifactAssetManagerは、すべてのプロジェクトアセットの一元管理場所を提供します。"
            "インポート、整理、プレビュー、レイヤーへのアセットリンクを処理します。"
            "アセットは検索、タグ付け、複数のコンポジション間で再利用可能です。",
            "ArtifactAssetManager为所有项目素材提供集中管理位置。"
            "它处理导入、组织、预览和将素材链接到图层。"
            "素材可以搜索、标记并在多个合成之间重复使用。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"assetCount", loc("Number of assets in library", "ライブラリ内のアセット数", "库中的素材数"), "int"},
            {"totalSizeMB", loc("Total asset storage size", "総アセットストレージサイズ", "总素材存储大小"), "float"},
            {"watchFolder", loc("Auto-import from folder", "フォルダから自動インポート", "从文件夹自动导入"), "QString"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"importFile", loc("Import single file as asset", "単一ファイルをアセットとしてインポート", "将单个文件导入为素材"), 
             "Asset*", {"QString"}, {"filePath"}},
            {"importFolder", loc("Import all files from folder", "フォルダ内の全ファイルをインポート", "导入文件夹内所有文件"), 
             "QList<Asset*>", {"QString"}, {"folderPath"}},
            {"findByName", loc("Search assets by name", "名前でアセットを検索", "按名称搜索素材"), 
             "QList<Asset*>", {"QString"}, {"searchTerm"}},
            {"getThumbnail", loc("Get preview thumbnail", "プレビューサムネイルを取得", "获取预览缩略图"), 
             "QImage", {"Asset*"}, {"asset"}},
            {"remove", loc("Remove asset from project", "アセットをプロジェクトから削除", "从项目中移除素材"), 
             "void", {"Asset*"}, {"asset"}},
            {"replace", loc("Replace asset source file", "アセットのソースファイルを置換", "替换素材源文件"), 
             "bool", {"Asset*", "QString"}, {"asset", "newFilePath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactProject", "Asset", "ArtifactImageLayer", "ArtifactVideoLayer"};
    }
};

// ============================================================================
// Asset Description
// ============================================================================

class AssetDescription : public IDescribable {
public:
    QString className() const override { return "Asset"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Represents a single media asset with metadata and file reference.",
            "メタデータとファイル参照を持つ単一のメディアアセットを表します。",
            "表示具有元数据和文件引用的单个媒体素材。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Asset display name", "アセット表示名", "素材显示名称"), "QString"},
            {"filePath", loc("Source file path", "ソースファイルパス", "源文件路径"), "QString"},
            {"type", loc("Asset type (Image, Video, Audio, Font)", "アセットタイプ（画像、動画、オーディオ、フォント）", "素材类型（图像、视频、音频、字体）"), "AssetType"},
            {"fileSize", loc("File size in bytes", "ファイルサイズ（バイト）", "文件大小（字节）"), "qint64"},
            {"createdDate", loc("Import timestamp", "インポートタイムスタンプ", "导入时间戳"), "QDateTime"},
            {"modifiedDate", loc("Last modified timestamp", "最終更新タイムスタンプ", "最后修改时间戳"), "QDateTime"},
            {"tags", loc("User-defined tags", "ユーザー定義タグ", "用户定义标签"), "QStringList"},
            {"usageCount", loc("Number of layers using this asset", "このアセットを使用するレイヤー数", "使用此素材的图层数"), "int"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAssetManager", "ArtifactAbstractLayer"};
    }
};

// ============================================================================
// ProjectSettings Description
// ============================================================================

class ProjectSettingsDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactProjectSettings"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Stores project-wide settings including defaults, preferences, and metadata.",
            "デフォルト、設定、メタデータを含むプロジェクト全体の設定を保存します。",
            "存储项目范围的设置，包括默认值、首选项和元数据。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"defaultWidth", loc("Default composition width", "デフォルトコンポジション幅", "默认合成宽度"), "int", "1920"},
            {"defaultHeight", loc("Default composition height", "デフォルトコンポジション高さ", "默认合成高度"), "int", "1080"},
            {"defaultFrameRate", loc("Default frames per second", "デフォルトフレームレート", "默认帧率"), "float", "30.0"},
            {"colorDepth", loc("Bit depth (8, 16, or 32)", "ビット深度（8、16、または32）", "位深度（8、16或32）"), "int", "8"},
            {"colorProfile", loc("Default color profile", "デフォルトカラープロファイル", "默认颜色配置文件"), "QString", "sRGB"},
            {"autoSaveInterval", loc("Auto-save interval in minutes", "自動保存間隔（分）", "自动保存间隔（分钟）"), "int", "5"},
            {"projectName", loc("Project name", "プロジェクト名", "项目名称"), "QString"},
            {"author", loc("Author name", "作者名", "作者名称"), "QString"},
            {"description", loc("Project description", "プロジェクト説明", "项目描述"), "QString"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactProject", "ArtifactComposition"};
    }
};

// ============================================================================
// Timeline Description
// ============================================================================

class TimelineDescription : public IDescribable {
public:
    QString className() const override { return "Timeline"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Visual timeline widget for viewing and editing layer timing.",
            "レイヤーのタイミングを表示・編集するためのビジュアルタイムラインウィジェット。",
            "用于查看和编辑图层时序的可视化时间线控件。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"zoom", loc("Timeline zoom level", "タイムラインズームレベル", "时间线缩放级别"), "float", "1.0"},
            {"scrollPosition", loc("Horizontal scroll position", "水平スクロール位置", "水平滚动位置"), "float"},
            {"snapEnabled", loc("Snap to frames and markers", "フレームとマーカーにスナップ", "对齐到帧和标记"), "bool", "true"},
            {"snapThreshold", loc("Snap sensitivity in pixels", "スナップ感度（ピクセル）", "对齐灵敏度（像素）"), "int", "10"},
            {"showWaveforms", loc("Display audio waveforms", "オーディオ波形を表示", "显示音频波形"), "bool", "true"},
            {"showKeyframes", loc("Display keyframe indicators", "キーフレームインジケーターを表示", "显示关键帧指示器"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"zoomToFit", loc("Zoom to show all content", "全コンテンツを表示するようにズーム", "缩放以显示所有内容"), "void"},
            {"goToTime", loc("Jump to specific time", "特定時間にジャンプ", "跳转到特定时间"), "void", {"float"}, {"time"}},
            {"goToFrame", loc("Jump to specific frame", "特定フレームにジャンプ", "跳转到特定帧"), "void", {"int64_t"}, {"frame"}},
            {"setWorkArea", loc("Set work area range", "ワークエリア範囲を設定", "设置工作区域范围"), "void", {"int64_t", "int64_t"}, {"start", "end"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactCompositionPlaybackController"};
    }
};

// ============================================================================
// Marker Description
// ============================================================================

class MarkerDescription : public IDescribable {
public:
    QString className() const override { return "Marker"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A named marker point on the timeline for navigation and reference.",
            "ナビゲーションと参照のためのタイムライン上の名前付きマーカーポイント。",
            "时间线上的命名标记点，用于导航和参考。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Marker label", "マーカーラベル", "标记标签"), "QString"},
            {"time", loc("Marker position in seconds", "マーカー位置（秒）", "标记位置（秒）"), "float"},
            {"frame", loc("Marker position in frames", "マーカー位置（フレーム）", "标记位置（帧）"), "int64_t"},
            {"color", loc("Marker color", "マーカー色", "标记颜色"), "QColor", "red"},
            {"comment", loc("Marker notes", "マーカーノート", "标记备注"), "QString"},
            {"duration", loc("Marker duration (0 for point)", "マーカー継続時間（0でポイント）", "标记持续时间（0为点）"), "float", "0.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Timeline", "ArtifactComposition"};
    }
};

// ============================================================================
// AudioWaveform Description
// ============================================================================

class AudioWaveformDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAudioWaveform"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Visualizes audio amplitude over time for display and editing.",
            "表示と編集のためにオーディオ振幅を時間軸で可視化します。",
            "可视化音频振幅随时间变化，用于显示和编辑。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"samplesPerPixel", loc("Audio samples per display pixel", "表示ピクセルあたりのオーディオサンプル", "每显示像素的音频采样数"), "int", "256"},
            {"channel", loc("Which channel to display (-1 for all)", "表示するチャンネル（-1で全て）", "要显示的通道（-1为所有）"), "int", "-1"},
            {"amplitudeScale", loc("Vertical amplitude scaling", "垂直振幅スケーリング", "垂直振幅缩放"), "float", "1.0"},
            {"color", loc("Waveform color", "波形色", "波形颜色"), "QColor", "#00FF00"},
            {"backgroundColor", loc("Background color", "背景色", "背景颜色"), "QColor", "#000000"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"generate", loc("Generate waveform from audio", "オーディオから波形を生成", "从音频生成波形"), 
             "void", {"AudioData"}, {"audio"}},
            {"getAmplitudeAt", loc("Get amplitude at time position", "時間位置の振幅を取得", "获取时间位置的振幅"), 
             "float", {"float"}, {"time"}},
            {"render", loc("Render waveform to image", "波形を画像にレンダリング", "将波形渲染到图像"), 
             "QImage", {"int", "int"}, {"width", "height"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAudioLayer", "ArtifactAudioMixer"};
    }
};

// ============================================================================
// EffectPreset Description
// ============================================================================

class EffectPresetDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactEffectPreset"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Saves and loads effect configuration presets for reuse.",
            "再利用のためにエフェクト設定プリセットを保存・読み込みします。",
            "保存和加载效果配置预设以供重复使用。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Preset name", "プリセット名", "预设名称"), "QString"},
            {"category", loc("Preset category for organization", "整理用プリセットカテゴリ", "用于组织的预设类别"), "QString"},
            {"description", loc("Preset description", "プリセット説明", "预设描述"), "QString"},
            {"effects", loc("List of effects in preset", "プリセット内のエフェクトリスト", "预设中的效果列表"), "QList<Effect*>"},
            {"isBuiltIn", loc("Whether preset is built-in", "プリセットが組み込みか", "预设是否为内置"), "bool"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"save", loc("Save preset to file", "プリセットをファイルに保存", "将预设保存到文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"load", loc("Load preset from file", "ファイルからプリセットを読み込み", "从文件加载预设"), 
             "bool", {"QString"}, {"filePath"}},
            {"applyTo", loc("Apply preset to layer", "プリセットをレイヤーに適用", "将预设应用于图层"), 
             "void", {"ArtifactAbstractLayer*"}, {"layer"}},
            {"createFromLayer", loc("Create preset from layer effects", "レイヤーエフェクトからプリセットを作成", "从图层效果创建预设"), 
             "void", {"ArtifactAbstractLayer*"}, {"layer"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "ArtifactAbstractLayer"};
    }
};

// ============================================================================
// ColorManagement Description
// ============================================================================

class ColorManagementDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactColorManagement"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages color spaces, profiles, and color conversion settings.",
            "色空間、プロファイル、色変換設定を管理します。",
            "管理色彩空间、配置文件和颜色转换设置。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"workingSpace", loc("Project working color space", "プロジェクトワーキング色空間", "项目工作色彩空间"), "ColorSpace"},
            {"displayProfile", loc("Monitor display profile", "モニター表示プロファイル", "显示器配置文件"), "QString"},
            {"renderingIntent", loc("Color conversion intent", "色変換インテント", "颜色转换意图"), "RenderingIntent", "Perceptual"},
            {"useGPUConversion", loc("Use GPU for color conversion", "色変換にGPUを使用", "使用GPU进行颜色转换"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"convert", loc("Convert image between color spaces", "画像を色空間間で変換", "在色彩空间之间转换图像"), 
             "QImage", {"QImage", "ColorSpace", "ColorSpace"}, {"source", "from", "to"}},
            {"setWorkingSpace", loc("Set project working space", "プロジェクトワーキングスペースを設定", "设置项目工作空间"), 
             "void", {"ColorSpace"}, {"colorSpace"}},
            {"getDisplayTransform", loc("Get transform for display", "表示用トランスフォームを取得", "获取显示变换"), 
             "QTransform", {}, {}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ColorSpace", "ColorGrading"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<AssetManagerDescription> _reg_AssetManager("ArtifactAssetManager");
static AutoRegisterDescribable<AssetDescription> _reg_Asset("Asset");
static AutoRegisterDescribable<ProjectSettingsDescription> _reg_ProjectSettings("ArtifactProjectSettings");
static AutoRegisterDescribable<TimelineDescription> _reg_Timeline("Timeline");
static AutoRegisterDescribable<MarkerDescription> _reg_Marker("Marker");
static AutoRegisterDescribable<AudioWaveformDescription> _reg_AudioWaveform("ArtifactAudioWaveform");
static AutoRegisterDescribable<EffectPresetDescription> _reg_EffectPreset("ArtifactEffectPreset");
static AutoRegisterDescribable<ColorManagementDescription> _reg_ColorManagement("ArtifactColorManagement");

} // namespace ArtifactCore