module;

#include <QString>
#include <QObject>
#include <memory>

export module Effect.Transition;

import std;

/**
 * @brief Transition Effects
 * 
 * 映像編集のためのトランジションエフェクト
 * クリップ間の切り替え効果を実装
 */
export namespace ArtifactCore {

    /// @brief トランジションの種類
    enum class TransitionType {
        None,           // トランジションなし（カット）
        
        // 基本トランジション
        CrossDissolve,  // クロスディゾルブ
        DipToColor,     // カラーへディップ
        FadeIn,         // フェードイン
        FadeOut,        // フェードアウト
        
        // 方向付きトランジション
        WipeLeft,       // 左へワイプ
        WipeRight,      // 右へワイプ
        WipeUp,         // 上へワイプ
        WipeDown,       // 下へワイプ
        
        //  특수トランジション
        SlideLeft,      // 左へスライド
        SlideRight,     // 右へスライド
        SlideUp,        // 上へスライド
        SlideDown,      // 下へスライド
        
        // 拡張トランジション
        ZoomIn,         // ズームイン
        ZoomOut,        // ズームアウト
        PushLeft,       // 左へプッシュ
        PushRight,      // 右へプッシュ
        
        // モーフ系
        Morph,          // モーフ（要対応フレーム）
    };

    /// @brief イージング関数
    enum class EasingFunction {
        Linear,         // 線形
        EaseIn,         // 加速
        EaseOut,        // 減速
        EaseInOut,      // 加速→減速
        EaseInQuad,     // 二次関数 加速
        EaseOutQuad,    // 二次関数 減速
        EaseInOutQuad,  // 二次関数 加速→減速
        EaseInCubic,    // 三次関数 加速
        EaseOutCubic,   // 三次関数 減速
        EaseInOutCubic, // 三次関数 加速→減速
        Smooth,         // スムーズ
    };

    /// @brief トランジション設定
    struct TransitionSettings {
        TransitionType type = TransitionType::CrossDissolve;
        EasingFunction easing = EasingFunction::EaseInOut;
        
        // 時間設定（ミリ秒）
        int duration = 500;           // トランジション時間
        int preRoll = 0;              // プレロール（秒数を戻す）
        int postRoll = 0;             // ポストロール
        
        // 颜色設定
        QString dipColor = "#000000"; // ディップ色
        
        // 方向（方向付きトランジション用）
        bool reverse = false;         // 逆再生
        
        // カスタム
        QString customShader;         // カスタムシェーダー
        float customParam1 = 0.0f;
        float customParam2 = 0.0f;
    };

    /// @brief フレームデータ
    struct TransitionFrame {
        // 入力フレーム
        std::shared_ptr<class ImageF32x4_RGBA> fromFrame;  // 開始側フレーム
        std::shared_ptr<class ImageF32x4_RGBA> toFrame;    // 終了側フレーム
        
        // 出力
        std::shared_ptr<class ImageF32x4_RGBA> outputFrame;
        
        // 情報
        int frameIndex = 0;           // フレームインデックス
        float progress = 0.0f;        // 進行度（0.0 - 1.0）
        float easedProgress = 0.0f;   // イージング適用後の進行度
    };

    /// @brief トランジション管理クラス
    export class TransitionEffect : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief コンストラクタ
         * @param parent 親オブジェクト
         */
        explicit TransitionEffect(QObject* parent = nullptr);
        
        ~TransitionEffect() override;

        // コピー・ムーブ禁止
        TransitionEffect(const TransitionEffect&) = delete;
        TransitionEffect& operator=(const TransitionEffect&) = delete;

        // ===== 設定 =====

        /// @brief トランジションタイプを設定
        void setTransitionType(TransitionType type);
        TransitionType getTransitionType() const { return settings_.type; }

        /// @brief トランジション設定全体を設 定
        void setSettings(const TransitionSettings& settings);
        const TransitionSettings& getSettings() const { return settings_; }

        /// @brief 持続時間を設定（ミリ秒）
        void setDuration(int durationMs);
        int getDuration() const { return settings_.duration; }

        /// @brief イージング関数を設定
        void setEasing(EasingFunction easing);
        EasingFunction getEasing() const { return settings_.easing; }

        // ===== フレーム処理 =====

        /// @brief トランジションを適用（単一フレーム）
        /// @param fromFrame 開始側フレーム
        /// @param toFrame 終了側フレーム
        /// @param progress 進行度（0.0 - 1.0）
        /// @return 合成されたフレーム
        std::shared_ptr<class ImageF32x4_RGBA> apply(
            const std::shared_ptr<class ImageF32x4_RGBA>& fromFrame,
            const std::shared_ptr<class ImageF32x4_RGBA>& toFrame,
            float progress);

        /// @brief トランジションを適用（フレームリスト）
        /// @param fromFrames 開始側フレームリスト
        /// @param toFrames 終了側フレームリスト
        /// @return 合成されたフレームリスト
        std::vector<std::shared_ptr<class ImageF32x4_RGBA>> applyBatch(
            const std::vector<std::shared_ptr<class ImageF32x4_RGBA>>& fromFrames,
            const std::vector<std::shared_ptr<class ImageF32x4_RGBA>>& toFrames);

        // ===== ユーティリティ =====

        /// @brief トランジションタイプから名称を取得
        static QString transitionTypeToName(TransitionType type);

        /// @brief 名称からトランジションタイプを取得
        static TransitionType nameToTransitionType(const QString& name);

        /// @brief 全てのトランジションタイプ一覧を取得
        static QList<TransitionType> getAllTransitionTypes();

        /// @brief イージング関数を適用
        static float applyEasing(float t, EasingFunction easing);

        /// @brief 推奨トランジション時間を取得
        static int getRecommendedDuration(TransitionType type);

    signals:
        /// @brief トランジション完了シグナル
        void transitionCompleted();

        /// @brief 進捗シグナル
        void progressUpdated(float progress);

    private:
        TransitionSettings settings_;

        // 内部処理メソッド
        float calculateEasedProgress(float rawProgress) const;
        
        // 各種トランジションの実装
        std::shared_ptr<class ImageF32x4_RGBA> applyCrossDissolve(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress);
        
        std::shared_ptr<class ImageF32x4_RGBA> applyWipe(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress, TransitionType direction);
        
        std::shared_ptr<class ImageF32x4_RGBA> applySlide(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress, TransitionType direction);
        
        std::shared_ptr<class ImageF32x4_RGBA> applyZoom(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress, bool zoomIn);
        
        std::shared_ptr<class ImageF32x4_RGBA> applyPush(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress, TransitionType direction);
        
        std::shared_ptr<class ImageF32x4_RGBA> applyFadeToColor(
            const std::shared_ptr<class ImageF32x4_RGBA>& from,
            const std::shared_ptr<class ImageF32x4_RGBA>& to,
            float progress);
    };

    // ===== ヘルパー関数 =====

    /// @brief EasingFunctionから名称を取得
    export QString easingToName(EasingFunction easing);

    /// @brief EasingFunctionの説明を取得
    export QString easingToDescription(EasingFunction easing);

} // namespace ArtifactCore
