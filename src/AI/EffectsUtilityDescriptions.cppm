module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.EffectsUtilityDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// GaussianBlurEffect Description
// ============================================================================

class GaussianBlurEffectDescription : public IDescribable {
public:
    QString className() const override { return "GaussianBlurEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies gaussian blur to create soft, out-of-focus effect.",
            "ガウシアンブラーを適用してソフトなフォーカスアウトエフェクトを作成します。",
            "应用高斯模糊以创建柔和的失焦效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"radius", loc("Blur radius in pixels", "ブラー半径（ピクセル）", "模糊半径（像素）"), "float", "10.0", "0.0", "100.0"},
            {"quality", loc("Sampling quality level", "サンプリング品質レベル", "采样质量级别"), "int", "3", "1", "10"},
            {"direction", loc("Horizontal, Vertical, or Both", "水平、垂直、または両方", "水平、垂直或两者"), "BlurDirection", "Both"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "MotionBlurEffect"};
    }
};

// ============================================================================
// GlowEffect Description
// ============================================================================

class GlowEffectDescription : public IDescribable {
public:
    QString className() const override { return "GlowEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates a glowing halo effect around bright areas of the image.",
            "画像の明るい領域の周囲にグローハローエフェクトを作成します。",
            "在图像明亮区域周围创建发光光晕效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"threshold", loc("Brightness threshold for glow", "グローの明るさ閾値", "发光亮度阈值"), "float", "0.5", "0.0", "1.0"},
            {"intensity", loc("Glow intensity", "グロー強度", "发光强度"), "float", "1.0", "0.0", "5.0"},
            {"radius", loc("Glow spread radius", "グロー拡散半径", "发光扩散半径"), "float", "20.0", "0.0", "100.0"},
            {"color", loc("Tint color for glow", "グローのティント色", "发光色调颜色"), "QColor", "white"},
            {"blendMode", loc("How glow blends with source", "グローとソースのブレンド方法", "发光与源混合方式"), "BlendMode", "Screen"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "BloomEffect"};
    }
};

// ============================================================================
// BloomEffect Description
// ============================================================================

class BloomEffectDescription : public IDescribable {
public:
    QString className() const override { return "BloomEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies realistic lens bloom effect to bright highlights.",
            "明るいハイライトにリアルなレンズブルームエフェクトを適用します。",
            "对明亮高光应用逼真的镜头泛光效果。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "BloomEffect simulates the way bright light bleeds across camera sensors and lenses, "
            "creating realistic lens flares and halation around overexposed areas.",
            "BloomEffectは、明るい光がカメラセンサーやレンズに広がる様子をシミュレートし、"
            "露出過多領域の周囲にリアルなレンズフレアとハレーションを作成します。",
            "BloomEffect模拟明亮光线在相机传感器和镜头上的扩散方式，"
            "在过曝区域周围创建逼真的镜头光晕和光晕效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"threshold", loc("Luminance threshold", "輝度閾値", "亮度阈值"), "float", "0.8", "0.0", "1.0"},
            {"softness", loc("Edge softness of bloom", "ブルームのエッジ柔らかさ", "泛光边缘柔和度"), "float", "0.5", "0.0", "1.0"},
            {"intensity", loc("Bloom strength", "ブルーム強度", "泛光强度"), "float", "1.0"},
            {"lensFlare", loc("Enable lens flare", "レンズフレアを有効化", "启用镜头光晕"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"GlowEffect", "ArtifactAbstractEffect"};
    }
};

// ============================================================================
// NoiseEffect Description
// ============================================================================

class NoiseEffectDescription : public IDescribable {
public:
    QString className() const override { return "NoiseEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Adds film grain or digital noise to the image for texture or vintage look.",
            "テクスチャやヴィンテージルックのために画像にフィルムグレインやデジタルノイズを追加します。",
            "为图像添加胶片颗粒或数字噪点以获得纹理或复古外观。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"amount", loc("Noise intensity", "ノイズ強度", "噪点强度"), "float", "0.1", "0.0", "1.0"},
            {"size", loc("Grain particle size", "グレイン粒子サイズ", "颗粒粒子大小"), "float", "1.0", "0.5", "5.0"},
            {"type", loc("Film, Digital, or Static", "フィルム、デジタル、または静止", "胶片、数字或静态"), "NoiseType", "Film"},
            {"colored", loc("Use RGB noise instead of luminance", "輝度の代わりにRGBノイズを使用", "使用RGB噪点代替亮度"), "bool", "true"},
            {"animate", loc("Animate noise over time", "ノイズを時間でアニメーション", "随时间动画噪点"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactFilmEffects", "ArtifactAbstractEffect"};
    }
};

// ============================================================================
// SharpenEffect Description
// ============================================================================

class SharpenEffectDescription : public IDescribable {
public:
    QString className() const override { return "SharpenEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Enhances edge contrast to make details appear sharper and clearer.",
            "エッジコントラストを強調してディテールをより鮮明に見せます。",
            "增强边缘对比度，使细节看起来更清晰。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"amount", loc("Sharpening strength", "シャープ化強度", "锐化强度"), "float", "0.5", "0.0", "2.0"},
            {"radius", loc("Edge detection radius", "エッジ検出半径", "边缘检测半径"), "float", "1.0", "0.1", "10.0"},
            {"threshold", loc("Minimum contrast for sharpening", "シャープ化の最小コントラスト", "锐化的最小对比度"), "float", "0.0", "0.0", "1.0"},
            {"mode", loc("Unsharp Mask or High Pass", "アンシャープマスクまたはハイパス", "USM或高通"), "SharpenMode"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "GaussianBlurEffect"};
    }
};

// ============================================================================
// VignetteEffect Description
// ============================================================================

class VignetteEffectDescription : public IDescribable {
public:
    QString className() const override { return "VignetteEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Darkens or lightens the edges of the image to focus attention on center.",
            "中央に注目を集めるために画像の端を暗くまたは明るくします。",
            "使图像边缘变暗或变亮以将注意力集中在中心。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"amount", loc("Vignette intensity", "ビネット強度", "暗角强度"), "float", "0.5", "-1.0", "1.0"},
            {"radius", loc("Size of unaffected center", "影響しない中央のサイズ", "不受影响中心的大小"), "float", "0.5", "0.0", "1.0"},
            {"softness", loc("Edge softness", "エッジの柔らかさ", "边缘柔和度"), "float", "0.5", "0.0", "1.0"},
            {"roundness", loc("Round vs rectangular shape", "丸みと四角形の形状", "圆形与矩形形状"), "float", "1.0", "0.0", "1.0"},
            {"color", loc("Vignette color", "ビネット色", "暗角颜色"), "QColor", "black"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactFilmEffects", "ArtifactAbstractEffect"};
    }
};

// ============================================================================
// EmbossEffect Description
// ============================================================================

class EmbossEffectDescription : public IDescribable {
public:
    QString className() const override { return "EmbossEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates a raised or recessed 3D surface effect from image luminance.",
            "画像の輝度から隆起または凹んだ3D表面エフェクトを作成します。",
            "从图像亮度创建凸起或凹陷的3D表面效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"strength", loc("Emboss depth strength", "エンボス深度強度", "浮雕深度强度"), "float", "1.0"},
            {"direction", loc("Light direction angle", "光の方向角度", "光线方向角度"), "float", "45.0", "0.0", "360.0"},
            {"height", loc("Surface height map scale", "表面ハイトマップスケール", "表面高度图缩放"), "float", "1.0"},
            {"preserveColor", loc("Keep original colors", "元の色を保持", "保持原始颜色"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "BumpMapEffect"};
    }
};

// ============================================================================
// PixelateEffect Description
// ============================================================================

class PixelateEffectDescription : public IDescribable {
public:
    QString className() const override { return "PixelateEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Reduces image resolution by creating large pixel blocks for mosaic effect.",
            "モザイクエフェクトのために大きなピクセルブロックを作成して解像度を下げます。",
            "通过创建大像素块降低图像分辨率以实现马赛克效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"pixelSize", loc("Size of each pixel block", "各ピクセルブロックのサイズ", "每个像素块的大小"), "int", "8", "1", "100"},
            {"mode", loc("Average, Nearest, or Custom", "平均、最近傍、またはカスタム", "平均、最近邻或自定义"), "PixelateMode", "Average"},
            {"shape", loc("Square, Circle, or Diamond", "正方形、円、または菱形", "正方形、圆形或菱形"), "PixelShape", "Square"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "MosaicEffect"};
    }
};

// ============================================================================
// MotionEstimator Description
// ============================================================================

class MotionEstimatorDescription : public IDescribable {
public:
    QString className() const override { return "MotionEstimator"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Analyzes video frames to detect and track motion for blur and stabilization.",
            "ブラーと安定化のために動画フレームを解析して動きを検出・追跡します。",
            "分析视频帧以检测和跟踪运动，用于模糊和稳定。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"sensitivity", loc("Motion detection sensitivity", "動き検出感度", "运动检测灵敏度"), "float", "1.0"},
            {"smoothing", loc("Motion smoothing factor", "動きスムージング係数", "运动平滑系数"), "float", "0.5"},
            {"trackingMode", loc("Point, Region, or Global", "ポイント、領域、またはグローバル", "点、区域或全局"), "TrackingMode"},
            {"featureCount", loc("Number of tracking features", "追跡特徴点数", "跟踪特征点数"), "int", "100"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"MotionBlurEffect", "StabilizerEffect"};
    }
};

// ============================================================================
// LayerGroup Description
// ============================================================================

class LayerGroupDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactLayerGroup"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Groups multiple layers together for unified transform and effect application.",
            "統一されたトランスフォームとエフェクト適用のために複数のレイヤーをグループ化します。",
            "将多个图层组合在一起以进行统一的变换和效果应用。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"layerCount", loc("Number of layers in group", "グループ内のレイヤー数", "组内的图层数"), "int"},
            {"collapsed", loc("Whether group is collapsed in timeline", "タイムラインでグループが折りたたまれているか", "组在时间线中是否折叠"), "bool"},
            {"blendTogether", loc("Apply blend mode to group as whole", "グループ全体にブレンドモードを適用", "将混合模式应用于整体组"), "bool"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addLayer", loc("Add layer to group", "レイヤーをグループに追加", "将图层添加到组"), 
             "void", {"ArtifactAbstractLayer*"}, {"layer"}},
            {"removeLayer", loc("Remove layer from group", "レイヤーをグループから削除", "从组中移除图层"), 
             "void", {"ArtifactAbstractLayer*"}, {"layer"}},
            {"moveUp", loc("Move layer up in stack", "レイヤーをスタックの上に移動", "在堆栈中上移图层"), 
             "void", {"int"}, {"index"}},
            {"moveDown", loc("Move layer down in stack", "レイヤーをスタックの下に移動", "在堆栈中下移图层"), 
             "void", {"int"}, {"index"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactComposition"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<GaussianBlurEffectDescription> _reg_GaussianBlurEffect("GaussianBlurEffect");
static AutoRegisterDescribable<GlowEffectDescription> _reg_GlowEffect("GlowEffect");
static AutoRegisterDescribable<BloomEffectDescription> _reg_BloomEffect("BloomEffect");
static AutoRegisterDescribable<NoiseEffectDescription> _reg_NoiseEffect("NoiseEffect");
static AutoRegisterDescribable<SharpenEffectDescription> _reg_SharpenEffect("SharpenEffect");
static AutoRegisterDescribable<VignetteEffectDescription> _reg_VignetteEffect("VignetteEffect");
static AutoRegisterDescribable<EmbossEffectDescription> _reg_EmbossEffect("EmbossEffect");
static AutoRegisterDescribable<PixelateEffectDescription> _reg_PixelateEffect("PixelateEffect");
static AutoRegisterDescribable<MotionEstimatorDescription> _reg_MotionEstimator("MotionEstimator");
static AutoRegisterDescribable<LayerGroupDescription> _reg_LayerGroup("ArtifactLayerGroup");

} // namespace ArtifactCore