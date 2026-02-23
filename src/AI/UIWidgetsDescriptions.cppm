module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.UIWidgetsDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// TimelineWidget Description
// ============================================================================

class TimelineWidgetDescription : public IDescribable {
public:
    QString className() const override { return "TimelineWidget"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Main timeline UI widget with tracks, keyframes, and time ruler.",
            "トラック、キーフレーム、タイムルーラーを持つメインタイムラインUIウィジェット。",
            "具有轨道、关键帧和时间标尺的主时间线UI控件。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"trackHeight", loc("Height of each track row", "各トラック行の高さ", "每个轨道行的高度"), "int", "30"},
            {"timeRulerHeight", loc("Time ruler height", "タイムルーラーの高さ", "时间标尺高度"), "int", "25"},
            {"showKeyframes", loc("Display keyframe lanes", "キーフレームレーンを表示", "显示关键帧通道"), "bool", "true"},
            {"showWaveforms", loc("Display audio waveforms", "オーディオ波形を表示", "显示音频波形"), "bool", "true"},
            {"snapToFrame", loc("Snap to frame boundaries", "フレーム境界にスナップ", "对齐到帧边界"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addTrack", loc("Add new track to timeline", "タイムラインに新規トラックを追加", "向时间线添加新轨道"), 
             "void", {"TrackType"}, {"type"}},
            {"selectTrack", loc("Select track by index", "インデックスでトラックを選択", "按索引选择轨道"), 
             "void", {"int"}, {"index"}},
            {"zoomIn", loc("Zoom in timeline view", "タイムラインビューをズームイン", "放大时间线视图"), "void"},
            {"zoomOut", loc("Zoom out timeline view", "タイムラインビューをズームアウト", "缩小时间线视图"), "void"},
            {"fitToView", loc("Fit all content to view", "全コンテンツをビューにフィット", "将所有内容适应到视图"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Timeline", "TrackWidget", "KeyframeEditor"};
    }
};

// ============================================================================
// TrackWidget Description
// ============================================================================

class TrackWidgetDescription : public IDescribable {
public:
    QString className() const override { return "TrackWidget"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Individual track row in the timeline showing layer clips.",
            "レイヤークリップを表示するタイムライン内の個別トラック行。",
            "时间线中显示图层片段的单个轨道行。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"trackName", loc("Display name", "表示名", "显示名称"), "QString"},
            {"trackColor", loc("Track header color", "トラックヘッダー色", "轨道标题颜色"), "QColor"},
            {"muted", loc("Whether track is muted", "トラックがミュートされているか", "轨道是否静音"), "bool", "false"},
            {"solo", loc("Solo this track", "このトラックをソロ", "独奏此轨道"), "bool", "false"},
            {"locked", loc("Prevent editing", "編集を防止", "防止编辑"), "bool", "false"},
            {"visible", loc("Show in timeline", "タイムラインに表示", "在时间线中显示"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"TimelineWidget", "ClipWidget"};
    }
};

// ============================================================================
// ClipWidget Description
// ============================================================================

class ClipWidgetDescription : public IDescribable {
public:
    QString className() const override { return "ClipWidget"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Visual representation of a layer clip on the timeline track.",
            "タイムライントラック上のレイヤークリップの視覚表現。",
            "时间线轨道上图层片段的视觉表示。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"clipName", loc("Clip label", "クリップラベル", "片段标签"), "QString"},
            {"clipColor", loc("Background color", "背景色", "背景颜色"), "QColor"},
            {"startTime", loc("Clip start time", "クリップ開始時間", "片段开始时间"), "float"},
            {"duration", loc("Clip length", "クリップ長", "片段长度"), "float"},
            {"selected", loc("Whether clip is selected", "クリップが選択されているか", "片段是否被选中"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"TrackWidget", "TimelineWidget"};
    }
};

// ============================================================================
// KeyframeEditor Description
// ============================================================================

class KeyframeEditorDescription : public IDescribable {
public:
    QString className() const override { return "KeyframeEditor"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Widget for editing keyframes on a property timeline.",
            "プロパティタイムラインでキーフレームを編集するウィジェット。",
            "在属性时间线上编辑关键帧的控件。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"propertyName", loc("Name of animated property", "アニメーションプロパティ名", "动画属性名称"), "QString"},
            {"keyframeCount", loc("Number of keyframes", "キーフレーム数", "关键帧数"), "int"},
            {"selectedKeyframe", loc("Currently selected keyframe index", "現在選択されているキーフレームインデックス", "当前选中的关键帧索引"), "int", "-1"},
            {"showTangents", loc("Display bezier handles", "ベジェハンドルを表示", "显示贝塞尔手柄"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addKeyframe", loc("Add keyframe at time", "時間にキーフレームを追加", "在时间处添加关键帧"), 
             "void", {"float"}, {"time"}},
            {"removeKeyframe", loc("Remove selected keyframe", "選択キーフレームを削除", "删除选中的关键帧"), "void"},
            {"moveKeyframe", loc("Move keyframe to new time", "キーフレームを新しい時間に移動", "将关键帧移动到新时间"), 
             "void", {"int", "float"}, {"index", "newTime"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Keyframe", "CurveEditor"};
    }
};

// ============================================================================
// PreviewWidget Description
// ============================================================================

class PreviewWidgetDescription : public IDescribable {
public:
    QString className() const override { return "PreviewWidget"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Main preview viewport for viewing composition output.",
            "コンポジション出力を表示するメインプレビュービューポート。",
            "查看合成输出的主预览视口。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"zoom", loc("Preview zoom level", "プレビューズームレベル", "预览缩放级别"), "float", "1.0"},
            {"showSafeAreas", loc("Display safe area guides", "セーフエリアガイドを表示", "显示安全区域指南"), "bool", "false"},
            {"showGrid", loc("Display grid overlay", "グリッドオーバーレイを表示", "显示网格叠加"), "bool", "false"},
            {"showGuides", loc("Display guide lines", "ガイドラインを表示", "显示参考线"), "bool", "true"},
            {"backgroundColor", loc("Canvas background color", "キャンバス背景色", "画布背景颜色"), "QColor", "#404040"},
            {"resolution", loc("Preview resolution scale", "プレビュー解像度スケール", "预览分辨率缩放"), "float", "1.0", "0.25", "1.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"fitToWindow", loc("Fit composition to window", "コンポジションをウィンドウにフィット", "将合成适应到窗口"), "void"},
            {"zoomTo100", loc("Set zoom to 100%", "ズームを100%に設定", "设置缩放为100%"), "void"},
            {"takeSnapshot", loc("Capture current frame", "現在のフレームをキャプチャ", "捕获当前帧"), 
             "QImage"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "TimelineWidget"};
    }
};

// ============================================================================
// LayerPanel Description
// ============================================================================

class LayerPanelDescription : public IDescribable {
public:
    QString className() const override { return "LayerPanel"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Panel showing layer stack with visibility and blend mode controls.",
            "可視性とブレンドモードコントロール付きのレイヤースタックを表示するパネル。",
            "显示图层堆栈并带有可见性和混合模式控件的面板。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"selectedLayer", loc("Index of selected layer", "選択レイヤーのインデックス", "选中图层的索引"), "int", "-1"},
            {"layerCount", loc("Number of layers", "レイヤー数", "图层数"), "int"},
            {"showThumbnails", loc("Display layer thumbnails", "レイヤーサムネイルを表示", "显示图层缩略图"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addLayer", loc("Add new layer at index", "インデックスに新規レイヤーを追加", "在索引处添加新图层"), 
             "void", {"int", "LayerType"}, {"index", "type"}},
            {"removeLayer", loc("Remove selected layer", "選択レイヤーを削除", "删除选中的图层"), "void"},
            {"moveLayerUp", loc("Move layer up in stack", "レイヤーをスタックの上に移動", "在堆栈中上移图层"), "void"},
            {"moveLayerDown", loc("Move layer down in stack", "レイヤーをスタックの下に移動", "在堆栈中下移图层"), "void"},
            {"duplicateLayer", loc("Duplicate selected layer", "選択レイヤーを複製", "复制选中的图层"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "EffectPanel"};
    }
};

// ============================================================================
// EffectPanel Description
// ============================================================================

class EffectPanelDescription : public IDescribable {
public:
    QString className() const override { return "EffectPanel"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Panel for managing and configuring effects on selected layer.",
            "選択レイヤーのエフェクトを管理・設定するパネル。",
            "管理和配置选中图层效果的面板。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"effectCount", loc("Number of effects on layer", "レイヤー上のエフェクト数", "图层上的效果数"), "int"},
            {"selectedEffect", loc("Index of selected effect", "選択エフェクトのインデックス", "选中的效果索引"), "int", "-1"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addEffect", loc("Add effect to layer", "レイヤーにエフェクトを追加", "向图层添加效果"), 
             "void", {"QString"}, {"effectName"}},
            {"removeEffect", loc("Remove selected effect", "選択エフェクトを削除", "删除选中的效果"), "void"},
            {"moveEffectUp", loc("Move effect up in stack", "エフェクトをスタックの上に移動", "在堆栈中上移效果"), "void"},
            {"moveEffectDown", loc("Move effect down in stack", "エフェクトをスタックの下に移動", "在堆栈中下移效果"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "LayerPanel"};
    }
};

// ============================================================================
// PropertyPanel Description
// ============================================================================

class PropertyPanelDescription : public IDescribable {
public:
    QString className() const override { return "PropertyPanel"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Panel displaying properties of selected object with animation controls.",
            "アニメーションコントロール付きで選択オブジェクトのプロパティを表示するパネル。",
            "显示选中对象属性并带有动画控件的面板。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"objectName", loc("Name of selected object", "選択オブジェクト名", "选中对象名称"), "QString"},
            {"propertyCount", loc("Number of properties", "プロパティ数", "属性数"), "int"},
            {"showAnimatedOnly", loc("Show only animatable properties", "アニメーション可能なプロパティのみ表示", "仅显示可动画属性"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"LayerPanel", "KeyframeEditor"};
    }
};

// ============================================================================
// ColorPicker Description
// ============================================================================

class ColorPickerDescription : public IDescribable {
public:
    QString className() const override { return "ColorPicker"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Color selection widget with wheel, sliders, and palettes.",
            "ホイール、スライダー、パレット付きの色選択ウィジェット。",
            "带有色轮、滑块和调色板的颜色选择控件。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"currentColor", loc("Selected color", "選択色", "选中的颜色"), "QColor"},
            {"colorMode", loc("RGB, HSL, or HSV mode", "RGB、HSL、またはHSVモード", "RGB、HSL或HSV模式"), "ColorMode", "RGB"},
            {"showAlpha", loc("Show alpha channel slider", "アルファチャンネルスライダーを表示", "显示Alpha通道滑块"), "bool", "true"},
            {"showHex", loc("Show hex color input", "16進色入力を表示", "显示十六进制颜色输入"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ColorGrading", "ColorWheels"};
    }
};

// ============================================================================
// RenderDialog Description
// ============================================================================

class RenderDialogDescription : public IDescribable {
public:
    QString className() const override { return "RenderDialog"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Dialog for configuring and starting render export.",
            "レンダーエクスポートの設定と開始ダイアログ。",
            "配置和启动渲染导出的对话框。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"outputPath", loc("Output file path", "出力ファイルパス", "输出文件路径"), "QString"},
            {"format", loc("Output format", "出力フォーマット", "输出格式"), "QString", "MP4"},
            {"resolution", loc("Output resolution", "出力解像度", "输出分辨率"), "QString", "1920x1080"},
            {"frameRate", loc("Output frame rate", "出力フレームレート", "输出帧率"), "float", "30.0"},
            {"quality", loc("Quality preset", "品質プリセット", "质量预设"), "QString", "High"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"startRender", loc("Begin rendering", "レンダリング開始", "开始渲染"), "void"},
            {"cancelRender", loc("Cancel current render", "現在のレンダリングをキャンセル", "取消当前渲染"), "void"},
            {"getProgress", loc("Get render progress percentage", "レンダリング進捗率を取得", "获取渲染进度百分比"), 
             "float"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"VideoExporter", "ImageSequenceExporter", "ArtifactRenderController"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<TimelineWidgetDescription> _reg_TimelineWidget("TimelineWidget");
static AutoRegisterDescribable<TrackWidgetDescription> _reg_TrackWidget("TrackWidget");
static AutoRegisterDescribable<ClipWidgetDescription> _reg_ClipWidget("ClipWidget");
static AutoRegisterDescribable<KeyframeEditorDescription> _reg_KeyframeEditor("KeyframeEditor");
static AutoRegisterDescribable<PreviewWidgetDescription> _reg_PreviewWidget("PreviewWidget");
static AutoRegisterDescribable<LayerPanelDescription> _reg_LayerPanel("LayerPanel");
static AutoRegisterDescribable<EffectPanelDescription> _reg_EffectPanel("EffectPanel");
static AutoRegisterDescribable<PropertyPanelDescription> _reg_PropertyPanel("PropertyPanel");
static AutoRegisterDescribable<ColorPickerDescription> _reg_ColorPicker("ColorPicker");
static AutoRegisterDescribable<RenderDialogDescription> _reg_RenderDialog("RenderDialog");

} // namespace ArtifactCore