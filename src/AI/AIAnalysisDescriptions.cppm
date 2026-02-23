module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.AIAnalysisDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ObjectDetector Description
// ============================================================================

class ObjectDetectorDescription : public IDescribable {
public:
    QString className() const override { return "ObjectDetector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "AI-powered object detection for automatic subject identification in frames.",
            "フレーム内の被写体自動識別のためのAI搭載オブジェクト検出。",
            "用于帧中主体自动识别的AI驱动对象检测。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ObjectDetector uses machine learning models to identify and locate objects "
            "in images or video frames. It can detect people, vehicles, animals, and other "
            "common objects with bounding boxes for tracking and masking.",
            "ObjectDetectorは機械学習モデルを使用して画像や動画フレーム内のオブジェクトを"
            "識別・位置特定します。追跡とマスキングのためのバウンディングボックスで、"
            "人、車両、動物、その他の一般的なオブジェクトを検出できます。",
            "ObjectDetector使用机器学习模型识别和定位图像或视频帧中的对象。"
            "它可以使用边界框检测人、车辆、动物和其他常见对象，用于跟踪和遮罩。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"model", loc("Detection model to use", "使用する検出モデル", "要使用的检测模型"), "QString", "yolov8"},
            {"confidence", loc("Minimum confidence threshold", "最小信頼度閾値", "最小置信度阈值"), "float", "0.5", "0.0", "1.0"},
            {"classes", loc("Object classes to detect", "検出するオブジェクトクラス", "要检测的对象类别"), "QStringList"},
            {"gpuEnabled", loc("Use GPU acceleration", "GPU加速を使用", "使用GPU加速"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"detect", loc("Detect objects in image", "画像内のオブジェクトを検出", "检测图像中的对象"), 
             "QList<Detection>", {"QImage"}, {"image"}},
            {"detectVideo", loc("Detect objects in video frames", "動画フレーム内のオブジェクトを検出", "检测视频帧中的对象"), 
             "void", {"QString"}, {"videoPath"}},
            {"setClasses", loc("Set classes to detect", "検出クラスを設定", "设置要检测的类别"), 
             "void", {"QStringList"}, {"classes"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"MotionTracker", "RotoscopeMask", "FaceDetector"};
    }
};

// ============================================================================
// FaceDetector Description
// ============================================================================

class FaceDetectorDescription : public IDescribable {
public:
    QString className() const override { return "FaceDetector"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Detects faces with landmarks for tracking and effects.",
            "追跡とエフェクトのためのランドマーク付きで顔を検出します。",
            "检测带有特征点的人脸用于跟踪和效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"landmarks", loc("Number of facial landmarks", "顔のランドマーク数", "人脸特征点数"), "int", "68"},
            {"minFaceSize", loc("Minimum face size (pixels)", "最小顔サイズ（ピクセル）", "最小人脸大小（像素）"), "int", "40"},
            {"detectEyes", loc("Detect eye positions", "目の位置を検出", "检测眼睛位置"), "bool", "true"},
            {"detectExpression", loc("Detect facial expression", "表情を検出", "检测面部表情"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"detect", loc("Detect faces in image", "画像内の顔を検出", "检测图像中的人脸"), 
             "QList<FaceDetection>", {"QImage"}, {"image"}},
            {"getLandmarks", loc("Get facial landmark points", "顔のランドマークポイントを取得", "获取人脸特征点"), 
             "QVector<QPointF>", {"FaceDetection"}, {"face"}},
            {"getHeadPose", loc("Estimate head orientation", "頭の向きを推定", "估计头部姿态"), 
             "QVector3D", {"FaceDetection"}, {"face"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ObjectDetector", "MotionTracker"};
    }
};

// ============================================================================
// MotionTracker Description
// ============================================================================

class MotionTrackerDescription : public IDescribable {
public:
    QString className() const override { return "MotionTracker"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Tracks object motion across video frames for follow effects and stabilization.",
            "追従エフェクトと安定化のために動画フレーム間でオブジェクトの動きを追跡します。",
            "跟踪视频帧间的对象运动，用于跟随效果和稳定。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"trackMode", loc("Point, Region, or Feature tracking", "ポイント、領域、または特徴追跡", "点、区域或特征跟踪"), "TrackMode"},
            {"smoothing", loc("Motion smoothing factor", "動きスムージング係数", "运动平滑系数"), "float", "0.5", "0.0", "1.0"},
            {"predictFrames", loc("Predict ahead frames", "予測先読みフレーム", "预测前瞻帧数"), "int", "5"},
            {"featureCount", loc("Number of track points", "追跡ポイント数", "跟踪点数"), "int", "100"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setRegion", loc("Set tracking region", "追跡領域を設定", "设置跟踪区域"), 
             "void", {"QRectF"}, {"region"}},
            {"trackForward", loc("Track forward in time", "時間前方に追跡", "向前跟踪"), 
             "void", {"int"}, {"frameCount"}},
            {"trackBackward", loc("Track backward in time", "時間後方に追跡", "向后跟踪"), 
             "void", {"int"}, {"frameCount"}},
            {"getPosition", loc("Get tracked position at frame", "フレームでの追跡位置を取得", "获取帧处的跟踪位置"), 
             "QPointF", {"int64_t"}, {"frame"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"MotionEstimator", "TrackMask", "StabilizerEffect"};
    }
};

// ============================================================================
// SceneClassifier Description
// ============================================================================

class SceneClassifierDescription : public IDescribable {
public:
    QString className() const override { return "SceneClassifier"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "AI-based scene classification for automatic tagging and organization.",
            "自動タグ付けと整理のためのAIベースのシーン分類。",
            "用于自动标记和组织的基于AI的场景分类。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"categories", loc("Scene categories to detect", "検出するシーンカテゴリ", "要检测的场景类别"), "QStringList"},
            {"threshold", loc("Minimum confidence for classification", "分類の最小信頼度", "分类的最小置信度"), "float", "0.7"},
            {"batchSize", loc("Frames per analysis batch", "解析バッチあたりのフレーム数", "每次分析的帧数"), "int", "30"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"classify", loc("Classify scene in frame", "フレーム内のシーンを分類", "分类帧中的场景"), 
             "SceneResult", {"QImage"}, {"image"}},
            {"classifyVideo", loc("Classify all scenes in video", "動画内の全シーンを分類", "分类视频中的所有场景"), 
             "QList<SceneResult>", {"QString"}, {"videoPath"}},
            {"detectSceneChange", loc("Find scene change points", "シーン変更点を見つける", "查找场景切换点"), 
             "QList<int64_t>", {"QString"}, {"videoPath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ObjectDetector", "ArtifactComposition"};
    }
};

// ============================================================================
// StyleTransfer Description
// ============================================================================

class StyleTransferDescription : public IDescribable {
public:
    QString className() const override { return "StyleTransfer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies artistic style from one image to another using neural networks.",
            "ニューラルネットワークを使用して画像間で芸術的なスタイルを適用します。",
            "使用神经网络将艺术风格从一幅图像应用到另一幅图像。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "StyleTransfer uses deep learning to apply the artistic style of one image "
            "to the content of another. This can create painting-like effects, "
            "style matching, and artistic transformations.",
            "StyleTransferはディープラーニングを使用して、ある画像の芸術的なスタイルを"
            "別の画像のコンテンツに適用します。これにより、絵画のようなエフェクト、"
            "スタイルマッチング、芸術的な変換を作成できます。",
            "StyleTransfer使用深度学习将一幅图像的艺术风格应用到另一幅图像的内容。"
            "这可以创建绘画般的效果、风格匹配和艺术转换。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"styleStrength", loc("Style application intensity", "スタイル適用強度", "风格应用强度"), "float", "1.0", "0.0", "1.0"},
            {"preserveColor", loc("Keep original colors", "元の色を保持", "保持原始颜色"), "bool", "false"},
            {"styleModel", loc("Pre-trained style model", "事前学習済みスタイルモデル", "预训练风格模型"), "QString"},
            {"resolution", loc("Output resolution scale", "出力解像度スケール", "输出分辨率缩放"), "float", "1.0", "0.25", "2.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setStyleImage", loc("Set style reference image", "スタイル参照画像を設定", "设置风格参考图像"), 
             "void", {"QImage"}, {"styleImage"}},
            {"apply", loc("Apply style to content image", "コンテンツ画像にスタイルを適用", "将风格应用于内容图像"), 
             "QImage", {"QImage"}, {"contentImage"}},
            {"loadPreset", loc("Load style preset", "スタイルプリセットを読み込み", "加载风格预设"), 
             "bool", {"QString"}, {"presetName"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "ColorGrading"};
    }
};

// ============================================================================
// BackgroundRemover Description
// ============================================================================

class BackgroundRemoverDescription : public IDescribable {
public:
    QString className() const override { return "BackgroundRemover"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "AI-powered automatic background removal for subjects.",
            "被写体のためのAI搭載自動背景除去。",
            "用于主体的AI驱动自动背景移除。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"model", loc("Segmentation model", "セグメンテーションモデル", "分割模型"), "QString", "u2net"},
            {"edgeRefinement", loc("Edge smoothing strength", "エッジスムージング強度", "边缘平滑强度"), "float", "0.5", "0.0", "1.0"},
            {"foregroundThreshold", loc("Foreground sensitivity", "前景感度", "前景灵敏度"), "float", "0.5"},
            {"temporalConsistency", loc("Frame-to-frame stability", "フレーム間安定性", "帧间稳定性"), "float", "0.8"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"removeBackground", loc("Remove background from image", "画像から背景を除去", "从图像中移除背景"), 
             "QImage", {"QImage"}, {"image"}},
            {"getMask", loc("Get foreground mask", "前景マスクを取得", "获取前景蒙版"), 
             "QImage", {"QImage"}, {"image"}},
            {"processVideo", loc("Process entire video", "動画全体を処理", "处理整个视频"), 
             "void", {"QString", "QString"}, {"inputPath", "outputPath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"RotoscopeMask", "ChromaKeyEffect", "ObjectDetector"};
    }
};

// ============================================================================
// SuperResolution Description
// ============================================================================

class SuperResolutionDescription : public IDescribable {
public:
    QString className() const override { return "SuperResolution"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Upscales images using AI to add detail and sharpness.",
            "AIを使用してディテールと鮮明さを追加して画像を拡大します。",
            "使用AI放大图像以增加细节和清晰度。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"scale", loc("Upscaling factor (2x, 4x)", "拡大倍率（2x、4x）", "放大倍数（2x、4x）"), "int", "2"},
            {"model", loc("SR model (ESRGAN, RealSR, etc)", "SRモデル（ESRGAN、RealSR等）", "SR模型（ESRGAN、RealSR等）"), "QString", "ESRGAN"},
            {"denoiseStrength", loc("Noise reduction level", "ノイズ低減レベル", "降噪级别"), "float", "0.5"},
            {"sharpen", loc("Output sharpness", "出力シャープネス", "输出锐度"), "float", "0.3"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"upscale", loc("Upscale image", "画像を拡大", "放大图像"), 
             "QImage", {"QImage"}, {"image"}},
            {"upscaleVideo", loc("Upscale video frames", "動画フレームを拡大", "放大视频帧"), 
             "void", {"QString", "QString"}, {"inputPath", "outputPath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"SharpenEffect", "ArtifactImageLayer"};
    }
};

// ============================================================================
// ColorMatch Description
// ============================================================================

class ColorMatchDescription : public IDescribable {
public:
    QString className() const override { return "ColorMatch"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Matches color grading between images or frames automatically.",
            "画像間またはフレーム間のカラーグレーディングを自動的にマッチングします。",
            "自动匹配图像或帧之间的颜色分级。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"referenceImage", loc("Target color reference", "ターゲット色参照", "目标颜色参考"), "QImage"},
            {"method", loc("Matching algorithm", "マッチングアルゴリズム", "匹配算法"), "MatchMethod", "Histogram"},
            {"strength", loc("Match application strength", "マッチ適用強度", "匹配应用强度"), "float", "1.0"},
            {"preserveSkinTones", loc("Protect skin colors", "肌色を保護", "保护肤色"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"match", loc("Apply color match to image", "画像にカラーマッチを適用", "将颜色匹配应用于图像"), 
             "QImage", {"QImage"}, {"sourceImage"}},
            {"setReference", loc("Set reference image", "参照画像を設定", "设置参考图像"), 
             "void", {"QImage"}, {"referenceImage"}},
            {"createLUT", loc("Generate color matching LUT", "カラーマッチングLUTを生成", "生成颜色匹配LUT"), 
             "QImage"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ColorGrading", "ColorSpace"};
    }
};

// ============================================================================
// AudioAnalyzer Description
// ============================================================================

class AudioAnalyzerDescription : public IDescribable {
public:
    QString className() const override { return "AudioAnalyzer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Analyzes audio for visualization, keyframe generation, and effects sync.",
            "可視化、キーフレーム生成、エフェクト同期のためのオーディオ解析。",
            "分析音频用于可视化、关键帧生成和效果同步。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"fftSize", loc("FFT window size", "FFTウィンドウサイズ", "FFT窗口大小"), "int", "2048"},
            {"windowFunction", loc("Window function type", "ウィンドウ関数タイプ", "窗口函数类型"), "WindowFunction", "Hann"},
            {"sampleRate", loc("Analysis sample rate", "解析サンプリングレート", "分析采样率"), "int", "44100"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"analyze", loc("Analyze audio file", "オーディオファイルを解析", "分析音频文件"), 
             "AudioData", {"QString"}, {"audioPath"}},
            {"getAmplitude", loc("Get amplitude at time", "時間の振幅を取得", "获取时间处的振幅"), 
             "float", {"float"}, {"time"}},
            {"getFrequencySpectrum", loc("Get frequency data at time", "時間の周波数データを取得", "获取时间处的频率数据"), 
             "QVector<float>", {"float"}, {"time"}},
            {"getBeatTimes", loc("Detect beat positions", "ビート位置を検出", "检测节拍位置"), 
             "QVector<float>"},
            {"generateKeyframes", loc("Create amplitude-based keyframes", "振幅ベースのキーフレームを作成", "创建基于振幅的关键帧"), 
             "QList<Keyframe>", {"float", "float"}, {"threshold", "interval"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAudioWaveform", "ArtifactAudioMixer", "ArtifactAudioLayer"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<ObjectDetectorDescription> _reg_ObjectDetector("ObjectDetector");
static AutoRegisterDescribable<FaceDetectorDescription> _reg_FaceDetector("FaceDetector");
static AutoRegisterDescribable<MotionTrackerDescription> _reg_MotionTracker("MotionTracker");
static AutoRegisterDescribable<SceneClassifierDescription> _reg_SceneClassifier("SceneClassifier");
static AutoRegisterDescribable<StyleTransferDescription> _reg_StyleTransfer("StyleTransfer");
static AutoRegisterDescribable<BackgroundRemoverDescription> _reg_BackgroundRemover("BackgroundRemover");
static AutoRegisterDescribable<SuperResolutionDescription> _reg_SuperResolution("SuperResolution");
static AutoRegisterDescribable<ColorMatchDescription> _reg_ColorMatch("ColorMatch");
static AutoRegisterDescribable<AudioAnalyzerDescription> _reg_AudioAnalyzer("AudioAnalyzer");

} // namespace ArtifactCore