module;

#include <QString>
#include <QSize>
#include <QDir>

export module Asset.ProxySettings;

import std;

/**
 * @brief Proxy editing settings
 * 
 * プロキシ編集のための設定クラス
 * 解像度、コーデック、品質などの設定を管理
 */
export namespace ArtifactCore {

    /// @brief プロキシ解像度のプリセット
    enum class ProxyResolution {
        Original,     // 元解像度を使用（プロキシ無効）
        Half,         // 1/2解像度
        Quarter,      // 1/4解像度
        Custom,       // カスタム解像度
        
        // 固定解像度プリセット
        Res480p,      // 854x480
        Res720p,      // 1280x720
        Res1080p,     // 1920x1080
        Res1440p,     // 2560x1440
        Res4K,        // 3840x2160
    };

    /// @brief プロキシコーデック
    enum class ProxyCodec {
        H264,         // H.264/AVC
        H265,         // H.265/HEVC
        VP9,          // VP9
        ProRes422,    // ProRes 422
        ProRes4444,   // ProRes 4444
        DNxHD,        // DNxHD
        Uncompressed, // 無圧縮
    };

    /// @brief プロキシ品質レベル
    enum class ProxyQuality {
        Low,          // 低品質（ファイルサイズ最小）
        Medium,       // 中品質
        High,         // 高品質
        Lossless,     // 無劣化
    };

    /// @brief プロキシ生成ポリシー
    enum class ProxyPolicy {
        Manual,       // 手動でのみ生成
        OnImport,     // インポート時に自動生成
        OnDemand,     // 必要時に遅延生成
        Adaptive,     // 自動（再生パフォーマンスに基づく）
    };

    /// @brief プロキシ設定クラス
    export class ProxySettings {
    public:
        ProxySettings();
        ~ProxySettings() = default;

        // コピー・ムーブ
        ProxySettings(const ProxySettings&) = default;
        ProxySettings& operator=(const ProxySettings&) = default;
        ProxySettings(ProxySettings&&) = default;
        ProxySettings& operator=(ProxySettings&&) = default;

        // ===== 設定メソッド =====

        /// @brief 解像度プリセットを設定
        void setResolution(ProxyResolution resolution);
        ProxyResolution getResolution() const { return resolution_; }

        /// @brief カスタム解像度を設定（原点を優先度が高い場合のみ有効）
        void setCustomResolution(int width, int height);
        QSize getCustomResolution() const { return customResolution_; }

        /// @brief 実際のプロキシ解像度を計算（元の解像度から）
        QSize calculateProxySize(int originalWidth, int originalHeight) const;

        /// @brief コーデックを設定
        void setCodec(ProxyCodec codec);
        ProxyCodec getCodec() const { return codec_; }

        /// @brief 品質を設定
        void setQuality(ProxyQuality quality);
        ProxyQuality getQuality() const { return quality_; }

        /// @brief 生成ポリシーを設定
        void setPolicy(ProxyPolicy policy);
        ProxyPolicy getPolicy() const { return policy_; }

        /// @brief プロキシデレクトリを設定
        void setProxyDirectory(const QString& directory);
        QString getProxyDirectory() const { return proxyDirectory_; }

        /// @brief プロキシファイル名のサフィックスを設定
        void setProxySuffix(const QString& suffix);
        QString getProxySuffix() const { return proxySuffix_; }

        /// @brief 有効/無効
        void setEnabled(bool enabled);
        bool isEnabled() const { return enabled_; }

        /// @brief ハードウェアエンコードを有効にするか
        void setHardwareEncoding(bool enabled);
        bool isHardwareEncoding() const { return hardwareEncoding_; }

        // ===== ファイルパス関連 =====

        /// @brief 元ファイルからプロキシパスを生成
        QString generateProxyPath(const QString& originalPath) const;

        /// @brief プロキシファイル是否存在を確認
        bool proxyExists(const QString& originalPath) const;

        /// @brief プロキシフリアンを取得
        QDir getProxyDirectoryAsDir() const;

        // ===== 設定の保存/読み込み =====

        /// @brief JSONとしてシリアライズ
        QString toJson() const;

        /// @brief JSONからデシリアライズ
        static ProxySettings fromJson(const QString& json);

        // ===== 静的なデフォルト設定 =====

        /// @brief デフォルト設定を取得
        static ProxySettings defaultSettings();

        /// @brief 編集向け設定（高品質）
        static ProxySettings editingSettings();

        /// @brief プレビュー向け設定（高速）
        static ProxySettings previewSettings();

    private:
        ProxyResolution resolution_ = ProxyResolution::Half;
        QSize customResolution_{1280, 720};
        ProxyCodec codec_ = ProxyCodec::H264;
        ProxyQuality quality_ = ProxyQuality::Medium;
        ProxyPolicy policy_ = ProxyPolicy::OnDemand;
        QString proxyDirectory_;
        QString proxySuffix_ = "_proxy";
        bool enabled_ = true;
        bool hardwareEncoding_ = true;
    };

    // ===== ヘルパー関数 =====

    /// @brief ProxyResolutionから代表的なサイズを取得
    export QSize resolutionToSize(ProxyResolution resolution, int originalWidth, int originalHeight);

    /// @brief ファイル拡張子を取得
    export QString codecToExtension(ProxyCodec codec);

    /// @brief コーデックFriendly名を取得
    export QString codecToFriendlyName(ProxyCodec codec);

} // namespace ArtifactCore
