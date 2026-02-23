module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.ImageMathDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ImageF32x4RGBA Description
// ============================================================================

class ImageF32x4RGBADescription : public IDescribable {
public:
    QString className() const override { return "ImageF32x4RGBA"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "High dynamic range image with 32-bit float RGBA channels for color-accurate processing.",
            "色精度処理のための32ビット浮動小数点RGBAチャンネルを持つハイダイナミックレンジ画像。",
            "具有32位浮点RGBA通道的高动态范围图像，用于精确颜色处理。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ImageF32x4RGBA stores image data as 32-bit floating point values per channel, "
            "allowing for HDR content and color-accurate processing without clipping. "
            "This format is used internally for all effect processing.",
            "ImageF32x4RGBAはチャンネルごとに32ビット浮動小数点値として画像データを保存し、"
            "HDRコンテンツとクリッピングなしの色精度処理を可能にします。"
            "このフォーマットはすべてのエフェクト処理で内部的に使用されます。",
            "ImageF32x4RGBA将图像数据存储为每通道32位浮点值，"
            "允许HDR内容和无裁剪的精确颜色处理。"
            "此格式在内部用于所有效果处理。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"width", loc("Image width in pixels", "画像幅（ピクセル）", "图像宽度（像素）"), "int"},
            {"height", loc("Image height in pixels", "画像高さ（ピクセル）", "图像高度（像素）"), "int"},
            {"channelCount", loc("Number of color channels", "カラーチャンネル数", "颜色通道数"), "int", "4"},
            {"bitDepth", loc("Bits per channel", "チャンネルあたりのビット数", "每通道位数"), "int", "32"},
            {"hasAlpha", loc("Whether alpha channel exists", "アルファチャンネルが存在するか", "是否存在Alpha通道"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"pixel", loc("Get pixel at coordinates", "座標のピクセルを取得", "获取坐标处的像素"), 
             "QVector4D", {"int", "int"}, {"x", "y"}},
            {"setPixel", loc("Set pixel at coordinates", "座標にピクセルを設定", "设置坐标处的像素"), 
             "void", {"int", "int", "QVector4D"}, {"x", "y", "color"}},
            {"toQImage", loc("Convert to 8-bit QImage", "8ビットQImageに変換", "转换为8位QImage"), 
             "QImage"},
            {"fromQImage", loc("Create from QImage", "QImageから作成", "从QImage创建"), 
             "void", {"QImage"}, {"image"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "ColorSpace"};
    }
};

// ============================================================================
// BezierCurve Description
// ============================================================================

class BezierCurveDescription : public IDescribable {
public:
    QString className() const override { return "BezierCurve"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Represents a cubic Bezier curve with control points for path animation.",
            "パスアニメーション用のコントロールポイントを持つ3次ベジェ曲線を表します。",
            "表示具有控制点的三次贝塞尔曲线，用于路径动画。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"p0", loc("Start point", "開始点", "起点"), "QPointF"},
            {"p1", loc("First control point", "第1制御点", "第一控制点"), "QPointF"},
            {"p2", loc("Second control point", "第2制御点", "第二控制点"), "QPointF"},
            {"p3", loc("End point", "終了点", "终点"), "QPointF"},
            {"length", loc("Approximate curve length", "近似曲線長", "近似曲线长度"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"pointAt", loc("Get point at parameter t (0-1)", "パラメータt（0-1）の点を取得", "获取参数t（0-1）处的点"), 
             "QPointF", {"float"}, {"t"}},
            {"tangentAt", loc("Get tangent at parameter t", "パラメータtの接線を取得", "获取参数t处的切线"), 
             "QPointF", {"float"}, {"t"}},
            {"lengthAt", loc("Get arc length at parameter t", "パラメータtの弧長を取得", "获取参数t处的弧长"), 
             "float", {"float"}, {"t"}},
            {"closestPoint", loc("Find closest point on curve", "曲線上の最も近い点を見つける", "找到曲线上最近的点"), 
             "QPointF", {"QPointF"}, {"testPoint"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Keyframe", "AnimatableTransform2D"};
    }
};

// ============================================================================
// Histogram Description
// ============================================================================

class HistogramDescription : public IDescribable {
public:
    QString className() const override { return "Histogram"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Calculates and displays luminance or RGB histogram for image analysis.",
            "画像解析のための輝度またはRGBヒストグラムを計算・表示します。",
            "计算并显示用于图像分析的亮度或RGB直方图。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"binCount", loc("Number of histogram bins", "ヒストグラムビン数", "直方图箱数"), "int", "256"},
            {"channel", loc("Luminance, RGB, or individual channels", "輝度、RGB、または個別チャンネル", "亮度、RGB或单独通道"), "HistogramChannel", "Luminance"},
            {"maxValue", loc("Peak histogram value", "ピークヒストグラム値", "峰值直方图值"), "int"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"calculate", loc("Calculate histogram from image", "画像からヒストグラムを計算", "从图像计算直方图"), 
             "void", {"QImage"}, {"image"}},
            {"getBinValue", loc("Get value at specific bin", "特定ビンの値を取得", "获取特定箱的值"), 
             "int", {"int"}, {"bin"}},
            {"normalize", loc("Normalize histogram values", "ヒストグラム値を正規化", "归一化直方图值"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ColorGrading", "ArtifactFrameCache"};
    }
};

// ============================================================================
// Vector2D/3D Math Description
// ============================================================================

class VectorMathDescription : public IDescribable {
public:
    QString className() const override { return "VectorMath"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Utility class for vector and matrix mathematical operations.",
            "ベクトルと行列の数学演算のためのユーティリティクラス。",
            "用于向量和矩阵数学运算的工具类。"
        );
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"lerp", loc("Linear interpolation between values", "値間の線形補間", "值之间的线性插值"), 
             "float", {"float", "float", "float"}, {"a", "b", "t"}},
            {"clamp", loc("Clamp value to range", "値を範囲にクランプ", "将值限制在范围内"), 
             "float", {"float", "float", "float"}, {"value", "min", "max"}},
            {"smoothstep", loc("Smooth interpolation", "スムーズ補間", "平滑插值"), 
             "float", {"float", "float", "float"}, {"edge0", "edge1", "x"}},
            {"distance", loc("Distance between 2D points", "2D点間の距離", "2D点之间的距离"), 
             "float", {"QPointF", "QPointF"}, {"p1", "p2"}},
            {"distance3D", loc("Distance between 3D points", "3D点間の距離", "3D点之间的距离"), 
             "float", {"QVector3D", "QVector3D"}, {"p1", "p2"}},
            {"normalize", loc("Normalize vector to unit length", "ベクトルを単位長に正規化", "将向量归一化为单位长度"), 
             "QVector3D", {"QVector3D"}, {"v"}},
            {"cross", loc("Cross product of two vectors", "2つのベクトルの外積", "两个向量的叉积"), 
             "QVector3D", {"QVector3D", "QVector3D"}, {"a", "b"}},
            {"dot", loc("Dot product of two vectors", "2つのベクトルの内積", "两个向量的点积"), 
             "float", {"QVector3D", "QVector3D"}, {"a", "b"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"AnimatableTransform2D", "AnimatableTransform3D"};
    }
};

// ============================================================================
// CurveEditor Description
// ============================================================================

class CurveEditorDescription : public IDescribable {
public:
    QString className() const override { return "CurveEditor"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Visual editor for bezier animation curves and easing functions.",
            "ベジェアニメーションカーブとイージング関数のビジュアルエディター。",
            "贝塞尔动画曲线和缓动函数的可视化编辑器。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"selectedCurve", loc("Currently selected curve", "現在選択されているカーブ", "当前选中的曲线"), "QString"},
            {"gridVisible", loc("Show background grid", "背景グリッドを表示", "显示背景网格"), "bool", "true"},
            {"snapToGrid", loc("Snap points to grid", "ポイントをグリッドにスナップ", "将点对齐到网格"), "bool", "false"},
            {"autoZoom", loc("Auto-fit curve to view", "カーブをビューに自動フィット", "自动将曲线适应到视图"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addKeyframe", loc("Add keyframe to curve", "カーブにキーフレームを追加", "向曲线添加关键帧"), 
             "void", {"float", "float"}, {"time", "value"}},
            {"removeKeyframe", loc("Remove keyframe", "キーフレームを削除", "移除关键帧"), 
             "void", {"int"}, {"index"}},
            {"setTangent", loc("Set bezier tangent handles", "ベジェ接線ハンドルを設定", "设置贝塞尔切线手柄"), 
             "void", {"int", "QPointF", "QPointF"}, {"index", "inTangent", "outTangent"}},
            {"previewCurve", loc("Preview curve shape", "カーブ形状をプレビュー", "预览曲线形状"), 
             "QImage", {"int", "int"}, {"width", "height"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Keyframe", "BezierCurve"};
    }
};

// ============================================================================
// Expression Description
// ============================================================================

class ExpressionDescription : public IDescribable {
public:
    QString className() const override { return "Expression"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "JavaScript-based expression for dynamic property values.",
            "動的プロパティ値のためのJavaScriptベースのエクスプレッション。",
            "用于动态属性值的基于JavaScript的表达式。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "Expression allows using JavaScript to create dynamic, procedural animations. "
            "Properties can be linked together, respond to other values, or use math functions "
            "for complex motion without manual keyframes.",
            "Expressionは、JavaScriptを使用して動的でプロシージャルなアニメーションを作成できます。"
            "プロパティを相互にリンクしたり、他の値に応答したり、手動キーフレームなしで"
            "複雑なモーションに数学関数を使用したりできます。",
            "Expression允许使用JavaScript创建动态的程序化动画。"
            "属性可以相互链接，响应其他值，或使用数学函数实现复杂运动而无需手动关键帧。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"code", loc("JavaScript expression code", "JavaScriptエクスプレッションコード", "JavaScript表达式代码"), "QString"},
            {"enabled", loc("Whether expression is active", "エクスプレッションが有効か", "表达式是否激活"), "bool", "true"},
            {"error", loc("Last error message", "最後のエラーメッセージ", "最后错误消息"), "QString"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Wiggle effect\n"
            "wiggle(5, 10);  // 5 times/sec, 10 pixel amplitude\n"
            "\n"
            "// Link to another property\n"
            "thisComp.layer('Target').transform.position;\n"
            "\n"
            "// Loop animation\n"
            "loopOut('cycle');",
            
            "// ウィグルエフェクト\n"
            "wiggle(5, 10);  // 毎秒5回、10ピクセル振幅\n"
            "\n"
            "// 別のプロパティにリンク\n"
            "thisComp.layer('Target').transform.position;\n"
            "\n"
            "// ループアニメーション\n"
            "loopOut('cycle');",
            
            "// 抖动效果\n"
            "wiggle(5, 10);  // 每秒5次，10像素振幅\n"
            "\n"
            "// 链接到其他属性\n"
            "thisComp.layer('Target').transform.position;\n"
            "\n"
            "// 循环动画\n"
            "loopOut('cycle');"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"Keyframe", "AnimatableTransform2D"};
    }
};

// ============================================================================
// RenderQueue Description
// ============================================================================

class RenderQueueDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactRenderQueue"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Manages background rendering queue for multiple compositions.",
            "複数のコンポジションのバックグラウンドレンダリングキューを管理します。",
            "管理多个合成的后台渲染队列。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"jobCount", loc("Number of jobs in queue", "キュー内のジョブ数", "队列中的作业数"), "int"},
            {"currentJob", loc("Currently rendering job", "現在レンダリング中のジョブ", "当前正在渲染的作业"), "QString"},
            {"isRendering", loc("Whether queue is active", "キューがアクティブか", "队列是否活动"), "bool"},
            {"pauseOnError", loc("Pause on render error", "レンダリングエラーで一時停止", "渲染错误时暂停"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addJob", loc("Add composition to queue", "コンポジションをキューに追加", "将合成添加到队列"), 
             "void", {"ArtifactComposition*", "QString"}, {"composition", "outputPath"}},
            {"removeJob", loc("Remove job from queue", "ジョブをキューから削除", "从队列移除作业"), 
             "void", {"int"}, {"index"}},
            {"startQueue", loc("Start rendering queue", "レンダリングキューを開始", "开始渲染队列"), "void"},
            {"pauseQueue", loc("Pause rendering queue", "レンダリングキューを一時停止", "暂停渲染队列"), "void"},
            {"clearQueue", loc("Clear all jobs", "全ジョブをクリア", "清除所有作业"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactRenderController", "ArtifactComposition"};
    }
};

// ============================================================================
// DistributionModes Description
// ============================================================================

class DistributionModesDescription : public IDescribable {
public:
    QString className() const override { return "DistributionModes"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Defines how generated elements are distributed in space.",
            "生成された要素が空間にどのように分布されるかを定義します。",
            "定义生成的元素在空间中的分布方式。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"mode", loc("Distribution mode type", "分布モードタイプ", "分布模式类型"), "DistributionMode", "Uniform"},
            {"jitter", loc("Random position variation", "ランダム位置変動", "随机位置变化"), "float", "0.0", "0.0", "1.0"},
            {"gridSpacing", loc("Grid cell size for grid mode", "グリッドモードのセルサイズ", "网格模式的单元格大小"), "float", "50.0"},
            {"radialCenter", loc("Center point for radial mode", "放射モードの中心点", "放射模式的中心点"), "QPointF", "(0, 0)"},
            {"radialRadius", loc("Radius for radial distribution", "放射分布の半径", "放射分布的半径"), "float", "100.0"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Uniform random distribution\n"
            "DistributionModes mode;\n"
            "mode.setMode(DistributionMode::Uniform);\n"
            "mode.setJitter(0.5f);\n"
            "\n"
            "// Grid distribution\n"
            "mode.setMode(DistributionMode::Grid);\n"
            "mode.setGridSpacing(100.0f);",
            
            "// 均一ランダム分布\n"
            "DistributionModes mode;\n"
            "mode.setMode(DistributionMode::Uniform);\n"
            "mode.setJitter(0.5f);\n"
            "\n"
            "// グリッド分布\n"
            "mode.setMode(DistributionMode::Grid);\n"
            "mode.setGridSpacing(100.0f);",
            
            "// 均匀随机分布\n"
            "DistributionModes mode;\n"
            "mode.setMode(DistributionMode::Uniform);\n"
            "mode.setJitter(0.5f);\n"
            "\n"
            "// 网格分布\n"
            "mode.setMode(DistributionMode::Grid);\n"
            "mode.setGridSpacing(100.0f);"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"CloneGenerator", "ParticleEmitter"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<ImageF32x4RGBADescription> _reg_ImageF32x4RGBA("ImageF32x4RGBA");
static AutoRegisterDescribable<BezierCurveDescription> _reg_BezierCurve("BezierCurve");
static AutoRegisterDescribable<HistogramDescription> _reg_Histogram("Histogram");
static AutoRegisterDescribable<VectorMathDescription> _reg_VectorMath("VectorMath");
static AutoRegisterDescribable<CurveEditorDescription> _reg_CurveEditor("CurveEditor");
static AutoRegisterDescribable<ExpressionDescription> _reg_Expression("Expression");
static AutoRegisterDescribable<RenderQueueDescription> _reg_RenderQueue("ArtifactRenderQueue");
static AutoRegisterDescribable<DistributionModesDescription> _reg_DistributionModes("DistributionModes");

} // namespace ArtifactCore