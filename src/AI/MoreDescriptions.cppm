module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.MoreDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ArtifactComposition Description
// ============================================================================

class CompositionDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactComposition"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A composition containing layers, effects, and timeline for video production.",
            "動画制作のためのレイヤー、エフェクト、タイムラインを含むコンポジション。",
            "包含图层、效果和时间线用于视频制作的合成。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "Composition is the main container for video projects. It manages layers in a stack, "
            "applies global effects, handles playback and rendering, and defines output settings "
            "like resolution, frame rate, and duration.",
            "Compositionは動画プロジェクトのメインコンテナです。スタック内のレイヤーを管理し、"
            "グローバルエフェクトを適用し、再生とレンダリングを処理し、解像度、フレームレート、"
            "デュレーションなどの出力設定を定義します。",
            "Composition是视频项目的主容器。它管理堆栈中的图层，应用全局效果，处理播放和渲染，"
            "并定义分辨率、帧率和持续时间等输出设置。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Composition name", "コンポジション名", "合成名称"), "QString"},
            {"width", loc("Composition width in pixels", "コンポジション幅（ピクセル）", "合成宽度（像素）"), "int", "1920"},
            {"height", loc("Composition height in pixels", "コンポジション高さ（ピクセル）", "合成高度（像素）"), "int", "1080"},
            {"frameRate", loc("Frames per second", "フレームレート（fps）", "帧率（fps）"), "float", "30.0"},
            {"duration", loc("Total duration in seconds", "総時間（秒）", "总时长（秒）"), "float"},
            {"currentFrame", loc("Current playback position", "現在の再生位置", "当前播放位置"), "int64_t"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addLayer", loc("Add a layer to the composition", "コンポジションにレイヤーを追加", "向合成添加图层"), 
             "void", {"ArtifactAbstractLayer*"}, {"layer"}},
            {"removeLayer", loc("Remove a layer", "レイヤーを削除", "移除图层"), "void", {"int"}, {"index"}},
            {"play", loc("Start playback", "再生を開始", "开始播放"), "void"},
            {"pause", loc("Pause playback", "再生を一時停止", "暂停播放"), "void"},
            {"renderFrame", loc("Render a single frame", "単一フレームをレンダリング", "渲染单帧"), "QImage", {"int64_t"}, {"frameNumber"}},
            {"render", loc("Render entire composition", "コンポジション全体をレンダリング", "渲染整个合成"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactProject", "ArtifactRenderController"};
    }
};

// ============================================================================
// ArtifactProject Description
// ============================================================================

class ProjectDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactProject"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Top-level project container managing compositions, assets, and settings.",
            "コンポジション、アセット、設定を管理するトップレベルのプロジェクトコンテナ。",
            "管理合成、素材和设置的顶级项目容器。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactProject is the root object for all video editing work. It contains multiple "
            "compositions, manages the asset library, stores project settings, and handles "
            "save/load operations.",
            "ArtifactProjectは全ての動画編集作業のルートオブジェクトです。複数のコンポジションを含み、"
            "アセットライブラリを管理し、プロジェクト設定を保存し、保存/読み込み操作を処理します。",
            "ArtifactProject是所有视频编辑工作的根对象。它包含多个合成，管理素材库，"
            "存储项目设置，并处理保存/加载操作。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"createComposition", loc("Create a new composition", "新しいコンポジションを作成", "创建新合成"), 
             "ArtifactComposition*", {"QString"}, {"name"}},
            {"save", loc("Save project to file", "プロジェクトをファイルに保存", "将项目保存到文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"load", loc("Load project from file", "ファイルからプロジェクトを読み込み", "从文件加载项目"), 
             "bool", {"QString"}, {"filePath"}},
            {"importAsset", loc("Import media asset", "メディアアセットをインポート", "导入媒体素材"), 
             "void", {"QString"}, {"filePath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactAssetManager", "ArtifactProjectSettings"};
    }
};

// ============================================================================
// TurbulenceEffector Description
// ============================================================================

class TurbulenceEffectorDescription : public IDescribable {
public:
    QString className() const override { return "TurbulenceEffector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Adds random, turbulent motion to particles using noise functions.",
            "ノイズ関数を使用してパーティクルにランダムな乱流動作を追加します。",
            "使用噪声函数为粒子添加随机湍流运动。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"frequency", loc("Noise frequency (detail level)", "ノイズ周波数（詳細レベル）", "噪声频率（细节级别）"), "float", "1.0"},
            {"amplitude", loc("Turbulence strength", "乱流の強さ", "湍流强度"), "float", "50.0"},
            {"octaves", loc("Number of noise layers", "ノイズレイヤー数", "噪声层数"), "float", "3.0"},
            {"evolution", loc("Time-based evolution", "時間ベースの変化", "基于时间的演变"), "float", "0.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEffector", "ForceEffector", "WindEffector"};
    }
};

// ============================================================================
// WindEffector Description
// ============================================================================

class WindEffectorDescription : public IDescribable {
public:
    QString className() const override { return "WindEffector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies directional wind force with optional turbulence.",
            "オプションの乱流付きで指向性の風の力を適用します。",
            "施加带有可选湍流的定向风力。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"windDirection", loc("Wind direction vector", "風の方向ベクトル", "风向向量"), "QVector3D", "(1, 0, 0)"},
            {"windStrength", loc("Wind force magnitude", "風力の大きさ", "风力大小"), "float", "50.0"},
            {"turbulence", loc("Random variation amount", "ランダム変動量", "随机变化量"), "float", "10.0"}
        );
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEffector", "ForceEffector", "TurbulenceEffector"};
    }
};

// ============================================================================
// AttractorEffector Description
// ============================================================================

class AttractorEffectorDescription : public IDescribable {
public:
    QString className() const override { return "AttractorEffector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Attracts particles toward a point in space.",
            "パーティクルを空間内のポイントに引き寄せます。",
            "将粒子吸引向空间中的某一点。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Attraction center point", "引力中心点", "吸引中心点"), "QVector3D", "(0, 0, 0)"},
            {"radius", loc("Attraction influence radius", "引力影響半径", "吸引影响半径"), "float", "200.0"},
            {"strength", loc("Attraction force strength", "引力の強さ", "吸引力强度"), "float", "1.0"},
            {"falloff", loc("Force falloff exponent", "力の減衰指数", "力衰减指数"), "float", "1.0"},
            {"killOnReach", loc("Kill particles that reach center", "中心に達したパーティクルを削除", "删除到达中心的粒子"), "bool", "false"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ParticleEffector", "RepellerEffector", "VortexEffector"};
    }
};

// ============================================================================
// ParticlePresets Description
// ============================================================================

class ParticlePresetsDescription : public IDescribable {
public:
    QString className() const override { return "ParticlePresets"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Factory class providing pre-configured particle emitter settings.",
            "事前設定されたパーティクルエミッター設定を提供するファクトリクラス。",
            "提供预配置粒子发射器设置的工厂类。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ParticlePresets contains static methods that return EmitterParams for common effects "
            "like fire, smoke, rain, snow, explosions, and more. Use these as starting points "
            "for your particle effects.",
            "ParticlePresetsには、炎、煙、雨、雪、爆発などの一般的なエフェクトのEmitterParamsを"
            "返す静的メソッドが含まれています。パーティクルエフェクトの開始点として使用してください。",
            "ParticlePresets包含返回常见效果（如火、烟、雨、雪、爆炸等）的EmitterParams的静态方法。"
            "将这些作为粒子效果的起点使用。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"fire", loc("Fire/flame preset", "炎/火炎プリセット", "火焰预设"), "EmitterParams"},
            {"smoke", loc("Smoke preset", "煙プリセット", "烟雾预设"), "EmitterParams"},
            {"rain", loc("Rain preset", "雨プリセット", "雨预设"), "EmitterParams"},
            {"snow", loc("Snow preset", "雪プリセット", "雪预设"), "EmitterParams"},
            {"explosion", loc("Explosion preset", "爆発プリセット", "爆炸预设"), "EmitterParams"},
            {"sparks", loc("Sparks preset", "火花プリセット", "火花预设"), "EmitterParams"},
            {"magic", loc("Magic/sparkle preset", "魔法/スパークルプリセット", "魔法/闪光预设"), "EmitterParams"},
            {"confetti", loc("Confetti preset", "紙吹雪プリセット", "彩纸预设"), "EmitterParams"},
            {"bubbles", loc("Bubbles preset", "泡プリセット", "气泡预设"), "EmitterParams"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Load fire preset\n"
            "EmitterParams params = ParticlePresets::fire();\n"
            "emitter->setParams(params);\n"
            "\n"
            "// Load and customize snow\n"
            "auto snowParams = ParticlePresets::snow();\n"
            "snowParams.rate = 200.0f;\n"
            "emitter->setParams(snowParams);",
            
            "// 炎プリセットを読み込み\n"
            "EmitterParams params = ParticlePresets::fire();\n"
            "emitter->setParams(params);\n"
            "\n"
            "// 雪を読み込んでカスタマイズ\n"
            "auto snowParams = ParticlePresets::snow();\n"
            "snowParams.rate = 200.0f;\n"
            "emitter->setParams(snowParams);",
            
            "// 加载火焰预设\n"
            "EmitterParams params = ParticlePresets::fire();\n"
            "emitter->setParams(params);\n"
            "\n"
            "// 加载并自定义雪\n"
            "auto snowParams = ParticlePresets::snow();\n"
            "snowParams.rate = 200.0f;\n"
            "emitter->setParams(snowParams);"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"EmitterParams", "ParticleEmitter", "ArtifactParticleLayer"};
    }
};

// ============================================================================
// TransitionPresets Description
// ============================================================================

class TransitionPresetsDescription : public IDescribable {
public:
    QString className() const override { return "TransitionPresets"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Factory class providing pre-configured transition effects.",
            "事前設定されたトランジションエフェクトを提供するファクトリクラス。",
            "提供预配置转场效果的工厂类。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"quickDissolve", loc("Fast cross dissolve", "高速クロスディゾルブ", "快速交叉溶解"), "CrossDissolveTransition*"},
            {"slowFade", loc("Slow fade transition", "スローフェードトランジション", "慢速淡入淡出"), "CrossDissolveTransition*"},
            {"smoothWipeLeft", loc("Smooth left wipe", "スムーズな左ワイプ", "平滑左擦除"), "WipeTransition*"},
            {"pushLeft", loc("Push left transition", "左プッシュトランジション", "左推动转场"), "SlideTransition*"},
            {"cinematicZoom", loc("Cinematic zoom effect", "シネマティックズームエフェクト", "电影式缩放效果"), "ZoomTransition*"},
            {"digitalGlitch", loc("Digital glitch effect", "デジタルグリッチエフェクト", "数字故障效果"), "GlitchTransition*"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AbstractTransition", "TransitionManager", "CrossDissolveTransition"};
    }
};

// ============================================================================
// ArtifactAbstractEffect Description
// ============================================================================

class AbstractEffectDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAbstractEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for all visual effects that can be applied to layers.",
            "レイヤーに適用可能な全てのビジュアルエフェクトの基底クラス。",
            "可应用于图层的所有视觉效果的基类。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactAbstractEffect defines the interface for image-processing effects. Effects can "
            "be stacked on layers, support CPU and GPU computation, and can be animated over time.",
            "ArtifactAbstractEffectは画像処理エフェクトのインターフェースを定義します。エフェクトは"
            "レイヤー上にスタックでき、CPUおよびGPU計算をサポートし、時間経過でアニメーション可能です。",
            "ArtifactAbstractEffect定义了图像处理效果的接口。效果可以堆叠在图层上，"
            "支持CPU和GPU计算，并可以随时间动画化。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"enabled", loc("Whether effect is active", "エフェクトが有効か", "效果是否激活"), "bool", "true"},
            {"computeMode", loc("CPU, GPU, or Auto", "CPU、GPU、または自動", "CPU、GPU或自动"), "ComputeMode", "Auto"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"apply", loc("Process input image to output", "入力画像を出力に処理", "将输入图像处理到输出"), 
             "void", {"ImageF32x4RGBAWithCache&", "ImageF32x4RGBAWithCache&"}, {"src", "dst"}},
            {"initialize", loc("Initialize effect resources", "エフェクトリソースを初期化", "初始化效果资源"), "bool"},
            {"release", loc("Release effect resources", "エフェクトリソースを解放", "释放效果资源"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "GaussianBlurEffect", "GlowEffect", "ChromaKeyEffect"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<CompositionDescription> _reg_Composition("ArtifactComposition");
static AutoRegisterDescribable<ProjectDescription> _reg_Project("ArtifactProject");
static AutoRegisterDescribable<TurbulenceEffectorDescription> _reg_TurbulenceEffector("TurbulenceEffector");
static AutoRegisterDescribable<WindEffectorDescription> _reg_WindEffector("WindEffector");
static AutoRegisterDescribable<AttractorEffectorDescription> _reg_AttractorEffector("AttractorEffector");
static AutoRegisterDescribable<ParticlePresetsDescription> _reg_ParticlePresets("ParticlePresets");
static AutoRegisterDescribable<TransitionPresetsDescription> _reg_TransitionPresets("TransitionPresets");
static AutoRegisterDescribable<AbstractEffectDescription> _reg_AbstractEffect("ArtifactAbstractEffect");

} // namespace ArtifactCore