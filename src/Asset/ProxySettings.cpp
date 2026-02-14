module;

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

export module Asset.ProxySettings;

import Asset.ProxySettings;

namespace ArtifactCore {

    // ProxySettings コンストラクタ
    ProxySettings::ProxySettings()
    {
        // デフォルトプロキシデレクトリをユーザーキャッシュフォルダに設定
        proxyDirectory_ = QDir::cleanPath(QDir::homePath() + "/.artifactcore/proxy");
    }

    // 解像度プリセットを設定
    void ProxySettings::setResolution(ProxyResolution resolution)
    {
        resolution_ = resolution;
    }

    // カスタム解像度を設定
    void ProxySettings::setCustomResolution(int width, int height)
    {
        customResolution_ = QSize(width, height);
    }

    // 実際のプロキシ解像度を計算
    QSize ProxySettings::calculateProxySize(int originalWidth, int originalHeight) const
    {
        switch (resolution_) {
        case ProxyResolution::Original:
            return QSize(originalWidth, originalHeight);
            
        case ProxyResolution::Half:
            return QSize(originalWidth / 2, originalHeight / 2);
            
        case ProxyResolution::Quarter:
            return QSize(originalWidth / 4, originalHeight / 4);
            
        case ProxyResolution::Custom:
            return customResolution_;
            
        case ProxyResolution::Res480p:
            return QSize(854, 480);
            
        case ProxyResolution::Res720p:
            return QSize(1280, 720);
            
        case ProxyResolution::Res1080p:
            return QSize(1920, 1080);
            
        case ProxyResolution::Res1440p:
            return QSize(2560, 1440);
            
        case ProxyResolution::Res4K:
            return QSize(3840, 2160);
            
        default:
            return QSize(originalWidth / 2, originalHeight / 2);
        }
    }

    // コーデックを設定
    void ProxySettings::setCodec(ProxyCodec codec)
    {
        codec_ = codec;
    }

    // 品質を設定
    void ProxySettings::setQuality(ProxyQuality quality)
    {
        quality_ = quality;
    }

    // 生成ポリシーを設定
    void ProxySettings::setPolicy(ProxyPolicy policy)
    {
        policy_ = policy;
    }

    // プロキシデレクトリを設定
    void ProxySettings::setProxyDirectory(const QString& directory)
    {
        proxyDirectory_ = directory;
    }

    // プロキシファイル名のサフィックスを設定
    void ProxySettings::setProxySuffix(const QString& suffix)
    {
        proxySuffix_ = suffix;
    }

    // 有効/無効を設定
    void ProxySettings::setEnabled(bool enabled)
    {
        enabled_ = enabled;
    }

    // ハードウェアエンコードを設定
    void ProxySettings::setHardwareEncoding(bool enabled)
    {
        hardwareEncoding_ = enabled;
    }

    // 元ファイルからプロキシパスを生成
    QString ProxySettings::generateProxyPath(const QString& originalPath) const
    {
        QFileInfo fileInfo(originalPath);
        QString baseName = fileInfo.baseName();
        QString extension = codecToExtension(codec_);
        
        return proxyDirectory_ + "/" + baseName + proxySuffix_ + "." + extension;
    }

    // プロキシファイル是否存在を確認
    bool ProxySettings::proxyExists(const QString& originalPath) const
    {
        QString proxyPath = generateProxyPath(originalPath);
        return QFileInfo::exists(proxyPath);
    }

    // プロキシディレクトリをQDirで取得
    QDir ProxySettings::getProxyDirectoryAsDir() const
    {
        return QDir(proxyDirectory_);
    }

    // JSONとしてシリアライズ
    QString ProxySettings::toJson() const
    {
        QJsonObject obj;
        obj["resolution"] = static_cast<int>(resolution_);
        obj["customWidth"] = customResolution_.width();
        obj["customHeight"] = customResolution_.height();
        obj["codec"] = static_cast<int>(codec_);
        obj["quality"] = static_cast<int>(quality_);
        obj["policy"] = static_cast<int>(policy_);
        obj["proxyDirectory"] = proxyDirectory_;
        obj["proxySuffix"] = proxySuffix_;
        obj["enabled"] = enabled_;
        obj["hardwareEncoding"] = hardwareEncoding_;
        
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    // JSONからデシリアライズ
    ProxySettings ProxySettings::fromJson(const QString& json)
    {
        ProxySettings settings;
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        
        if (!doc.isObject()) {
            return settings;
        }
        
        QJsonObject obj = doc.object();
        
        if (obj.contains("resolution")) {
            settings.setResolution(static_cast<ProxyResolution>(obj["resolution"].toInt()));
        }
        if (obj.contains("customWidth") && obj.contains("customHeight")) {
            settings.setCustomResolution(obj["customWidth"].toInt(), obj["customHeight"].toInt());
        }
        if (obj.contains("codec")) {
            settings.setCodec(static_cast<ProxyCodec>(obj["codec"].toInt()));
        }
        if (obj.contains("quality")) {
            settings.setQuality(static_cast<ProxyQuality>(obj["quality"].toInt()));
        }
        if (obj.contains("policy")) {
            settings.setPolicy(static_cast<ProxyPolicy>(obj["policy"].toInt()));
        }
        if (obj.contains("proxyDirectory")) {
            settings.setProxyDirectory(obj["proxyDirectory"].toString());
        }
        if (obj.contains("proxySuffix")) {
            settings.setProxySuffix(obj["proxySuffix"].toString());
        }
        if (obj.contains("enabled")) {
            settings.setEnabled(obj["enabled"].toBool());
        }
        if (obj.contains("hardwareEncoding")) {
            settings.setHardwareEncoding(obj["hardwareEncoding"].toBool());
        }
        
        return settings;
    }

    // デフォルト設定を取得
    ProxySettings ProxySettings::defaultSettings()
    {
        ProxySettings settings;
        settings.setResolution(ProxyResolution::Half);
        settings.setCodec(ProxyCodec::H264);
        settings.setQuality(ProxyQuality::Medium);
        settings.setPolicy(ProxyPolicy::OnDemand);
        settings.setEnabled(true);
        settings.setHardwareEncoding(true);
        return settings;
    }

    // 編集向け設定（高品質）
    ProxySettings ProxySettings::editingSettings()
    {
        ProxySettings settings;
        settings.setResolution(ProxyResolution::Res720p);
        settings.setCodec(ProxyCodec::ProRes422);
        settings.setQuality(ProxyQuality::High);
        settings.setPolicy(ProxyPolicy::OnImport);
        settings.setEnabled(true);
        settings.setHardwareEncoding(false);
        return settings;
    }

    // プレビュー向け設定（高速）
    ProxySettings ProxySettings::previewSettings()
    {
        ProxySettings settings;
        settings.setResolution(ProxyResolution::Quarter);
        settings.setCodec(ProxyCodec::H264);
        settings.setQuality(ProxyQuality::Low);
        settings.setPolicy(ProxyPolicy::OnDemand);
        settings.setEnabled(true);
        settings.setHardwareEncoding(true);
        return settings;
    }

    // ProxyResolutionから代表的なサイズを取得
    QSize resolutionToSize(ProxyResolution resolution, int originalWidth, int originalHeight)
    {
        ProxySettings settings;
        settings.setResolution(resolution);
        return settings.calculateProxySize(originalWidth, originalHeight);
    }

    // ファイル拡張子を取得
    QString codecToExtension(ProxyCodec codec)
    {
        switch (codec) {
        case ProxyCodec::H264:       return "mp4";
        case ProxyCodec::H265:       return "mp4";
        case ProxyCodec::VP9:        return "webm";
        case ProxyCodec::ProRes422:  return "mov";
        case ProxyCodec::ProRes4444: return "mov";
        case ProxyCodec::DNxHD:      return "mxf";
        case ProxyCodec::Uncompressed: return "avi";
        default:                     return "mp4";
        }
    }

    // コーデックFriendly名を取得
    QString codecToFriendlyName(ProxyCodec codec)
    {
        switch (codec) {
        case ProxyCodec::H264:       return "H.264/AVC";
        case ProxyCodec::H265:       return "H.265/HEVC";
        case ProxyCodec::VP9:        return "VP9";
        case ProxyCodec::ProRes422:  return "ProRes 422";
        case ProxyCodec::ProRes4444: return "ProRes 4444";
        case ProxyCodec::DNxHD:      return "DNxHD";
        case ProxyCodec::Uncompressed: return "Uncompressed";
        default:                     return "Unknown";
        }
    }

} // namespace ArtifactCore
