module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.RenderAudioColorDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ArtifactRenderController Description
// ============================================================================

class RenderControllerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactRenderController"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Controls the rendering pipeline for composition output.",
            "コンポジション出力のレンダリングパイプラインを制御します。",
            "控制合成输出的渲染管线。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactRenderController manages the rendering process including frame queue, "
            "render settings, output format, and progress tracking. It supports background "
            "rendering and can output to various video and image formats.",
            "ArtifactRenderControllerは、フレームキュー、レンダリング設定、出力フォーマット、"
            "進捗追跡を含むレンダリングプロセスを管理します。バックグラウンドレンダリングをサポートし、"
            "様々な動画および画像フォーマットに出力できます。",
            "ArtifactRenderController管理渲染过程，包括帧队列、渲染设置、输出格式和进度跟踪。"
            "它支持后台渲染，可以输出到各种视频和图像格式。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"outputPath", loc("Output file path", "出力ファイルパス", "输出文件路径"), "QString"},
            {"format", loc("Output format (MP4, PNG, etc)", "出力フォーマット（MP4、PNG等）", "输出格式（MP4、PNG等）"), "QString"},
            {"quality", loc("Render quality setting", "レンダリング品質設定", "渲染质量设置"), "int", "80", "1", "100"},
            {"frameRange", loc("Start and end frames", "開始・終了フレーム", "开始和结束帧"), "QPair<int64_t, int64_t>"},
            {"isRendering", loc("Currently rendering", "レンダリング中か", "是否正在渲染"), "bool"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"startRender", loc("Begin rendering process", "レンダリングプロセスを開始", "开始渲染过程"), "void"},
            {"pauseRender", loc("Pause rendering", "レンダリングを一時停止", "暂停渲染"), "void"},
            {"stopRender", loc("Stop and cancel rendering", "レンダリングを停止・キャンセル", "停止并取消渲染"), "void"},
            {"getProgress", loc("Get rendering progress 0-100", "レンダリング進捗（0-100）を取得", "获取渲染进度0-100"), "float"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactFrameCache", "ArtifactRenderQueue"};
    }
};

// ============================================================================
// ArtifactFrameCache Description
// ============================================================================

class FrameCacheDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactFrameCache"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Caches rendered frames for fast preview and playback.",
            "高速プレビューと再生のためにレンダリング済みフレームをキャッシュします。",
            "缓存渲染帧以实现快速预览和播放。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"maxSizeMB", loc("Maximum cache size in MB", "最大キャッシュサイズ（MB）", "最大缓存大小（MB）"), "int", "1024"},
            {"currentSizeMB", loc("Current cache usage in MB", "現在のキャッシュ使用量（MB）", "当前缓存使用量（MB）"), "int"},
            {"hitRate", loc("Cache hit rate percentage", "キャッシュヒット率（%）", "缓存命中率（%）"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"getFrame", loc("Get cached frame if available", "利用可能ならキャッシュフレームを取得", "如果可用则获取缓存帧"), 
             "QImage", {"int64_t"}, {"frameNumber"}},
            {"cacheFrame", loc("Store frame in cache", "フレームをキャッシュに保存", "将帧存储到缓存"), 
             "void", {"int64_t", "QImage"}, {"frameNumber", "image"}},
            {"clear", loc("Clear all cached frames", "全キャッシュフレームをクリア", "清除所有缓存帧"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactRenderController", "ArtifactComposition"};
    }
};

// ============================================================================
// ArtifactAudioMixer Description
// ============================================================================

class AudioMixerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAudioMixer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Mixes multiple audio tracks with volume, pan, and effects control.",
            "ボリューム、パン、エフェクト制御で複数のオーディオトラックをミックスします。",
            "使用音量、声像和效果控制混合多个音轨。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactAudioMixer handles audio mixing for the composition. It manages multiple "
            "audio sources, applies volume and pan settings, processes real-time effects, "
            "and outputs the final mixed audio stream.",
            "ArtifactAudioMixerはコンポジションのオーディオミキシングを処理します。複数の"
            "オーディオソースを管理し、ボリュームとパン設定を適用し、リアルタイムエフェクトを処理し、"
            "最終的なミックス済みオーディオストリームを出力します。",
            "ArtifactAudioMixer处理合成的音频混合。它管理多个音频源，应用音量和声像设置，"
            "处理实时效果，并输出最终混合的音频流。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"masterVolume", loc("Overall output volume", "全体出力音量", "总体输出音量"), "float", "1.0", "0.0", "2.0"},
            {"trackCount", loc("Number of active tracks", "アクティブなトラック数", "活动轨道数"), "int"},
            {"sampleRate", loc("Audio sample rate", "オーディオサンプリングレート", "音频采样率"), "int", "48000"},
            {"channels", loc("Number of output channels", "出力チャンネル数", "输出通道数"), "int", "2"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addTrack", loc("Add audio track to mixer", "オーディオトラックをミキサーに追加", "向混音器添加音轨"), 
             "int"},
            {"removeTrack", loc("Remove audio track", "オーディオトラックを削除", "移除音轨"), 
             "void", {"int"}, {"trackIndex"}},
            {"setTrackVolume", loc("Set track volume", "トラック音量を設定", "设置轨道音量"), 
             "void", {"int", "float"}, {"trackIndex", "volume"}},
            {"setTrackPan", loc("Set track stereo pan", "トラックのステレオパンを設定", "设置轨道立体声声像"), 
             "void", {"int", "float"}, {"trackIndex", "pan"}},
            {"process", loc("Process and output audio buffer", "オーディオバッファを処理・出力", "处理并输出音频缓冲"), 
             "QByteArray"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAudioLayer", "ArtifactAudioWaveform"};
    }
};

// ============================================================================
// ColorGrading Description
// ============================================================================

class ColorGradingDescription : public IDescribable {
public:
    QString className() const override { return "ColorGrading"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies color correction and grading adjustments to images or video.",
            "画像や動画にカラーコレクションとグレーディング調整を適用します。",
            "对图像或视频应用颜色校正和分级调整。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ColorGrading provides professional color adjustment tools including lift/gamma/gain, "
            "curves, hue/saturation/luminance, and color wheels. It supports LUT import and "
            "can be keyframed for animated color changes.",
            "ColorGradingは、リフト/ガンマ/ゲイン、カーブ、色相/彩度/輝度、カラーホイールを含む"
            "プロフェッショナルなカラー調整ツールを提供します。LUTインポートをサポートし、"
            "アニメーション化されたカラー変更のためにキーフレーム可能です。",
            "ColorGrading提供专业的颜色调整工具，包括提升/伽马/增益、曲线、色相/饱和度/亮度和色轮。"
            "它支持LUT导入，并可以关键帧化以实现动画颜色变化。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"lift", loc("Shadows adjustment", "シャドウ調整", "阴影调整"), "QColor"},
            {"gamma", loc("Midtones adjustment", "中間調調整", "中间调调整"), "QColor"},
            {"gain", loc("Highlights adjustment", "ハイライト調整", "高光调整"), "QColor"},
            {"contrast", loc("Overall contrast", "全体コントラスト", "整体对比度"), "float", "1.0", "0.0", "2.0"},
            {"saturation", loc("Color saturation", "色の彩度", "颜色饱和度"), "float", "1.0", "0.0", "2.0"},
            {"temperature", loc("Color temperature (Kelvin)", "色温度（ケルビン）", "色温（开尔文）"), "float", "6500.0"},
            {"tint", loc("Green-magenta tint", "緑-マゼンタのティント", "绿-品红色调"), "float", "0.0", "-1.0", "1.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"apply", loc("Apply grading to image", "画像にグレーディングを適用", "将分级应用于图像"), 
             "QImage", {"QImage"}, {"source"}},
            {"loadLUT", loc("Load LUT file", "LUTファイルを読み込み", "加载LUT文件"), 
             "bool", {"QString"}, {"filePath"}},
            {"reset", loc("Reset all adjustments", "全調整をリセット", "重置所有调整"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactColorManagement", "ColorSpace", "ArtifactColorWheels"};
    }
};

// ============================================================================
// ArtifactColorWheels Description
// ============================================================================

class ColorWheelsDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactColorWheels"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Provides three-way color wheel controls for shadows, midtones, and highlights.",
            "シャドウ、中間調、ハイライト用の3ウェイカラーホイールコントロールを提供します。",
            "为阴影、中间调和高光提供三向色轮控制。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"shadowsHue", loc("Shadows hue offset", "シャドウの色相オフセット", "阴影色相偏移"), "float"},
            {"shadowsSaturation", loc("Shadows saturation", "シャドウの彩度", "阴影饱和度"), "float"},
            {"midtonesHue", loc("Midtones hue offset", "中間調の色相オフセット", "中间调色相偏移"), "float"},
            {"highlightsHue", loc("Highlights hue offset", "ハイライトの色相オフセット", "高光色相偏移"), "float"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ColorGrading", "ArtifactColorManagement"};
    }
};

// ============================================================================
// MotionBlurEffect Description
// ============================================================================

class MotionBlurEffectDescription : public IDescribable {
public:
    QString className() const override { return "MotionBlurEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies motion blur based on object or camera movement.",
            "オブジェクトやカメラの動きに基づいてモーションブラーを適用します。",
            "根据物体或摄像机运动应用运动模糊。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"shutterAngle", loc("Shutter angle (degrees)", "シャッター角度（度）", "快门角度（度）"), "float", "180.0", "0.0", "360.0"},
            {"shutterOffset", loc("Shutter timing offset", "シャッタータイミングオフセット", "快门时间偏移"), "float", "0.0"},
            {"samples", loc("Number of blur samples", "ブラーサンプル数", "模糊采样数"), "int", "16", "1", "64"},
            {"motionType", loc("Velocity, Directional, or Radial", "Velocity、Directional、またはRadial", "Velocity、Directional或Radial"), "MotionBlurType"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "MotionEstimator"};
    }
};

// ============================================================================
// StabilizerEffect Description
// ============================================================================

class StabilizerEffectDescription : public IDescribable {
public:
    QString className() const override { return "StabilizerEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Reduces camera shake and stabilizes video footage.",
            "カメラブレを軽減し、映像素材を安定化します。",
            "减少相机抖动并稳定视频素材。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "StabilizerEffect analyzes video frames to detect camera motion, then applies "
            "transformations to smooth out unwanted shake. It supports various smoothing modes "
            "and can track features across frames for accurate stabilization.",
            "StabilizerEffectは動画フレームを解析してカメラの動きを検出し、不要なブレを"
            "軽減する変換を適用します。様々なスムージングモードをサポートし、正確な安定化のために"
            "フレーム間で特徴点を追跡できます。",
            "StabilizerEffect分析视频帧以检测摄像机运动，然后应用变换来平滑不必要的抖动。"
            "它支持各种平滑模式，可以跨帧跟踪特征以实现精确稳定。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"smoothingWindow", loc("Temporal smoothing window size", "時間スムージングウィンドウサイズ", "时间平滑窗口大小"), "int", "30"},
            {"stabilizeTranslation", loc("Stabilize position movement", "位置移動を安定化", "稳定位置移动"), "bool", "true"},
            {"stabilizeRotation", loc("Stabilize rotation", "回転を安定化", "稳定旋转"), "bool", "true"},
            {"stabilizeScale", loc("Stabilize zoom changes", "ズーム変更を安定化", "稳定缩放变化"), "bool", "false"},
            {"borderFill", loc("How to fill border areas", "ボーダーエリアの埋め方", "如何填充边界区域"), "float", "0.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "MotionEstimator"};
    }
};

// ============================================================================
// FilmEffects Description
// ============================================================================

class FilmEffectsDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactFilmEffects"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Applies film-like effects such as grain, vignette, and color grading.",
            "グレイン、ビネット、カラーグレーディングなどのフィルムライクなエフェクトを適用します。",
            "应用颗粒、暗角和颜色分级等电影效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"grainAmount", loc("Film grain intensity", "フィルムグレインの強さ", "胶片颗粒强度"), "float", "0.1"},
            {"grainSize", loc("Grain particle size", "グレイン粒子サイズ", "颗粒粒子大小"), "float", "1.0"},
            {"vignetteAmount", loc("Vignette intensity", "ビネットの強さ", "暗角强度"), "float", "0.5"},
            {"vignetteRadius", loc("Vignette falloff radius", "ビネットのフォールオフ半径", "暗角衰减半径"), "float", "1.0"},
            {"chromaticAberration", loc("Color fringing amount", "色収差量", "色差量"), "float", "0.0"},
            {"filmStock", loc("Preset film stock type", "プリセットフィルムストックタイプ", "预设胶片类型"), "QString"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "ColorGrading"};
    }
};

// ============================================================================
// ChromaKeyEffect Description
// ============================================================================

class ChromaKeyEffectDescription : public IDescribable {
public:
    QString className() const override { return "ChromaKeyEffect"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Removes a specific color (typically green/blue screen) to create transparency.",
            "特定の色（通常はグリーン/ブルースクリーン）を削除して透明にします。",
            "移除特定颜色（通常是绿/蓝屏）以创建透明度。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"keyColor", loc("Color to remove", "削除する色", "要移除的颜色"), "QColor", "green"},
            {"tolerance", loc("Color matching tolerance", "色マッチング許容度", "颜色匹配容差"), "float", "30.0"},
            {"edgeSoftness", loc("Edge feathering amount", "エッジのぼかし量", "边缘羽化量"), "float", "5.0"},
            {"spillSuppression", loc("Remove color spill on edges", "エッジのカラースピルを除去", "移除边缘颜色溢出"), "float", "0.5"},
            {"despillAmount", loc("Amount of despill correction", "デスピル補正量", "去溢出校正量"), "float", "0.5"}
        };
    }
    
    LocalizedText usageExamples() const override {
        return loc(
            "// Basic green screen keying\n"
            "ChromaKeyEffect key;\n"
            "key.setKeyColor(QColor(0, 255, 0));  // Green\n"
            "key.setTolerance(40.0f);\n"
            "key.process(sourceImage, outputImage);",
            
            "// 基本的なグリーンスクリーンキーイング\n"
            "ChromaKeyEffect key;\n"
            "key.setKeyColor(QColor(0, 255, 0));  // 緑\n"
            "key.setTolerance(40.0f);\n"
            "key.process(sourceImage, outputImage);",
            
            "// 基本绿幕抠像\n"
            "ChromaKeyEffect key;\n"
            "key.setKeyColor(QColor(0, 255, 0));  // 绿色\n"
            "key.setTolerance(40.0f);\n"
            "key.process(sourceImage, outputImage);"
        );
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractEffect", "ArtifactAbstractLayer"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<RenderControllerDescription> _reg_RenderController("ArtifactRenderController");
static AutoRegisterDescribable<FrameCacheDescription> _reg_FrameCache("ArtifactFrameCache");
static AutoRegisterDescribable<AudioMixerDescription> _reg_AudioMixer("ArtifactAudioMixer");
static AutoRegisterDescribable<ColorGradingDescription> _reg_ColorGrading("ColorGrading");
static AutoRegisterDescribable<ColorWheelsDescription> _reg_ColorWheels("ArtifactColorWheels");
static AutoRegisterDescribable<MotionBlurEffectDescription> _reg_MotionBlurEffect("MotionBlurEffect");
static AutoRegisterDescribable<StabilizerEffectDescription> _reg_StabilizerEffect("StabilizerEffect");
static AutoRegisterDescribable<FilmEffectsDescription> _reg_FilmEffects("ArtifactFilmEffects");
static AutoRegisterDescribable<ChromaKeyEffectDescription> _reg_ChromaKeyEffect("ChromaKeyEffect");

} // namespace ArtifactCore