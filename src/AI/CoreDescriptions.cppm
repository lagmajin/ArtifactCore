module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.Descriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ParticleSystem Description
// ============================================================================

class ParticleSystemDescription : public IDescribable {
public:
    QString className() const override { return "ParticleSystem"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages multiple particle emitters and renders them as a unified system.",
            "複数のパーティクルエミッターを管理し、統一されたシステムとしてレンダリングします。",
            "管理多个粒子发射器并将它们作为一个统一系统进行渲染。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ParticleSystem is a container for ParticleEmitter objects. It handles simulation updates, "
            "rendering with blend modes and sorting, and provides time control for slow-motion or fast-forward effects.",
            "ParticleSystemはParticleEmitterオブジェクトのコンテナです。シミュレーション更新、"
            "ブレンドモードとソートによるレンダリング、スローモーションや早送りエフェクトのための時間制御を提供します。",
            "ParticleSystem是ParticleEmitter对象的容器。它处理模拟更新、混合模式和排序渲染，"
            "并提供慢动作或快进效果的时间控制。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"paused", loc("Whether simulation is paused", "シミュレーションが一時停止中か", "模拟是否暂停"), "bool", "false"},
            {"timeScale", loc("Time multiplier for slow-mo/fast-forward", "スローモーション/早送りの時間倍率", "慢动作/快进的时间倍率"), "float", "1.0", "0.0", "10.0"},
            {"time", loc("Current simulation time", "現在のシミュレーション時間", "当前模拟时间"), "float", "0.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"createEmitter", loc("Create and add a new emitter", "新しいエミッターを作成・追加", "创建并添加新发射器"), "ParticleEmitter*"},
            {"update", loc("Advance simulation by delta time", "デルタタイム分シミュレーションを進める", "按时间增量推进模拟"), "void", {"float"}, {"deltaTime"}},
            {"render", loc("Render all particles to target", "全パーティクルをターゲットにレンダリング", "将所有粒子渲染到目标"), "void", {"QPainter&", "QTransform"}, {"painter", "transform"}},
            {"clear", loc("Remove all emitters and particles", "全エミッターとパーティクルを削除", "移除所有发射器和粒子"), "void"},
            {"preWarm", loc("Pre-simulate for smooth start", "スムーズな開始のために事前シミュレート", "预模拟以平滑开始"), "void", {"float"}, {"duration"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEmitter", "ParticleEffector", "ParticleRenderSettings", "ArtifactParticleLayer"};
    }
};

// ============================================================================
// ParticleLayer Description
// ============================================================================

class ParticleLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactParticleLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer type that renders particle systems in the composition timeline.",
            "コンポジションタイムラインでパーティクルシステムをレンダリングするレイヤータイプ。",
            "在合成时间线中渲染粒子系统的图层类型。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactParticleLayer integrates particle systems into the composition. It supports presets, "
            "JSON serialization for project saving, and automatic time synchronization with the timeline.",
            "ArtifactParticleLayerはパーティクルシステムをコンポジションに統合します。プリセット、"
            "プロジェクト保存のためのJSONシリアライズ、タイムラインとの自動時間同期をサポートします。",
            "ArtifactParticleLayer将粒子系统集成到合成中。它支持预设、用于项目保存的JSON序列化，"
            "以及与时间线的自动时间同步。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"emitterCount", loc("Number of emitters in the layer", "レイヤー内のエミッター数", "图层中的发射器数量"), "int", "0"},
            {"isPlaying", loc("Whether the particle system is playing", "パーティクルシステムが再生中か", "粒子系统是否正在播放"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addEmitter", loc("Add an emitter with optional params", "パラメータ指定でエミッターを追加", "添加带可选参数的发射器"), "ParticleEmitter*", {"EmitterParams"}, {"params"}},
            {"loadPreset", loc("Load a preset configuration", "プリセット設定を読み込み", "加载预设配置"), "void", {"QString"}, {"presetName"}},
            {"play", loc("Start particle simulation", "パーティクルシミュレーションを開始", "开始粒子模拟"), "void"},
            {"pause", loc("Pause particle simulation", "パーティクルシミュレーションを一時停止", "暂停粒子模拟"), "void"},
            {"reset", loc("Clear all particles and reset time", "全パーティクルを削除し時間をリセット", "清除所有粒子并重置时间"), "void"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Create particle layer with fire preset\n"
            "auto layer = createParticleLayer(\"fire\");\n"
            "layer->play();\n"
            "\n"
            "// Custom configuration\n"
            "auto* emitter = layer->addEmitter();\n"
            "emitter->params().rate = 100.0f;",
            
            "// 炎プリセットでパーティクルレイヤー作成\n"
            "auto layer = createParticleLayer(\"fire\");\n"
            "layer->play();\n"
            "\n"
            "// カスタム設定\n"
            "auto* emitter = layer->addEmitter();\n"
            "emitter->params().rate = 100.0f;",
            
            "// 使用火焰预设创建粒子图层\n"
            "auto layer = createParticleLayer(\"fire\");\n"
            "layer->play();\n"
            "\n"
            "// 自定义配置\n"
            "auto* emitter = layer->addEmitter();\n"
            "emitter->params().rate = 100.0f;"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleSystem", "ParticleEmitter", "ArtifactAbstractLayer", "ParticlePresets"};
    }
};

// ============================================================================
// AbstractTransition Description
// ============================================================================

class TransitionDescription : public IDescribable {
public:
    QString className() const override { return "AbstractTransition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for transition effects between two frames or layers.",
            "2つのフレームまたはレイヤー間のトランジションエフェクトの基底クラス。",
            "两个帧或图层之间转场效果的基类。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "AbstractTransition provides the foundation for all transition effects. Subclasses implement "
            "specific transition types like cross dissolve, wipe, slide, and zoom. The system supports "
            "easing curves for smooth animations and custom timing.",
            "AbstractTransitionは全てのトランジションエフェクトの基盤を提供します。サブクラスは"
            "クロスディゾルブ、ワイプ、スライド、ズームなどの具体的なトランジションタイプを実装します。"
            "システムはスムーズなアニメーションのためのイージングカーブとカスタムタイミングをサポートします。",
            "AbstractTransition为所有转场效果提供基础。子类实现具体的转场类型如交叉溶解、擦除、滑动和缩放。"
            "系统支持用于平滑动画的缓动曲线和自定义时间。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"duration", loc("Transition duration in seconds", "トランジション時間（秒）", "转场持续时间（秒）"), "float", "1.0", "0.0", "60.0"},
            {"easing", loc("Easing curve type for animation", "アニメーションのイージングカーブタイプ", "动画缓动曲线类型"), "QEasingCurve::Type", "Linear"},
            {"reverse", loc("Play transition in reverse", "トランジションを逆再生", "反向播放转场"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"CrossDissolveTransition", "WipeTransition", "SlideTransition", "ZoomTransition", 
                "GlitchTransition", "TransitionManager", "TransitionPresets"};
    }
};

// ============================================================================
// TransitionManager Description
// ============================================================================

class TransitionManagerDescription : public IDescribable {
public:
    QString className() const override { return "TransitionManager"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages all transition effects and provides a registry for custom transitions.",
            "全トランジションエフェクトを管理し、カスタムトランジションの登録を提供します。",
            "管理所有转场效果并提供自定义转场的注册。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"applyTransition", loc("Apply transition between two frames", "2つのフレーム間にトランジションを適用", "在两帧之间应用转场"), 
             "void", {"QString", "QImage&", "QImage&", "QImage&", "float"}, 
             {"name", "fromFrame", "toFrame", "output", "progress"}},
            {"availableTransitions", loc("Get list of registered transitions", "登録済みトランジションのリストを取得", "获取已注册转场列表"), "QStringList"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "TransitionFactory", "TransitionPresets"};
    }
};

// ============================================================================
// ForceEffector Description
// ============================================================================

class ForceEffectorDescription : public IDescribable {
public:
    QString className() const override { return "ForceEffector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies a constant force (like gravity) to all particles.",
            "全パーティクルに一定の力（重力など）を適用します。",
            "对所有粒子施加恒定力（如重力）。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"force", loc("Force vector (pixels/second²)", "力ベクトル（ピクセル/秒²）", "力向量（像素/秒²）"), "QVector3D", "(0, -100, 0)"},
            {"strength", loc("Force strength multiplier", "力の強さ倍率", "力强度倍数"), "float", "1.0"},
            {"enabled", loc("Whether effector is active", "エフェクターが有効か", "效果器是否激活"), "bool", "true"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Add gravity\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 200, 0);  // Downward\n"
            "emitter->addEffector(std::move(gravity));",
            
            "// 重力を追加\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 200, 0);  // 下向き\n"
            "emitter->addEffector(std::move(gravity));",
            
            "// 添加重力\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 200, 0);  // 向下\n"
            "emitter->addEffector(std::move(gravity));"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEffector", "VortexEffector", "TurbulenceEffector", "WindEffector"};
    }
};

// ============================================================================
// VortexEffector Description
// ============================================================================

class VortexEffectorDescription : public IDescribable {
public:
    QString className() const override { return "VortexEffector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Creates a swirling vortex effect that rotates particles around a center point.",
            "中心点の周りでパーティクルを回転させる渦エフェクトを作成します。",
            "创建使粒子围绕中心点旋转的漩涡效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Center of the vortex", "渦の中心位置", "漩涡中心位置"), "QVector3D", "(0, 0, 0)"},
            {"radius", loc("Vortex influence radius", "渦の影響半径", "漩涡影响半径"), "float", "100.0"},
            {"angularVelocity", loc("Rotation speed (degrees/second)", "回転速度（度/秒）", "旋转速度（度/秒）"), "float", "180.0"},
            {"tightness", loc("How tightly particles spiral", "パーティクルがどれだけ密らかに渦巻くか", "粒子螺旋的紧密程度"), "float", "1.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEffector", "ForceEffector", "TurbulenceEffector"};
    }
};

// ============================================================================
// ArtifactAbstractLayer Description
// ============================================================================

class AbstractLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAbstractLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for all layers in the composition timeline.",
            "コンポジションタイムライン内の全レイヤーの基底クラス。",
            "合成时间线中所有图层的基类。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactAbstractLayer defines the common interface for all layer types including visibility, "
            "blend modes, transforms, effects, and masks. Layers can be 2D or 3D and support time remapping.",
            "ArtifactAbstractLayerは、可視性、ブレンドモード、トランスフォーム、エフェクト、マスクなど、"
            "全レイヤータイプの共通インターフェースを定義します。レイヤーは2Dまたは3Dで、タイムリマップをサポートします。",
            "ArtifactAbstractLayer定义了所有图层类型的通用接口，包括可见性、混合模式、变换、效果和蒙版。"
            "图层可以是2D或3D，并支持时间重映射。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"layerName", loc("Display name of the layer", "レイヤーの表示名", "图层的显示名称"), "QString"},
            {"visible", loc("Whether layer is rendered", "レイヤーがレンダリングされるか", "图层是否被渲染"), "bool", "true"},
            {"blendMode", loc("How layer blends with others", "他のレイヤーとのブレンド方法", "图层与其他图层的混合方式"), "LAYER_BLEND_TYPE", "Normal"},
            {"is3D", loc("Whether layer exists in 3D space", "レイヤーが3D空間に存在するか", "图层是否存在于3D空间"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"draw", loc("Render the layer content", "レイヤーコンテンツをレンダリング", "渲染图层内容"), "void"},
            {"addEffect", loc("Add an effect to the layer", "レイヤーにエフェクトを追加", "向图层添加效果"), "void", {"shared_ptr<Effect>"}, {"effect"}},
            {"addMask", loc("Add a mask to the layer", "レイヤーにマスクを追加", "向图层添加蒙版"), "void", {"LayerMask"}, {"mask"}},
            {"transform2D", loc("Get 2D transform for animation", "アニメーション用2Dトランスフォームを取得", "获取用于动画的2D变换"), "AnimatableTransform2D&"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactImageLayer", "ArtifactVideoLayer", "ArtifactTextLayer", 
                "ArtifactShapeLayer", "ArtifactParticleLayer", "ArtifactAbstractEffect"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<ParticleSystemDescription> _reg_ParticleSystem("ParticleSystem");
static AutoRegisterDescribable<ParticleLayerDescription> _reg_ParticleLayer("ArtifactParticleLayer");
static AutoRegisterDescribable<TransitionDescription> _reg_Transition("AbstractTransition");
static AutoRegisterDescribable<TransitionManagerDescription> _reg_TransitionManager("TransitionManager");
static AutoRegisterDescribable<ForceEffectorDescription> _reg_ForceEffector("ForceEffector");
static AutoRegisterDescribable<VortexEffectorDescription> _reg_VortexEffector("VortexEffector");
static AutoRegisterDescribable<AbstractLayerDescription> _reg_AbstractLayer("ArtifactAbstractLayer");

} // namespace ArtifactCore