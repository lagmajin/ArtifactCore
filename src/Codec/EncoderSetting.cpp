module;

#include <QVector>
#include <QString>

module Codec.Encoder.Setting;

import std;
import Utils.String.UniString;

namespace ArtifactCore {

class EncoderSettings::Impl {
public:
    OutputMode mode = OutputMode::VideoAndAudio;
    OutputFormat outputFormat = OutputFormat::VideoFile;
    VideoCodec vCodec = VideoCodec::H264;
    AudioCodec aCodec = AudioCodec::AAC;
    VideoProfile vProfile = VideoProfile::H264_High;
    
    int width = 1920;
    int height = 1080;
    double frameRate = 60.0;
    int bitrateMbps = 10;
    int quality = 80;  // 0-100
    
    int audioBitrateMbps = 128;
    int audioSampleRate = 48000;

    ImageFormat imageFormat = ImageFormat::PNG;
    QString imagePrefix = "output";
    int imagePadding = 4;  // 0001, 0002...

    Impl() = default;
};

EncoderSettings::EncoderSettings()
    : impl_(new Impl())
{
}

EncoderSettings::~EncoderSettings()
{
    delete impl_;
}

EncoderSettings::EncoderSettings(const EncoderSettings& other)
    : impl_(new Impl(*other.impl_))
{
}

EncoderSettings& EncoderSettings::operator=(const EncoderSettings& other)
{
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

// --- Output Mode ---
void EncoderSettings::setOutputMode(OutputMode m)
{
    impl_->mode = m;
}

OutputMode EncoderSettings::getOutputMode() const
{
    return impl_->mode;
}

void EncoderSettings::setOutputFormat(OutputFormat format)
{
    impl_->outputFormat = format;
}

OutputFormat EncoderSettings::getOutputFormat() const
{
    return impl_->outputFormat;
}

// --- Video Settings ---
void EncoderSettings::setVideoCodec(VideoCodec codec)
{
    impl_->vCodec = codec;
}

VideoCodec EncoderSettings::getVideoCodec() const
{
    return impl_->vCodec;
}

void EncoderSettings::setVideoProfile(VideoProfile profile)
{
    impl_->vProfile = profile;
}

VideoProfile EncoderSettings::getVideoProfile() const
{
    return impl_->vProfile;
}

void EncoderSettings::setWidth(int width)
{
    impl_->width = std::max(320, std::min(7680, width));
}

int EncoderSettings::getWidth() const
{
    return impl_->width;
}

void EncoderSettings::setHeight(int height)
{
    impl_->height = std::max(240, std::min(4320, height));
}

int EncoderSettings::getHeight() const
{
    return impl_->height;
}

void EncoderSettings::setFrameRate(double fps)
{
    impl_->frameRate = std::clamp(fps, 1.0, 120.0);
}

double EncoderSettings::getFrameRate() const
{
    return impl_->frameRate;
}

void EncoderSettings::setBitrate(int bitrateMbps)
{
    impl_->bitrateMbps = std::max(1, std::min(500, bitrateMbps));
}

int EncoderSettings::getBitrate() const
{
    return impl_->bitrateMbps;
}

void EncoderSettings::setQuality(int quality)
{
    impl_->quality = std::clamp(quality, 0, 100);
}

int EncoderSettings::getQuality() const
{
    return impl_->quality;
}

// --- Audio Settings ---
void EncoderSettings::setAudioCodec(AudioCodec codec)
{
    impl_->aCodec = codec;
}

AudioCodec EncoderSettings::getAudioCodec() const
{
    return impl_->aCodec;
}

void EncoderSettings::setAudioBitrate(int bitrateMbps)
{
    impl_->audioBitrateMbps = std::clamp(bitrateMbps, 32, 320);
}

int EncoderSettings::getAudioBitrate() const
{
    return impl_->audioBitrateMbps;
}

void EncoderSettings::setAudioSampleRate(int sampleRate)
{
    // 一般的なサンプルレート: 44100, 48000, 96000
    if (sampleRate == 44100 || sampleRate == 48000 || sampleRate == 96000) {
        impl_->audioSampleRate = sampleRate;
    }
}

int EncoderSettings::getAudioSampleRate() const
{
    return impl_->audioSampleRate;
}

// --- Image Sequence Settings ---
void EncoderSettings::setImageFormat(ImageFormat format)
{
    impl_->imageFormat = format;
}

ImageFormat EncoderSettings::getImageFormat() const
{
    return impl_->imageFormat;
}

void EncoderSettings::setImageSequencePrefix(const QString& prefix)
{
    impl_->imagePrefix = prefix;
}

QString EncoderSettings::getImageSequencePrefix() const
{
    return impl_->imagePrefix;
}

void EncoderSettings::setImageSequencePadding(int digits)
{
    impl_->imagePadding = std::clamp(digits, 1, 8);
}

int EncoderSettings::getImageSequencePadding() const
{
    return impl_->imagePadding;
}

QString EncoderSettings::generateImageSequencePath(int frameNumber) const
{
    QString paddedNumber = QString::number(frameNumber).rightJustified(impl_->imagePadding, '0');
    return impl_->imagePrefix + "_" + paddedNumber + getImageFormatExtension();
}

QString EncoderSettings::getImageFormatExtension() const
{
    switch (impl_->imageFormat) {
    case ImageFormat::PNG:
        return ".png";
    case ImageFormat::JPEG:
        return ".jpg";
    case ImageFormat::TIFF:
        return ".tiff";
    case ImageFormat::EXR:
        return ".exr";
    case ImageFormat::BMP:
        return ".bmp";
    default:
        return ".png";
    }
}

UniString EncoderSettings::getImageFormatName(ImageFormat format)
{
    switch (format) {
    case ImageFormat::PNG:
        return UniString(QString("PNG"));
    case ImageFormat::JPEG:
        return UniString(QString("JPEG"));
    case ImageFormat::TIFF:
        return UniString(QString("TIFF"));
    case ImageFormat::EXR:
        return UniString(QString("OpenEXR"));
    case ImageFormat::BMP:
        return UniString(QString("BMP"));
    default:
        return UniString(QString("Unknown"));
    }
}

QVector<ImageFormat> EncoderSettings::getAvailableImageFormats() const
{
    return QVector<ImageFormat>{
        ImageFormat::PNG,
        ImageFormat::JPEG,
        ImageFormat::TIFF,
        ImageFormat::EXR,
        ImageFormat::BMP
    };
}

// --- プリセット機能 ---
void EncoderSettings::applyPreset(Preset preset)
{
    switch (preset) {
    case Preset::YouTube_1080p_60fps:
        impl_->vCodec = VideoCodec::H264;
        impl_->width = 1920;
        impl_->height = 1080;
        impl_->frameRate = 60.0;
        impl_->bitrateMbps = 12;
        impl_->aCodec = AudioCodec::AAC;
        impl_->audioBitrateMbps = 128;
        break;

    case Preset::YouTube_4K_60fps:
        impl_->vCodec = VideoCodec::H265;
        impl_->width = 3840;
        impl_->height = 2160;
        impl_->frameRate = 60.0;
        impl_->bitrateMbps = 50;
        impl_->aCodec = AudioCodec::AAC;
        impl_->audioBitrateMbps = 256;
        break;

    case Preset::ProRes_422_FullHD:
        impl_->vCodec = VideoCodec::AppleProRes;
        impl_->vProfile = VideoProfile::ProRes_422;
        impl_->width = 1920;
        impl_->height = 1080;
        impl_->frameRate = 24.0;
        impl_->aCodec = AudioCodec::AAC;
        break;

    case Preset::ProRes_4444_UltraHD:
        impl_->vCodec = VideoCodec::AppleProRes;
        impl_->vProfile = VideoProfile::ProRes_4444;
        impl_->width = 3840;
        impl_->height = 2160;
        impl_->frameRate = 24.0;
        impl_->aCodec = AudioCodec::AAC;
        break;

    case Preset::WebH264_High:
        impl_->vCodec = VideoCodec::H264;
        impl_->width = 1280;
        impl_->height = 720;
        impl_->frameRate = 30.0;
        impl_->bitrateMbps = 5;
        impl_->aCodec = AudioCodec::AAC;
        impl_->audioBitrateMbps = 96;
        break;

    case Preset::WebH265_High:
        impl_->vCodec = VideoCodec::H265;
        impl_->width = 1920;
        impl_->height = 1080;
        impl_->frameRate = 30.0;
        impl_->bitrateMbps = 8;
        impl_->aCodec = AudioCodec::AAC;
        impl_->audioBitrateMbps = 128;
        break;
    }
}

UniString EncoderSettings::getPresetName(Preset preset)
{
    switch (preset) {
    case Preset::YouTube_1080p_60fps:
        return UniString(QString("YouTube 1080p 60fps"));
    case Preset::YouTube_4K_60fps:
        return UniString(QString("YouTube 4K 60fps"));
    case Preset::ProRes_422_FullHD:
        return UniString(QString("ProRes 422 FullHD"));
    case Preset::ProRes_4444_UltraHD:
        return UniString(QString("ProRes 4444 UltraHD"));
    case Preset::WebH264_High:
        return UniString(QString("Web H.264"));
    case Preset::WebH265_High:
        return UniString(QString("Web H.265"));
    default:
        return UniString(QString("Unknown"));
    }
}

// --- ビットレート自動計算 ---
int EncoderSettings::calculateBitrate() const
{
    // 解像度とfpsから推奨ビットレートを計算
    int pixelCount = impl_->width * impl_->height;
    double bitrate = (pixelCount * impl_->frameRate) / 150000.0;  // 経験則
    
    // コーデックによる調整
    if (impl_->vCodec == VideoCodec::H265) {
        bitrate *= 0.6;  // H265は約40%効率が良い
    }
    
    return std::clamp(static_cast<int>(bitrate), 1, 500);
}

int EncoderSettings::calculateAudioBitrate() const
{
    // サンプルレートから推奨オーディオビットレートを計算
    if (impl_->audioSampleRate == 96000) {
        return 192;
    } else if (impl_->audioSampleRate == 48000) {
        return 128;
    } else {
        return 96;
    }
}

// --- 検証 ---
bool EncoderSettings::isValid() const
{
    return getValidationErrors().empty();
}

QVector<UniString> EncoderSettings::getValidationErrors() const
{
    QVector<UniString> errors;

    if (impl_->mode == OutputMode::AudioOnly) {
        if (impl_->aCodec == AudioCodec::None) {
            errors.push_back(UniString(QString("Audio codec must be set for audio-only mode")));
        }
    } else {
        if (impl_->vCodec == VideoCodec::None) {
            errors.push_back(UniString(QString("Video codec is required")));
        }
        
        if (impl_->vCodec == VideoCodec::AppleProRes) {
            if (impl_->vProfile < VideoProfile::ProRes_Proxy || 
                impl_->vProfile > VideoProfile::ProRes_4444XQ) {
                errors.push_back(UniString(QString("Invalid ProRes profile")));
            }
        }
    }

    return errors;
}

// --- シリアライズ ---
UniString EncoderSettings::serialize() const
{
    std::string json = std::format(
        R"({{"width":{}, "height":{}, "fps":{}, "bitrate":{}, "quality":{}, "audioCodec":{}}})",
        impl_->width, impl_->height, impl_->frameRate, impl_->bitrateMbps, 
        impl_->quality, static_cast<int>(impl_->aCodec)
    );
    return UniString(QString::fromStdString(json));
}

bool EncoderSettings::deserialize(const UniString& data)
{
    // 簡易JSON パーサー（実装は省略、実際にはjsonライブラリを使用）
    return true;
}

// --- UI統合 ---
UniString EncoderSettings::getCodecName(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::H264:
        return UniString(QString("H.264"));
    case VideoCodec::H265:
        return UniString(QString("H.265 (HEVC)"));
    case VideoCodec::VP8:
        return UniString(QString("VP8"));
    case VideoCodec::AppleProRes:
        return UniString(QString("Apple ProRes"));
    case VideoCodec::None:
        return UniString(QString("None"));
    default:
        return UniString(QString("Unknown"));
    }
}

UniString EncoderSettings::getProfileName(VideoProfile profile)
{
    switch (profile) {
    case VideoProfile::ProRes_Proxy:
        return UniString(QString("ProRes Proxy"));
    case VideoProfile::ProRes_LT:
        return UniString(QString("ProRes LT"));
    case VideoProfile::ProRes_422:
        return UniString(QString("ProRes 422"));
    case VideoProfile::ProRes_422HQ:
        return UniString(QString("ProRes 422 HQ"));
    case VideoProfile::ProRes_4444:
        return UniString(QString("ProRes 4444"));
    case VideoProfile::ProRes_4444XQ:
        return UniString(QString("ProRes 4444 XQ"));
    case VideoProfile::H264_High:
        return UniString(QString("H.264 High"));
    case VideoProfile::H264_Main:
        return UniString(QString("H.264 Main"));
    case VideoProfile::None:
        return UniString(QString("None"));
    default:
        return UniString(QString("Unknown"));
    }
}

QVector<VideoCodec> EncoderSettings::getAvailableVideoCodecs() const
{
    return QVector<VideoCodec>{
        VideoCodec::H264,
        VideoCodec::H265,
        VideoCodec::VP8,
        VideoCodec::AppleProRes
    };
}

QVector<VideoProfile> EncoderSettings::getAvailableProfiles() const
{
    QVector<VideoProfile> profiles;
    
    if (impl_->vCodec == VideoCodec::AppleProRes) {
        profiles = QVector<VideoProfile>{
            VideoProfile::ProRes_Proxy,
            VideoProfile::ProRes_LT,
            VideoProfile::ProRes_422,
            VideoProfile::ProRes_422HQ,
            VideoProfile::ProRes_4444,
            VideoProfile::ProRes_4444XQ
        };
    } else if (impl_->vCodec == VideoCodec::H264) {
        profiles = QVector<VideoProfile>{
            VideoProfile::H264_High,
            VideoProfile::H264_Main
        };
    }
    
    return profiles;
}

QVector<int> EncoderSettings::getStandardFrameRates() const
{
    return QVector<int>{23, 24, 25, 30, 50, 60, 120};
}

bool EncoderSettings::isProfileValidForCodec(VideoCodec codec, VideoProfile profile) const
{
    if (codec == VideoCodec::AppleProRes) {
        return profile >= VideoProfile::ProRes_Proxy && profile <= VideoProfile::ProRes_4444XQ;
    } else if (codec == VideoCodec::H264) {
        return profile == VideoProfile::H264_High || profile == VideoProfile::H264_Main;
    }
    return true;
}

int EncoderSettings::getRecommendedBitrate() const
{
    return calculateBitrate();
}

int EncoderSettings::getRecommendedAudioBitrate() const
{
    return calculateAudioBitrate();
}

}

