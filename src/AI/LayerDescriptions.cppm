module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.LayerDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ArtifactTextLayer Description
// ============================================================================

class TextLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactTextLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer that displays animated text with customizable fonts and styling.",
            "カスタマイズ可能なフォントとスタイリングでアニメーションテキストを表示するレイヤー。",
            "显示具有可自定义字体和样式的动画文本的图层。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactTextLayer provides text rendering with support for multiple fonts, sizes, colors, "
            "and text animations. Text can be animated per-character or per-word with various effects "
            "like typewriter, fade, and scale.",
            "ArtifactTextLayerは、複数のフォント、サイズ、色、テキストアニメーションをサポートした"
            "テキストレンダリングを提供します。テキストは文字ごとまたは単語ごとにアニメーションでき、"
            "タイプライター、フェード、スケールなどの様々なエフェクトがあります。",
            "ArtifactTextLayer提供支持多种字体、大小、颜色和文本动画的文本渲染。"
            "文本可以按字符或按词动画，具有打字机、淡入淡出和缩放等各种效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"text", loc("Text content to display", "表示するテキスト内容", "要显示的文本内容"), "QString"},
            {"fontFamily", loc("Font family name", "フォントファミリー名", "字体族名称"), "QString", "Arial"},
            {"fontSize", loc("Font size in points", "フォントサイズ（ポイント）", "字体大小（点）"), "float", "72.0"},
            {"fontColor", loc("Text color", "テキスト色", "文本颜色"), "QColor", "white"},
            {"alignment", loc("Text alignment", "テキスト配置", "文本对齐"), "Qt::Alignment", "AlignLeft"},
            {"tracking", loc("Character spacing", "文字間隔", "字符间距"), "float", "0.0"},
            {"leading", loc("Line spacing", "行間隔", "行间距"), "float", "1.0"},
            {"animatePerCharacter", loc("Animate each character separately", "各文字を個別にアニメーション", "单独动画每个字符"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setText", loc("Set the text content", "テキスト内容を設定", "设置文本内容"), "void", {"QString"}, {"text"}},
            {"setFont", loc("Set font family and size", "フォントファミリーとサイズを設定", "设置字体族和大小"), "void", {"QString", "float"}, {"family", "size"}},
            {"setColor", loc("Set text color", "テキスト色を設定", "设置文本颜色"), "void", {"QColor"}, {"color"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactShapeLayer", "ArtifactImageLayer"};
    }
};

// ============================================================================
// ArtifactVideoLayer Description
// ============================================================================

class VideoLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactVideoLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer that plays video files with support for timing and frame-accurate control.",
            "タイミングとフレーム精度の制御をサポートした動画ファイルを再生するレイヤー。",
            "播放支持定时和精确帧控制的视频文件的图层。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactVideoLayer handles video playback within the composition. It supports various "
            "video formats, provides frame-accurate seeking, handles audio synchronization, and "
            "can be time-remapped for slow-motion or fast-forward effects.",
            "ArtifactVideoLayerはコンポジション内での動画再生を処理します。様々な動画フォーマットを"
            "サポートし、フレーム精度のシークを提供し、オーディオ同期を処理し、スローモーションや"
            "早送りエフェクトのためのタイムリマップが可能です。",
            "ArtifactVideoLayer处理合成中的视频播放。它支持各种视频格式，提供精确帧定位，"
            "处理音频同步，并可以重新映射时间以实现慢动作或快进效果。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"sourcePath", loc("Video file path", "動画ファイルパス", "视频文件路径"), "QString"},
            {"currentTime", loc("Current playback time", "現在の再生時間", "当前播放时间"), "float"},
            {"duration", loc("Video duration in seconds", "動画の長さ（秒）", "视频时长（秒）"), "float"},
            {"frameRate", loc("Video frame rate", "動画フレームレート", "视频帧率"), "float"},
            {"loop", loc("Whether to loop video", "動画をループするか", "是否循环视频"), "bool", "false"},
            {"muted", loc("Whether audio is muted", "オーディオをミュートするか", "是否静音"), "bool", "false"},
            {"volume", loc("Audio volume level", "オーディオ音量レベル", "音频音量级别"), "float", "1.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"loadVideo", loc("Load video from file", "ファイルから動画を読み込み", "从文件加载视频"), 
             "bool", {"QString"}, {"filePath"}},
            {"seek", loc("Seek to time position", "時間位置にシーク", "定位到时间位置"), 
             "void", {"float"}, {"time"}},
            {"play", loc("Start video playback", "動画再生を開始", "开始视频播放"), "void"},
            {"pause", loc("Pause video playback", "動画再生を一時停止", "暂停视频播放"), "void"},
            {"getFrame", loc("Get current video frame", "現在の動画フレームを取得", "获取当前视频帧"), 
             "QImage"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactImageLayer", "ArtifactAudioLayer"};
    }
};

// ============================================================================
// ArtifactImageLayer Description
// ============================================================================

class ImageLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactImageLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer that displays static images with transform and effect support.",
            "トランスフォームとエフェクトをサポートした静止画像を表示するレイヤー。",
            "显示支持变换和效果的静态图像的图层。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"sourcePath", loc("Image file path", "画像ファイルパス", "图像文件路径"), "QString"},
            {"width", loc("Image width in pixels", "画像幅（ピクセル）", "图像宽度（像素）"), "int"},
            {"height", loc("Image height in pixels", "画像高さ（ピクセル）", "图像高度（像素）"), "int"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"loadImage", loc("Load image from file", "ファイルから画像を読み込み", "从文件加载图像"), 
             "bool", {"QString"}, {"filePath"}},
            {"setImage", loc("Set image from QImage", "QImageから画像を設定", "从QImage设置图像"), 
             "void", {"QImage"}, {"image"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactVideoLayer", "ArtifactShapeLayer"};
    }
};

// ============================================================================
// ArtifactShapeLayer Description
// ============================================================================

class ShapeLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactShapeLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer for vector shapes with fill, stroke, and path animation.",
            "塗り、ストローク、パスアニメーションを持つベクターシェイプのレイヤー。",
            "具有填充、描边和路径动画的矢量形状图层。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ArtifactShapeLayer creates vector-based graphics using paths, shapes, and curves. "
            "It supports fill colors, stroke styles, gradients, and path morphing animations. "
            "Common shapes like rectangles, ellipses, and polygons are available as presets.",
            "ArtifactShapeLayerはパス、シェイプ、カーブを使用してベクターベースのグラフィックスを作成します。"
            "塗り色、ストロークスタイル、グラデーション、パスモーフィングアニメーションをサポートします。"
            "矩形、楕円、ポリゴンなどの一般的なシェイプはプリセットとして利用可能です。",
            "ArtifactShapeLayer使用路径、形状和曲线创建基于矢量的图形。"
            "它支持填充颜色、描边样式、渐变和路径变形动画。"
            "矩形、椭圆和多边形等常见形状可作为预设使用。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"fillColor", loc("Shape fill color", "シェイプの塗り色", "形状填充颜色"), "QColor"},
            {"strokeColor", loc("Stroke line color", "ストローク線の色", "描边线条颜色"), "QColor"},
            {"strokeWidth", loc("Stroke line width", "ストローク線幅", "描边线宽"), "float", "1.0"},
            {"strokeStyle", loc("Solid, dashed, or dotted", "実線、破線、点線", "实线、虚线或点线"), "StrokeStyle", "Solid"},
            {"fillEnabled", loc("Whether fill is visible", "塗りを表示するか", "是否显示填充"), "bool", "true"},
            {"strokeEnabled", loc("Whether stroke is visible", "ストロークを表示するか", "是否显示描边"), "bool", "true"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addRectangle", loc("Add rectangle shape", "矩形シェイプを追加", "添加矩形形状"), 
             "void", {"QRectF"}, {"rect"}},
            {"addEllipse", loc("Add ellipse shape", "楕円シェイプを追加", "添加椭圆形状"), 
             "void", {"QPointF", "float", "float"}, {"center", "rx", "ry"}},
            {"addPath", loc("Add custom path", "カスタムパスを追加", "添加自定义路径"), 
             "void", {"QPainterPath"}, {"path"}},
            {"clearShapes", loc("Remove all shapes", "全シェイプを削除", "移除所有形状"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactTextLayer", "ArtifactImageLayer"};
    }
};

// ============================================================================
// LayerMask Description
// ============================================================================

class LayerMaskDescription : public IDescribable {
public:
    QString className() const override { return "LayerMask"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Defines a mask that controls layer visibility with shapes or luminance.",
            "シェイプまたは輝度でレイヤーの可視性を制御するマスクを定義します。",
            "定义使用形状或亮度控制图层可见性的蒙版。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"enabled", loc("Whether mask is active", "マスクが有効か", "蒙版是否激活"), "bool", "true"},
            {"inverted", loc("Invert mask effect", "マスク効果を反転", "反转蒙版效果"), "bool", "false"},
            {"feather", loc("Edge softness", "エッジの柔らかさ", "边缘柔和度"), "float", "0.0"},
            {"expansion", loc("Mask expansion pixels", "マスク拡張ピクセル", "蒙版扩展像素"), "float", "0.0"},
            {"opacity", loc("Mask opacity", "マスク不透明度", "蒙版不透明度"), "float", "1.0"},
            {"blendMode", loc("How multiple masks combine", "複数マスクの合成方法", "多个蒙版的混合方式"), "MaskBlendMode", "Add"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setPath", loc("Set mask path shape", "マスクパスシェイプを設定", "设置蒙版路径形状"), 
             "void", {"QPainterPath"}, {"path"}},
            {"setFromLuminance", loc("Use image luminance as mask", "画像の輝度をマスクとして使用", "使用图像亮度作为蒙版"), 
             "void", {"QImage"}, {"image"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "MaskPath"};
    }
};

// ============================================================================
// ArtifactAudioLayer Description
// ============================================================================

class AudioLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactAudioLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A layer for audio playback with waveform visualization support.",
            "波形可視化をサポートしたオーディオ再生用レイヤー。",
            "支持波形可视化的音频播放图层。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"sourcePath", loc("Audio file path", "オーディオファイルパス", "音频文件路径"), "QString"},
            {"volume", loc("Playback volume", "再生音量", "播放音量"), "float", "1.0"},
            {"pan", loc("Stereo pan position", "ステレオパン位置", "立体声声像位置"), "float", "0.0"},
            {"muted", loc("Whether audio is muted", "オーディオをミュートするか", "是否静音"), "bool", "false"},
            {"duration", loc("Audio duration in seconds", "オーディオの長さ（秒）", "音频时长（秒）"), "float"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"loadAudio", loc("Load audio from file", "ファイルからオーディオを読み込み", "从文件加载音频"), 
             "bool", {"QString"}, {"filePath"}},
            {"play", loc("Start audio playback", "オーディオ再生を開始", "开始音频播放"), "void"},
            {"pause", loc("Pause audio playback", "オーディオ再生を一時停止", "暂停音频播放"), "void"},
            {"getWaveform", loc("Get audio waveform data", "オーディオ波形データを取得", "获取音频波形数据"), 
             "QVector<float>"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactAudioMixer", "ArtifactVideoLayer"};
    }
};

// ============================================================================
// ArtifactCameraLayer Description
// ============================================================================

class CameraLayerDescription : public IDescribable {
public:
    QString className() const override { return "ArtifactCameraLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "A camera layer for 3D composition view and perspective control.",
            "3Dコンポジションベューとパースペクティブ制御用のカメラレイヤー。",
            "用于3D合成视图和透视控制的摄像机图层。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Camera position in 3D space", "3D空間でのカメラ位置", "3D空间中的摄像机位置"), "QVector3D"},
            {"rotation", loc("Camera rotation angles", "カメラ回転角度", "摄像机旋转角度"), "QVector3D"},
            {"fov", loc("Field of view in degrees", "視野角（度）", "视野角度（度）"), "float", "45.0"},
            {"nearPlane", loc("Near clipping plane", "近クリッピング面", "近裁剪面"), "float", "1.0"},
            {"farPlane", loc("Far clipping plane", "遠クリッピング面", "远裁剪面"), "float", "10000.0"},
            {"depthOfField", loc("Enable depth of field blur", "被写界深度ブラーを有効化", "启用景深模糊"), "bool", "false"},
            {"focalDistance", loc("Focus distance for DoF", "DoFのフォーカス距離", "景深的对焦距离"), "float", "500.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"lookAt", loc("Point camera at target", "カメラをターゲットに向ける", "将摄像机指向目标"), 
             "void", {"QVector3D"}, {"target"}},
            {"reset", loc("Reset to default view", "デフォルトビューにリセット", "重置为默认视图"), "void"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactAbstractLayer", "ArtifactComposition"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<TextLayerDescription> _reg_TextLayer("ArtifactTextLayer");
static AutoRegisterDescribable<VideoLayerDescription> _reg_VideoLayer("ArtifactVideoLayer");
static AutoRegisterDescribable<ImageLayerDescription> _reg_ImageLayer("ArtifactImageLayer");
static AutoRegisterDescribable<ShapeLayerDescription> _reg_ShapeLayer("ArtifactShapeLayer");
static AutoRegisterDescribable<LayerMaskDescription> _reg_LayerMask("LayerMask");
static AutoRegisterDescribable<AudioLayerDescription> _reg_AudioLayer("ArtifactAudioLayer");
static AutoRegisterDescribable<CameraLayerDescription> _reg_CameraLayer("ArtifactCameraLayer");

} // namespace ArtifactCore