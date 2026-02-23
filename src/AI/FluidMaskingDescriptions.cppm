module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.FluidMaskingDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// FluidSimulation Description
// ============================================================================

class FluidSimulationDescription : public IDescribable {
public:
    QString className() const override { return "FluidSimulation"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Real-time fluid dynamics simulation for smoke, fire, and liquid effects.",
            "煙、炎、液体エフェクトのためのリアルタイム流体ダイナミクスシミュレーション。",
            "用于烟雾、火焰和液体效果的实时流体动力学模拟。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "FluidSimulation implements Navier-Stokes equations for realistic fluid behavior. "
            "It supports density, velocity, and temperature fields for various effects like "
            "rising smoke, flowing water, and fire with customizable viscosity and diffusion.",
            "FluidSimulationはリアルな流体挙動のためにNavier-Stokes方程式を実装しています。"
            "上昇する煙、流れる水、火など様々なエフェクトのための密度、速度、温度フィールドをサポートし、"
            "カスタマイズ可能な粘度と拡散を提供します。",
            "FluidSimulation实现Navier-Stokes方程以实现逼真的流体行为。"
            "它支持密度、速度和温度场用于各种效果，如上升的烟雾、流动的水和火焰，"
            "具有可自定义的粘度和扩散。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"resolution", loc("Grid resolution for simulation", "シミュレーションのグリッド解像度", "模拟的网格分辨率"), "int", "128", "32", "512"},
            {"viscosity", loc("Fluid viscosity (thickness)", "流体粘度（厚さ）", "流体粘度（稠度）"), "float", "0.1", "0.0", "1.0"},
            {"diffusion", loc("Density diffusion rate", "密度拡散率", "密度扩散率"), "float", "0.0001", "0.0", "0.1"},
            {"buoyancy", loc("Upward force from heat", "熱による上向きの力", "热量产生的上升力"), "float", "1.0", "0.0", "5.0"},
            {"vorticity", loc("Swirling motion strength", "渦運動の強さ", "涡旋运动强度"), "float", "0.5", "0.0", "2.0"},
            {"dissipation", loc("Density decay rate", "密度減衰率", "密度衰减率"), "float", "0.99", "0.0", "1.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addDensity", loc("Add density at position", "位置に密度を追加", "在位置添加密度"), 
             "void", {"int", "int", "float"}, {"x", "y", "amount"}},
            {"addVelocity", loc("Add velocity at position", "位置に速度を追加", "在位置添加速度"), 
             "void", {"int", "int", "QVector2D"}, {"x", "y", "velocity"}},
            {"addTemperature", loc("Add heat at position", "位置に熱を追加", "在位置添加热量"), 
             "void", {"int", "int", "float"}, {"x", "y", "temp"}},
            {"step", loc("Advance simulation by timestep", "タイムステップ分シミュレーションを進める", "按时间步推进模拟"), 
             "void", {"float"}, {"dt"}},
            {"reset", loc("Clear all simulation data", "全シミュレーションデータをクリア", "清除所有模拟数据"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"FluidRenderer", "ParticleEmitter", "ForceEffector"};
    }
};

// ============================================================================
// FluidRenderer Description
// ============================================================================

class FluidRendererDescription : public IDescribable {
public:
    QString className() const override { return "FluidRenderer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Renders fluid simulation data to visual output with shading and coloring.",
            "シェーディングと色付けで流体シミュレーションデータをビジュアル出力にレンダリングします。",
            "使用着色和颜色将流体模拟数据渲染为视觉输出。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"colorMode", loc("Density, Temperature, or Velocity coloring", "密度、温度、または速度着色", "密度、温度或速度着色"), "FluidColorMode", "Density"},
            {"lowColor", loc("Color at minimum value", "最小値の色", "最小值的颜色"), "QColor", "black"},
            {"highColor", loc("Color at maximum value", "最大値の色", "最大值的颜色"), "QColor", "white"},
            {"threshold", loc("Minimum density to render", "レンダリングの最小密度", "渲染的最小密度"), "float", "0.1"},
            {"opacity", loc("Overall opacity", "全体の不透明度", "整体不透明度"), "float", "1.0"},
            {"blurAmount", loc("Softness of fluid edges", "流体エッジの柔らかさ", "流体边缘的柔和度"), "float", "0.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"FluidSimulation"};
    }
};

// ============================================================================
// RotoscopeMask Description
// ============================================================================

class RotoscopeMaskDescription : public IDescribable {
public:
    QString className() const override { return "RotoscopeMask"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Frame-by-frame animated mask for isolating objects in video.",
            "動画内のオブジェクトを分離するためのフレーム単位のアニメーションマスク。",
            "用于隔离视频中物体的逐帧动画蒙版。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "RotoscopeMask allows creating precise masks by drawing bezier paths that can be "
            "animated over time. Used for isolating subjects, removing backgrounds, or "
            "applying effects to specific areas in video.",
            "RotoscopeMaskは、時間でアニメーション可能なベジェパスを描画することで正確なマスクを作成できます。"
            "被写体の分離、背景の削除、動画内の特定領域へのエフェクト適用に使用されます。",
            "RotoscopeMask允许通过绘制可随时间动画化的贝塞尔路径来创建精确的蒙版。"
            "用于分离主体、移除背景或对视频中的特定区域应用效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"pathCount", loc("Number of mask paths", "マスクパス数", "蒙版路径数"), "int"},
            {"feather", loc("Edge softness", "エッジの柔らかさ", "边缘柔和度"), "float", "0.0"},
            {"motionBlur", loc("Motion blur for animated masks", "アニメーションマスクのモーションブラー", "动画蒙版的运动模糊"), "float", "0.0"},
            {"inverted", loc("Invert mask effect", "マスク効果を反転", "反转蒙版效果"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addPoint", loc("Add control point to path", "パスに制御点を追加", "向路径添加控制点"), 
             "void", {"int", "QPointF", "float"}, {"pathIndex", "point", "time"}},
            {"setPoint", loc("Move existing control point", "既存の制御点を移動", "移动现有控制点"), 
             "void", {"int", "int", "QPointF", "float"}, {"pathIndex", "pointIndex", "position", "time"}},
            {"interpolate", loc("Get interpolated path at time", "時間での補間パスを取得", "获取时间处的插值路径"), 
             "QPainterPath", {"float"}, {"time"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"LayerMask", "ArtifactVideoLayer"};
    }
};

// ============================================================================
// LumaKey Description
// ============================================================================

class LumaKeyDescription : public IDescribable {
public:
    QString className() const override { return "LumaKey"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates transparency based on luminance (brightness) values of the image.",
            "画像の輝度（明るさ）値に基づいて透明度を作成します。",
            "根据图像的亮度值创建透明度。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"threshold", loc("Luminance threshold for key", "キーの輝度閾値", "抠像亮度阈值"), "float", "0.5", "0.0", "1.0"},
            {"tolerance", loc("Range of similar values", "類似値の範囲", "相似值的范围"), "float", "0.1", "0.0", "1.0"},
            {"edgeSoftness", loc("Edge feathering", "エッジのぼかし", "边缘羽化"), "float", "5.0", "0.0", "50.0"},
            {"invert", loc("Key dark instead of bright", "明るい代わりに暗いをキー", "抠暗色而非亮色"), "bool", "false"},
            {"clipBlack", loc("Minimum output opacity", "最小出力不透明度", "最小输出不透明度"), "float", "0.0"},
            {"clipWhite", loc("Maximum output opacity", "最大出力不透明度", "最大输出不透明度"), "float", "1.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ChromaKeyEffect", "LayerMask"};
    }
};

// ============================================================================
// DifferenceKey Description
// ============================================================================

class DifferenceKeyDescription : public IDescribable {
public:
    QString className() const override { return "DifferenceKey"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates transparency by comparing differences between two images.",
            "2つの画像間の違いを比較して透明度を作成します。",
            "通过比较两个图像之间的差异创建透明度。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "DifferenceKey compares a source image against a reference (clean plate) and "
            "creates transparency where they match. Useful for removing static backgrounds "
            "or isolating moving objects.",
            "DifferenceKeyはソース画像を参照（クリーンプレート）と比較し、"
            "一致する部分に透明度を作成します。静止背景の削除や動くオブジェクトの分離に便利です。",
            "DifferenceKey将源图像与参考（干净板）进行比较，并在它们匹配的地方创建透明度。"
            "适用于移除静态背景或隔离移动物体。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"referenceImage", loc("Clean plate for comparison", "比較用クリーンプレート", "用于比较的干净板"), "QImage"},
            {"threshold", loc("Difference threshold", "差分閾値", "差异阈值"), "float", "0.1", "0.0", "1.0"},
            {"tolerance", loc("Tolerance for matching", "マッチング許容度", "匹配容差"), "float", "0.05", "0.0", "0.5"},
            {"softness", loc("Edge softness", "エッジの柔らかさ", "边缘柔和度"), "float", "5.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ChromaKeyEffect", "LumaKey"};
    }
};

// ============================================================================
// TrackMask Description
// ============================================================================

class TrackMaskDescription : public IDescribable {
public:
    QString className() const override { return "TrackMask"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Automatically tracks motion to animate mask position over time.",
            "時間経過でマスク位置をアニメーションするために動きを自動追跡します。",
            "自动跟踪运动以随时间动画化蒙版位置。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"trackPoint", loc("Point to track", "追跡するポイント", "要跟踪的点"), "QPointF"},
            {"searchRegion", loc("Search area size", "検索エリアサイズ", "搜索区域大小"), "int", "50"},
            {"trackMode", loc("Point, Area, or Feature tracking", "ポイント、エリア、または特徴追跡", "点、区域或特征跟踪"), "TrackMode"},
            {"confidence", loc("Current tracking confidence", "現在の追跡信頼度", "当前跟踪置信度"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"analyze", loc("Track forward from current frame", "現在のフレームから前方追跡", "从当前帧向前跟踪"), 
             "void", {"int"}, {"frameCount"}},
            {"analyzeBackward", loc("Track backward from current frame", "現在のフレームから後方追跡", "从当前帧向后跟踪"), 
             "void", {"int"}, {"frameCount"}},
            {"applyTrack", loc("Apply tracked motion to mask", "追跡した動きをマスクに適用", "将跟踪的运动应用于蒙版"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"RotoscopeMask", "MotionEstimator"};
    }
};

// ============================================================================
// ParticleCollider Description
// ============================================================================

class ParticleColliderDescription : public IDescribable {
public:
    QString className() const override { return "ParticleCollider"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Handles collision detection and response for particles against shapes.",
            "シェイプに対するパーティクルの衝突検出と応答を処理します。",
            "处理粒子与形状的碰撞检测和响应。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"bounce", loc("Bounce factor on collision", "衝突時のバウンス係数", "碰撞时的弹跳系数"), "float", "0.5", "0.0", "1.0"},
            {"friction", loc("Velocity reduction on collision", "衝突時の速度減少", "碰撞时的速度减少"), "float", "0.1", "0.0", "1.0"},
            {"lifetimeLoss", loc("Lifetime reduction on collision", "衝突時の寿命減少", "碰撞时的寿命减少"), "float", "0.0", "0.0", "1.0"},
            {"killOnCollision", loc("Kill particles on collision", "衝突時にパーティクルを削除", "碰撞时删除粒子"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addBoxCollider", loc("Add rectangular collision area", "矩形衝突エリアを追加", "添加矩形碰撞区域"), 
             "void", {"QRectF"}, {"rect"}},
            {"addCircleCollider", loc("Add circular collision area", "円形衝突エリアを追加", "添加圆形碰撞区域"), 
             "void", {"QPointF", "float"}, {"center", "radius"}},
            {"addPathCollider", loc("Add custom path collision", "カスタムパス衝突を追加", "添加自定义路径碰撞"), 
             "void", {"QPainterPath"}, {"path"}},
            {"clearColliders", loc("Remove all collision shapes", "全衝突シェイプを削除", "移除所有碰撞形状"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEmitter", "ParticleEffector"};
    }
};

// ============================================================================
// EmitterParams Description
// ============================================================================

class EmitterParamsDescription : public IDescribable {
public:
    QString className() const override { return "EmitterParams"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Configuration parameters for particle emitter behavior.",
            "パーティクルエミッターの動作設定パラメータ。",
            "粒子发射器行为的配置参数。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "EmitterParams is a data structure containing all configurable properties "
            "for a particle emitter. It includes emission settings, particle properties, "
            "rendering options, and physics parameters.",
            "EmitterParamsはパーティクルエミッターの全設定可能プロパティを含むデータ構造です。"
            "放出設定、パーティクルプロパティ、レンダリングオプション、物理パラメータが含まれます。",
            "EmitterParams是一个数据结构，包含粒子发射器的所有可配置属性。"
            "它包括发射设置、粒子属性、渲染选项和物理参数。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"rate", loc("Particles per second", "毎秒のパーティクル数", "每秒粒子数"), "float", "50.0"},
            {"burstCount", loc("Particles for burst mode", "バーストモードのパーティクル数", "爆发模式粒子数"), "int", "0"},
            {"lifeMin", loc("Minimum particle lifetime", "パーティクル最小寿命", "粒子最小寿命"), "float", "1.0"},
            {"lifeMax", loc("Maximum particle lifetime", "パーティクル最大寿命", "粒子最大寿命"), "float", "3.0"},
            {"speedMin", loc("Minimum initial speed", "初期速度最小値", "初始速度最小值"), "float", "50.0"},
            {"speedMax", loc("Maximum initial speed", "初期速度最大値", "初始速度最大值"), "float", "100.0"},
            {"sizeStart", loc("Size at birth", "生成時のサイズ", "出生时的大小"), "float", "5.0"},
            {"sizeEnd", loc("Size at death", "消滅時のサイズ", "死亡时的大小"), "float", "0.0"},
            {"colorStart", loc("Color at birth", "生成時の色", "出生时的颜色"), "QColor", "white"},
            {"colorEnd", loc("Color at death", "消滅時の色", "死亡时的颜色"), "QColor", "transparent"},
            {"gravity", loc("Downward acceleration", "下向き加速度", "向下加速度"), "float", "100.0"},
            {"blendMode", loc("Particle blend mode", "パーティクルブレンドモード", "粒子混合模式"), "BlendMode", "Add"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEmitter", "ParticlePresets"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<FluidSimulationDescription> _reg_FluidSimulation("FluidSimulation");
static AutoRegisterDescribable<FluidRendererDescription> _reg_FluidRenderer("FluidRenderer");
static AutoRegisterDescribable<RotoscopeMaskDescription> _reg_RotoscopeMask("RotoscopeMask");
static AutoRegisterDescribable<LumaKeyDescription> _reg_LumaKey("LumaKey");
static AutoRegisterDescribable<DifferenceKeyDescription> _reg_DifferenceKey("DifferenceKey");
static AutoRegisterDescribable<TrackMaskDescription> _reg_TrackMask("TrackMask");
static AutoRegisterDescribable<ParticleColliderDescription> _reg_ParticleCollider("ParticleCollider");
static AutoRegisterDescribable<EmitterParamsDescription> _reg_EmitterParams("EmitterParams");

} // namespace ArtifactCore