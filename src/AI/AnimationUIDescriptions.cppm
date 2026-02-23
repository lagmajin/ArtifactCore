module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>
#include <QEasingCurve>

module Core.AI.AnimationUIDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// AnimatableTransform2D Description
// ============================================================================

class AnimatableTransform2DDescription : public IDescribable {
public:
    QString className() const override { return "AnimatableTransform2D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "2D transform with keyframe animation support for position, rotation, and scale.",
            "位置、回転、スケールのキーフレームアニメーションをサポートした2Dトランスフォーム。",
            "支持位置、旋转和缩放关键帧动画的2D变换。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "AnimatableTransform2D provides a 2D transformation matrix that can be animated over time. "
            "It supports anchor point adjustments, separate XYZ scale, and various interpolation modes "
            "for smooth motion graphics animations.",
            "AnimatableTransform2Dは時間でアニメーション可能な2D変換行列を提供します。"
            "アンカーポイント調整、XYZ別スケール、スムーズなモーショングラフィックスアニメーションのための"
            "様々な補間モードをサポートします。",
            "AnimatableTransform2D提供可随时间动画化的2D变换矩阵。"
            "它支持锚点调整、XYZ独立缩放以及用于平滑动态图形动画的各种插值模式。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("X, Y position", "X, Y位置", "X, Y位置"), "QVector2D", "(0, 0)"},
            {"rotation", loc("Rotation in degrees", "回転角度（度）", "旋转角度（度）"), "float", "0.0"},
            {"scale", loc("X, Y scale factors", "X, Yスケール係数", "X, Y缩放系数"), "QVector2D", "(1, 1)"},
            {"anchorPoint", loc("Transform origin point", "トランスフォーム原点", "变换原点"), "QVector2D", "(0, 0)"},
            {"opacity", loc("Visibility opacity", "可視性の不透明度", "可见性不透明度"), "float", "1.0", "0.0", "1.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setPosition", loc("Set position at time", "時間の位置を設定", "设置时间点位置"), 
             "void", {"QVector2D", "float"}, {"pos", "time"}},
            {"setRotation", loc("Set rotation at time", "時間の回転を設定", "设置时间点旋转"), 
             "void", {"float", "float"}, {"rotation", "time"}},
            {"setScale", loc("Set scale at time", "時間のスケールを設定", "设置时间点缩放"), 
             "void", {"QVector2D", "float"}, {"scale", "time"}},
            {"getKeyframes", loc("Get all keyframes for property", "プロパティの全キーフレームを取得", "获取属性的所有关键帧"), 
             "QList<Keyframe>", {"QString"}, {"propertyName"}},
            {"evaluate", loc("Get interpolated value at time", "時間での補間値を取得", "获取时间点的插值"), 
             "QTransform", {"float"}, {"time"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AnimatableTransform3D", "Keyframe", "AnimationCurve"};
    }
};

// ============================================================================
// AnimatableTransform3D Description
// ============================================================================

class AnimatableTransform3DDescription : public IDescribable {
public:
    QString className() const override { return "AnimatableTransform3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "3D transform with full keyframe animation for position, rotation, and scale in 3D space.",
            "3D空間での位置、回転、スケールの完全なキーフレームアニメーションを持つ3Dトランスフォーム。",
            "在3D空间中具有位置、旋转和缩放完整关键帧动画的3D变换。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("X, Y, Z position", "X, Y, Z位置", "X, Y, Z位置"), "QVector3D", "(0, 0, 0)"},
            {"rotation", loc("X, Y, Z rotation in degrees", "X, Y, Z回転角度（度）", "X, Y, Z旋转角度（度）"), "QVector3D", "(0, 0, 0)"},
            {"scale", loc("X, Y, Z scale factors", "X, Y, Zスケール係数", "X, Y, Z缩放系数"), "QVector3D", "(1, 1, 1)"},
            {"orientation", loc("Quaternion orientation", "クォータニオンの向き", "四元数方向"), "QQuaternion"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AnimatableTransform2D", "ArtifactCameraLayer"};
    }
};

// ============================================================================
// Keyframe Description
// ============================================================================

class KeyframeDescription : public IDescribable {
public:
    QString className() const override { return "Keyframe"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Represents a single keyframe with time, value, and interpolation settings.",
            "時間、値、補間設定を持つ単一のキーフレームを表します。",
            "表示具有时间、值和插值设置的单个关键帧。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"time", loc("Keyframe time position", "キーフレームの時間位置", "关键帧时间位置"), "float"},
            {"value", loc("Keyframe value", "キーフレームの値", "关键帧值"), "QVariant"},
            {"easing", loc("Interpolation curve type", "補間カーブタイプ", "插值曲线类型"), "QEasingCurve::Type", "Linear"},
            {"inTangent", loc("Incoming bezier handle", "入口ベジェハンドル", "入贝塞尔手柄"), "QVector2D"},
            {"outTangent", loc("Outgoing bezier handle", "出口ベジェハンドル", "出贝塞尔手柄"), "QVector2D"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AnimatableTransform2D", "AnimationCurve", "KeyframeAnimation"};
    }
};

// ============================================================================
// TimeRemap Description
// ============================================================================

class TimeRemapDescription : public IDescribable {
public:
    QString className() const override { return "TimeRemap"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Controls time flow with speed changes, reverse playback, and freeze frames.",
            "速度変更、逆再生、フリーズフレームで時間の流れを制御します。",
            "使用速度变化、倒放和冻结帧控制时间流。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "TimeRemap allows non-linear time mapping for creative effects like slow-motion, "
            "time-lapse, reverse playback, and freeze frames. It can be keyframed for dynamic "
            "time manipulation during playback.",
            "TimeRemapは、スローモーション、タイムラプス、逆再生、フリーズフレームなどの"
            "クリエイティブなエフェクトのための非線形時間マッピングを可能にします。"
            "再生中の動的な時間操作のためにキーフレーム可能です。",
            "TimeRemap允许非线性时间映射，实现慢动作、延时摄影、倒放和冻结帧等创意效果。"
            "它可以关键帧化以在播放期间进行动态时间操作。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"speed", loc("Playback speed multiplier", "再生速度倍率", "播放速度倍数"), "float", "1.0"},
            {"reverse", loc("Play backwards", "逆再生", "倒放"), "bool", "false"},
            {"loop", loc("Loop between in/out points", "イン/アウトポイント間でループ", "在入出点之间循环"), "bool", "false"},
            {"freezeFrame", loc("Hold current frame", "現在のフレームをホールド", "保持当前帧"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactVideoLayer", "ArtifactComposition", "Keyframe"};
    }
};

// ============================================================================
// InputOperator Description
// ============================================================================

class InputOperatorDescription : public IDescribable {
public:
    QString className() const override { return "InputOperator"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles user input from keyboard, mouse, and other devices for UI interaction.",
            "UI操作のためのキーボード、マウス、その他のデバイスからのユーザー入力を処理します。",
            "处理来自键盘、鼠标和其他设备的用户输入以进行UI交互。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"enabled", loc("Whether input is processed", "入力を処理するか", "是否处理输入"), "bool", "true"},
            {"mousePosition", loc("Current mouse position", "現在のマウス位置", "当前鼠标位置"), "QPointF"},
            {"mouseButtons", loc("Currently pressed mouse buttons", "現在押されているマウスボタン", "当前按下的鼠标按钮"), "Qt::MouseButtons"},
            {"modifiers", loc("Keyboard modifiers (Shift, Ctrl, etc)", "キーボード修飾子（Shift、Ctrl等）", "键盘修饰键（Shift、Ctrl等）"), "Qt::KeyboardModifiers"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"bindKey", loc("Bind action to keyboard key", "キーボードキーにアクションをバインド", "将动作绑定到键盘按键"), 
             "void", {"Qt::Key", "QString"}, {"key", "actionId"}},
            {"bindMouse", loc("Bind action to mouse button", "マウスボタンにアクションをバインド", "将动作绑定到鼠标按钮"), 
             "void", {"Qt::MouseButton", "QString"}, {"button", "actionId"}},
            {"isKeyPressed", loc("Check if key is currently pressed", "キーが現在押されているか確認", "检查按键是否当前按下"), 
             "bool", {"Qt::Key"}, {"key"}},
            {"isActionTriggered", loc("Check if action was triggered", "アクションがトリガーされたか確認", "检查动作是否被触发"), 
             "bool", {"QString"}, {"actionId"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactPlaybackShortcuts", "ShortcutManager"};
    }
};

// ============================================================================
// ShortcutManager Description
// ============================================================================

class ShortcutManagerDescription : public IDescribable {
public:
    QString className() const override { return "ShortcutManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages keyboard shortcuts and their assignments across the application.",
            "アプリケーション全体のキーボードショートカットとその割り当てを管理します。",
            "管理应用程序中的键盘快捷键及其分配。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"shortcuts", loc("List of registered shortcuts", "登録済みショートカットのリスト", "已注册快捷键列表"), "QMap<QString, QKeySequence>"},
            {"conflictResolution", loc("How to handle shortcut conflicts", "ショートカット競合の処理方法", "如何处理快捷键冲突"), "ConflictMode"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"registerShortcut", loc("Register a new shortcut", "新しいショートカットを登録", "注册新快捷键"), 
             "void", {"QString", "QKeySequence"}, {"id", "sequence"}},
            {"setShortcut", loc("Change shortcut for action", "アクションのショートカットを変更", "更改动作的快捷键"), 
             "void", {"QString", "QKeySequence"}, {"actionId", "sequence"}},
            {"loadPresets", loc("Load shortcut preset file", "ショートカットプリセットファイルを読み込み", "加载快捷键预设文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"savePresets", loc("Save shortcuts to file", "ショートカットをファイルに保存", "将快捷键保存到文件"), 
             "bool", {"QString"}, {"filePath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"InputOperator", "ArtifactPlaybackShortcuts"};
    }
};

// ============================================================================
// ArtifactPlaybackController Description
// ============================================================================

class PlaybackControllerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactCompositionPlaybackController"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Controls playback of the composition including play, pause, seek, and loop.",
            "再生、一時停止、シーク、ループを含むコンポジションの再生を制御します。",
            "控制合成的播放，包括播放、暂停、定位和循环。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"isPlaying", loc("Whether currently playing", "現在再生中か", "是否正在播放"), "bool"},
            {"currentTime", loc("Current playback time in seconds", "現在の再生時間（秒）", "当前播放时间（秒）"), "float"},
            {"currentFrame", loc("Current frame number", "現在のフレーム番号", "当前帧号"), "int64_t"},
            {"playbackSpeed", loc("Playback speed multiplier", "再生速度倍率", "播放速度倍数"), "float", "1.0"},
            {"loopEnabled", loc("Loop playback", "ループ再生", "循环播放"), "bool", "false"},
            {"loopStart", loc("Loop start frame", "ループ開始フレーム", "循环开始帧"), "int64_t"},
            {"loopEnd", loc("Loop end frame", "ループ終了フレーム", "循环结束帧"), "int64_t"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"play", loc("Start playback", "再生を開始", "开始播放"), "void"},
            {"pause", loc("Pause playback", "再生を一時停止", "暂停播放"), "void"},
            {"stop", loc("Stop and reset to start", "停止して開始位置にリセット", "停止并重置到开始位置"), "void"},
            {"seek", loc("Jump to time position", "時間位置にジャンプ", "跳转到时间位置"), "void", {"float"}, {"time"}},
            {"seekToFrame", loc("Jump to frame number", "フレーム番号にジャンプ", "跳转到帧号"), "void", {"int64_t"}, {"frame"}},
            {"stepForward", loc("Advance one frame", "1フレーム進める", "前进一帧"), "void"},
            {"stepBackward", loc("Go back one frame", "1フレーム戻る", "后退一帧"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactPlaybackShortcuts"};
    }
};

// ============================================================================
// InOutPoints Description
// ============================================================================

class InOutPointsDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactInOutPoints"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Defines the working region with in/out points for playback and export.",
            "再生とエクスポートのための作業領域をイン/アウトポイントで定義します。",
            "使用入出点定义播放和导出的工作区域。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"inPoint", loc("Start frame of working region", "作業領域の開始フレーム", "工作区域开始帧"), "int64_t", "0"},
            {"outPoint", loc("End frame of working region", "作業領域の終了フレーム", "工作区域结束帧"), "int64_t"},
            {"duration", loc("Length in frames", "長さ（フレーム）", "长度（帧）"), "int64_t"},
            {"durationSeconds", loc("Length in seconds", "長さ（秒）", "长度（秒）"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setInPoint", loc("Set in point at current time", "現在時刻にインポイントを設定", "在当前时间设置入点"), "void"},
            {"setOutPoint", loc("Set out point at current time", "現在時刻にアウトポイントを設定", "在当前时间设置出点"), "void"},
            {"clearInPoint", loc("Remove in point", "インポイントを削除", "移除入点"), "void"},
            {"clearOutPoint", loc("Remove out point", "アウトポイントを削除", "移除出点"), "void"},
            {"isInRange", loc("Check if frame is in working region", "フレームが作業領域内か確認", "检查帧是否在工作区域内"), "bool", {"int64_t"}, {"frame"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactPlaybackController"};
    }
};

// ============================================================================
// ColorSpace Description
// ============================================================================

class ColorSpaceDescription : public IDescribable {
public:
    QString className() const override { return "ColorSpace"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Defines a color space for color management and accurate color reproduction.",
            "カラーマネージメントと正確な色再現のための色空間を定義します。",
            "定义用于色彩管理和准确色彩再现的色彩空间。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Color space name (sRGB, Adobe RGB, etc)", "色空間名（sRGB、Adobe RGB等）", "色彩空间名称（sRGB、Adobe RGB等）"), "QString"},
            {"gamma", loc("Gamma value", "ガンマ値", "伽马值"), "float", "2.2"},
            {"whitePoint", loc("White point coordinates", "白色点座標", "白点坐标"), "QVector2D"},
            {"primaries", loc("RGB primary coordinates", "RGB原色座標", "RGB原色坐标"), "QVector<QVector2D>"},
            {"isHDR", loc("Whether HDR color space", "HDR色空間か", "是否HDR色彩空间"), "bool"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactColorManagement", "ColorGrading"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<AnimatableTransform2DDescription> _reg_AnimatableTransform2D("AnimatableTransform2D");
static AutoRegisterDescribable<AnimatableTransform3DDescription> _reg_AnimatableTransform3D("AnimatableTransform3D");
static AutoRegisterDescribable<KeyframeDescription> _reg_Keyframe("Keyframe");
static AutoRegisterDescribable<TimeRemapDescription> _reg_TimeRemap("TimeRemap");
static AutoRegisterDescribable<InputOperatorDescription> _reg_InputOperator("InputOperator");
static AutoRegisterDescribable<ShortcutManagerDescription> _reg_ShortcutManager("ShortcutManager");
static AutoRegisterDescribable<PlaybackControllerDescription> _reg_PlaybackController("ArtifactCompositionPlaybackController");
static AutoRegisterDescribable<InOutPointsDescription> _reg_InOutPoints("ArtifactInOutPoints");
static AutoRegisterDescribable<ColorSpaceDescription> _reg_ColorSpace("ColorSpace");

} // namespace ArtifactCore