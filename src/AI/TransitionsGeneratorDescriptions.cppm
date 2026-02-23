module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.TransitionsGeneratorDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// CrossDissolveTransition Description
// ============================================================================

class CrossDissolveTransitionDescription : public IDescribable {
public:
    QString className() const override { return "CrossDissolveTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Smoothly fades between two clips using opacity blending.",
            "不透明度ブレンドを使用して2つのクリップ間をスムーズにフェードします。",
            "使用不透明度混合在两个片段之间平滑淡入淡出。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "1.0", "0.1", "10.0"},
            {"easing", loc("Fade curve type", "フェードカーブタイプ", "淡入淡出曲线类型"), "QEasingCurve::Type", "Linear"},
            {"midpoint", loc("Position of 50% blend", "50%ブレンドの位置", "50%混合位置"), "float", "0.5"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "TransitionManager"};
    }
};

// ============================================================================
// WipeTransition Description
// ============================================================================

class WipeTransitionDescription : public IDescribable {
public:
    QString className() const override { return "WipeTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Reveals the next clip by wiping across the screen in a specified direction.",
            "指定された方向に画面をワイプして次のクリップを表示します。",
            "通过在指定方向上擦除屏幕来显示下一个片段。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"direction", loc("Wipe direction (Left, Right, Up, Down)", "ワイプ方向（左、右、上、下）", "擦除方向（左、右、上、下）"), "WipeDirection", "Left"},
            {"angle", loc("Custom angle in degrees", "カスタム角度（度）", "自定义角度（度）"), "float", "0.0", "0.0", "360.0"},
            {"feather", loc("Edge softness", "エッジの柔らかさ", "边缘柔和度"), "float", "0.0", "0.0", "100.0"},
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "1.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "SlideTransition"};
    }
};

// ============================================================================
// SlideTransition Description
// ============================================================================

class SlideTransitionDescription : public IDescribable {
public:
    QString className() const override { return "SlideTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Slides one clip out while sliding another in from the opposite side.",
            "一方のクリップをスライドアウトしながら反対側から別のクリップをスライドインします。",
            "将一个片段滑出，同时从相反方向滑入另一个片段。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"direction", loc("Slide direction", "スライド方向", "滑动方向"), "SlideDirection", "Left"},
            {"push", loc("Push outgoing clip", "アウトゴーイングクリップをプッシュ", "推推出片段"), "bool", "true"},
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "0.5"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "WipeTransition"};
    }
};

// ============================================================================
// ZoomTransition Description
// ============================================================================

class ZoomTransitionDescription : public IDescribable {
public:
    QString className() const override { return "ZoomTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Zooms in/out on one clip while zooming out/in on the next.",
            "一方のクリップをズームイン/アウトしながら次のクリップをズームアウト/インします。",
            "对一个片段放大/缩小，同时对下一个片段缩小/放大。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"zoomIn", loc("Zoom direction (true=in, false=out)", "ズーム方向（true=イン、false=アウト）", "缩放方向（true=放大，false=缩小）"), "bool", "true"},
            {"center", loc("Zoom center point", "ズーム中心点", "缩放中心点"), "QPointF", "(0.5, 0.5)"},
            {"scale", loc("Maximum zoom scale factor", "最大ズームスケール係数", "最大缩放系数"), "float", "2.0", "1.0", "5.0"},
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "0.75"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "RotateTransition"};
    }
};

// ============================================================================
// GlitchTransition Description
// ============================================================================

class GlitchTransitionDescription : public IDescribable {
public:
    QString className() const override { return "GlitchTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates a digital glitch effect transition with distortion and noise.",
            "歪みとノイズでデジタルグリッチエフェクトのトランジションを作成します。",
            "使用失真和噪点创建数字故障效果转场。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"intensity", loc("Glitch intensity", "グリッチ強度", "故障强度"), "float", "1.0", "0.0", "3.0"},
            {"blockSize", loc("Glitch block size", "グリッチブロックサイズ", "故障块大小"), "int", "20", "5", "100"},
            {"colorShift", loc("RGB color separation", "RGB色分離", "RGB颜色分离"), "float", "10.0", "0.0", "50.0"},
            {"frequency", loc("Glitch occurrence rate", "グリッチ発生頻度", "故障发生频率"), "float", "5.0"},
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "0.5"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "DigitalNoiseEffect"};
    }
};

// ============================================================================
// SpinTransition Description
// ============================================================================

class SpinTransitionDescription : public IDescribable {
public:
    QString className() const override { return "SpinTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Rotates one clip out while rotating the next clip in.",
            "一方のクリップを回転アウトしながら次のクリップを回転インします。",
            "将一个片段旋转出去，同时将下一个片段旋转进来。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"rotation", loc("Total rotation in degrees", "総回転角度（度）", "总旋转角度（度）"), "float", "360.0"},
            {"clockwise", loc("Rotation direction", "回転方向", "旋转方向"), "bool", "true"},
            {"center", loc("Rotation center point", "回転中心点", "旋转中心点"), "QPointF", "(0.5, 0.5)"},
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "0.75"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "ZoomTransition"};
    }
};

// ============================================================================
// CloneGenerator Description
// ============================================================================

class CloneGeneratorDescription : public IDescribable {
public:
    QString className() const override { return "CloneGenerator"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Generates multiple copies of layers with offset transforms for echo effects.",
            "エコーエフェクトのためにオフセットトランスフォームでレイヤーの複数コピーを生成します。",
            "使用偏移变换生成图层的多个副本以实现回声效果。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "CloneGenerator creates multiple visual copies of a source layer, each with its own "
            "transform offset, opacity, and timing. Useful for motion trails, echo effects, "
            "and kaleidoscope patterns.",
            "CloneGeneratorはソースレイヤーの複数のビジュアルコピーを作成し、それぞれ独自の"
            "トランスフォームオフセット、不透明度、タイミングを持ちます。モーショントレイル、"
            "エコーエフェクト、カレイドスコープパターンに便利です。",
            "CloneGenerator创建源图层的多个视觉副本，每个副本都有自己的变换偏移、不透明度和时间。"
            "适用于运动轨迹、回声效果和万花筒图案。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"cloneCount", loc("Number of clones to generate", "生成するクローン数", "要生成的克隆数"), "int", "5", "1", "50"},
            {"positionOffset", loc("Position offset per clone", "クローンごとの位置オフセット", "每个克隆的位置偏移"), "QVector2D", "(50, 0)"},
            {"rotationOffset", loc("Rotation offset per clone (degrees)", "クローンごとの回転オフセット（度）", "每个克隆的旋转偏移（度）"), "float", "0.0"},
            {"scaleDecay", loc("Scale multiplier per clone", "クローンごとのスケール倍率", "每个克隆的缩放倍数"), "float", "1.0"},
            {"opacityDecay", loc("Opacity multiplier per clone", "クローンごとの不透明度倍率", "每个克隆的不透明度倍数"), "float", "0.8"},
            {"timeOffset", loc("Time offset per clone (seconds)", "クローンごとの時間オフセット（秒）", "每个克隆的时间偏移（秒）"), "float", "0.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setSource", loc("Set source layer to clone", "クローンするソースレイヤーを設定", "设置要克隆的源图层"), 
             "void", {"ArtifactAbstractLayer*"}, {"source"}},
            {"generate", loc("Generate all clones", "全クローンを生成", "生成所有克隆"), "void"},
            {"clearClones", loc("Remove all clones", "全クローンを削除", "移除所有克隆"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactParticleGenerator", "ArtifactAbstractLayer"};
    }
};

// ============================================================================
// ArtifactParticleGenerator Description
// ============================================================================

class ParticleGeneratorDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactParticleGenerator"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Procedural particle system generator for creating dynamic visual effects.",
            "ダイナミックなビジュアルエフェクトを作成するためのプロシージャルパーティクルシステムジェネレーター。",
            "用于创建动态视觉效果的过程式粒子系统生成器。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"emissionRate", loc("Particles per second", "毎秒のパーティクル数", "每秒粒子数"), "float", "100.0"},
            {"burstCount", loc("Particles for burst mode", "バーストモードのパーティクル数", "爆发模式粒子数"), "int", "50"},
            {"emitFromShell", loc("Emit from shape edge only", "シェイプのエッジからのみ放出", "仅从形状边缘发射"), "bool", "false"},
            {"randomSeed", loc("Random seed for reproducibility", "再現性のためのランダムシード", "用于可重现性的随机种子"), "int", "0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEmitter", "ParticlePresets", "CloneGenerator"};
    }
};

// ============================================================================
// TextAnimator Description
// ============================================================================

class TextAnimatorDescription : public IDescribable {
public:
    QString className() const override { return "TextAnimator"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies per-character or per-word animations to text layers.",
            "テキストレイヤーに文字ごとまたは単語ごとのアニメーションを適用します。",
            "对文本图层应用逐字符或逐词动画。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "TextAnimator enables sophisticated text animations by applying transforms, "
            "colors, and other properties to individual characters or words over time. "
            "Supports typewriter, wave, scramble, and custom animation presets.",
            "TextAnimatorは、時間経過で個々の文字や単語にトランスフォーム、色、その他のプロパティを"
            "適用することで、洗練されたテキストアニメーションを実現します。タイプライター、ウェーブ、"
            "スクランブル、カスタムアニメーションプリセットをサポートします。",
            "TextAnimator通过随时间对单个字符或词应用变换、颜色和其他属性，实现复杂的文本动画。"
            "支持打字机、波浪、乱码和自定义动画预设。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"rangeUnit", loc("Character, Word, or Line", "文字、単語、または行", "字符、词或行"), "RangeUnit", "Character"},
            {"rangeStart", loc("Start of animation range", "アニメーション範囲の開始", "动画范围开始"), "float", "0.0", "0.0", "100.0"},
            {"rangeEnd", loc("End of animation range", "アニメーション範囲の終了", "动画范围结束"), "float", "100.0", "0.0", "100.0"},
            {"offset", loc("Timing offset per unit", "単位ごとのタイミングオフセット", "每单位时间偏移"), "float", "0.05"},
            {"easing", loc("Animation curve type", "アニメーションカーブタイプ", "动画曲线类型"), "QEasingCurve::Type", "OutQuad"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"applyPosition", loc("Animate position", "位置をアニメーション", "动画位置"), 
             "void", {"QVector2D", "float"}, {"offset", "time"}},
            {"applyRotation", loc("Animate rotation", "回転をアニメーション", "动画旋转"), 
             "void", {"float", "float"}, {"rotation", "time"}},
            {"applyOpacity", loc("Animate opacity", "不透明度をアニメーション", "动画不透明度"), 
             "void", {"float", "float"}, {"opacity", "time"}},
            {"applyScale", loc("Animate scale", "スケールをアニメーション", "动画缩放"), 
             "void", {"QVector2D", "float"}, {"scale", "time"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactTextLayer", "Keyframe"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<CrossDissolveTransitionDescription> _reg_CrossDissolveTransition("CrossDissolveTransition");
static AutoRegisterDescribable<WipeTransitionDescription> _reg_WipeTransition("WipeTransition");
static AutoRegisterDescribable<SlideTransitionDescription> _reg_SlideTransition("SlideTransition");
static AutoRegisterDescribable<ZoomTransitionDescription> _reg_ZoomTransition("ZoomTransition");
static AutoRegisterDescribable<GlitchTransitionDescription> _reg_GlitchTransition("GlitchTransition");
static AutoRegisterDescribable<SpinTransitionDescription> _reg_SpinTransition("SpinTransition");
static AutoRegisterDescribable<CloneGeneratorDescription> _reg_CloneGenerator("CloneGenerator");
static AutoRegisterDescribable<ParticleGeneratorDescription> _reg_ParticleGenerator("ArtifactParticleGenerator");
static AutoRegisterDescribable<TextAnimatorDescription> _reg_TextAnimator("TextAnimator");

} // namespace ArtifactCore