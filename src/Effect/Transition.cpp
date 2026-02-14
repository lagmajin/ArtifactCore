module;

#include <cmath>

export module Effect.Transition;

import Effect.Transition;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

    // コンストラクタ
    TransitionEffect::TransitionEffect(QObject* parent)
        : QObject(parent)
    {
    }

    // デストラクタ
    TransitionEffect::~TransitionEffect() = default;

    // トランジションタイプを設定
    void TransitionEffect::setTransitionType(TransitionType type)
    {
        settings_.type = type;
    }

    // トランジション設定整体を設定
    void TransitionEffect::setSettings(const TransitionSettings& settings)
    {
        settings_ = settings;
    }

    // 持続時間を設定
    void TransitionEffect::setDuration(int durationMs)
    {
        settings_.duration = durationMs;
    }

    // イージング関数を設定
    void TransitionEffect::setEasing(EasingFunction easing)
    {
        settings_.easing = easing;
    }

    // イージング適用後の進行度を計算
    float TransitionEffect::calculateEasedProgress(float rawProgress) const
    {
        return applyEasing(rawProgress, settings_.easing);
    }

    // トランジションを適用（単一フレーム）
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::apply(
        const std::shared_ptr<ImageF32x4_RGBA>& fromFrame,
        const std::shared_ptr<ImageF32x4_RGBA>& toFrame,
        float progress)
    {
        if (!fromFrame || !toFrame) {
            return nullptr;
        }

        // イージングを適用
        float easedProgress = calculateEasedProgress(progress);

        switch (settings_.type) {
        case TransitionType::None:
            return (progress < 0.5f) ? fromFrame : toFrame;

        case TransitionType::CrossDissolve:
            return applyCrossDissolve(fromFrame, toFrame, easedProgress);

        case TransitionType::DipToColor:
            return applyFadeToColor(fromFrame, toFrame, easedProgress);

        case TransitionType::FadeIn:
            return applyCrossDissolve(nullptr, toFrame, easedProgress);

        case TransitionType::FadeOut:
            return applyCrossDissolve(fromFrame, nullptr, easedProgress);

        case TransitionType::WipeLeft:
            return applyWipe(fromFrame, toFrame, easedProgress, TransitionType::WipeLeft);

        case TransitionType::WipeRight:
            return applyWipe(fromFrame, toFrame, easedProgress, TransitionType::WipeRight);

        case TransitionType::WipeUp:
            return applyWipe(fromFrame, toFrame, easedProgress, TransitionType::WipeUp);

        case TransitionType::WipeDown:
            return applyWipe(fromFrame, toFrame, easedProgress, TransitionType::WipeDown);

        case TransitionType::SlideLeft:
            return applySlide(fromFrame, toFrame, easedProgress, TransitionType::SlideLeft);

        case TransitionType::SlideRight:
            return applySlide(fromFrame, toFrame, easedProgress, TransitionType::SlideRight);

        case TransitionType::SlideUp:
            return applySlide(fromFrame, toFrame, easedProgress, TransitionType::SlideUp);

        case TransitionType::SlideDown:
            return applySlide(fromFrame, toFrame, easedProgress, TransitionType::SlideDown);

        case TransitionType::ZoomIn:
            return applyZoom(fromFrame, toFrame, easedProgress, true);

        case TransitionType::ZoomOut:
            return applyZoom(fromFrame, toFrame, easedProgress, false);

        case TransitionType::PushLeft:
            return applyPush(fromFrame, toFrame, easedProgress, TransitionType::PushLeft);

        case TransitionType::PushRight:
            return applyPush(fromFrame, toFrame, easedProgress, TransitionType::PushRight);

        case TransitionType::Morph:
            // モーフは基本的なクロスディゾルブとして扱う
            return applyCrossDissolve(fromFrame, toFrame, easedProgress);

        default:
            return applyCrossDissolve(fromFrame, toFrame, easedProgress);
        }
    }

    // バッチ処理
    std::vector<std::shared_ptr<ImageF32x4_RGBA>> TransitionEffect::applyBatch(
        const std::vector<std::shared_ptr<ImageF32x4_RGBA>>& fromFrames,
        const std::vector<std::shared_ptr<ImageF32x4_RGBA>>& toFrames)
    {
        std::vector<std::shared_ptr<ImageF32x4_RGBA>> result;
        
        size_t frameCount = std::max(fromFrames.size(), toFrames.size());
        if (frameCount == 0) {
            return result;
        }

        result.reserve(frameCount);

        for (size_t i = 0; i < frameCount; ++i) {
            float progress = static_cast<float>(i) / static_cast<float>(frameCount - 1);
            
            auto fromFrame = (i < fromFrames.size()) ? fromFrames[i] : nullptr;
            auto toFrame = (i < toFrames.size()) ? toFrames[i] : nullptr;
            
            result.push_back(apply(fromFrame, toFrame, progress));
        }

        return result;
    }

    // クロスディゾルブの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applyCrossDissolve(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress)
    {
        if (!from && !to) return nullptr;
        if (!from) return to;
        if (!to) return from;

        int width = from->width();
        int height = from->height();

        auto output = std::make_shared<ImageF32x4_RGBA>(width, height);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float4 fromColor = from->pixel(x, y);
                float4 toColor = to->pixel(x, y);
                
                // 線形補間
                float4 result;
                result.x = fromColor.x * (1.0f - progress) + toColor.x * progress;
                result.y = fromColor.y * (1.0f - progress) + toColor.y * progress;
                result.z = fromColor.z * (1.0f - progress) + toColor.z * progress;
                result.w = fromColor.w * (1.0f - progress) + toColor.w * progress;
                
                output->setPixel(x, y, result);
            }
        }

        return output;
    }

    // ワイプの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applyWipe(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress, TransitionType direction)
    {
        if (!from || !to) {
            return applyCrossDissolve(from, to, progress);
        }

        int width = from->width();
        int height = from->height();

        auto output = std::make_shared<ImageF32x4_RGBA>(width, height);

        bool useFrom = false;
        int boundary = 0;

        switch (direction) {
        case TransitionType::WipeLeft:
            boundary = static_cast<int>(width * (1.0f - progress));
            break;
        case TransitionType::WipeRight:
            boundary = static_cast<int>(width * progress);
            break;
        case TransitionType::WipeUp:
            boundary = static_cast<int>(height * (1.0f - progress));
            break;
        case TransitionType::WipeDown:
            boundary = static_cast<int>(height * progress);
            break;
        default:
            boundary = static_cast<int>(width * progress);
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                switch (direction) {
                case TransitionType::WipeLeft:
                    useFrom = x >= boundary;
                    break;
                case TransitionType::WipeRight:
                    useFrom = x < boundary;
                    break;
                case TransitionType::WipeUp:
                    useFrom = y >= boundary;
                    break;
                case TransitionType::WipeDown:
                    useFrom = y < boundary;
                    break;
                default:
                    useFrom = x < boundary;
                }

                auto color = useFrom ? from->pixel(x, y) : to->pixel(x, y);
                output->setPixel(x, y, color);
            }
        }

        return output;
    }

    // スライドの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applySlide(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress, TransitionType direction)
    {
        if (!from || !to) {
            return applyCrossDissolve(from, to, progress);
        }

        int width = from->width();
        int height = from->height();

        auto output = std::make_shared<ImageF32x4_RGBA>(width, height);

        int offsetX = 0, offsetY = 0;

        switch (direction) {
        case TransitionType::SlideLeft:
            offsetX = static_cast<int>(-width * progress);
            break;
        case TransitionType::SlideRight:
            offsetX = static_cast<int>(width * progress);
            break;
        case TransitionType::SlideUp:
            offsetY = static_cast<int>(-height * progress);
            break;
        case TransitionType::SlideDown:
            offsetY = static_cast<int>(height * progress);
            break;
        default:
            offsetX = static_cast<int>(-width * progress);
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int fromX = x - offsetX;
                int fromY = y - offsetY;
                int toX = x + offsetX;
                int toY = y + offsetY;

                float4 color;

                if (fromX >= 0 && fromX < width && fromY >= 0 && fromY < height) {
                    color = from->pixel(fromX, fromY);
                } else if (toX >= 0 && toX < width && toY >= 0 && toY < height) {
                    color = to->pixel(toX, toY);
                } else {
                    color = float4(0, 0, 0, 1);
                }

                output->setPixel(x, y, color);
            }
        }

        return output;
    }

    // ズームの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applyZoom(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress, bool zoomIn)
    {
        if (!from || !to) {
            return applyCrossDissolve(from, to, progress);
        }

        int width = from->width();
        int height = from->height();

        auto output = std::make_shared<ImageF32x4_RGBA>(width, height);

        // ズーム率
        float scale = zoomIn ? (1.0f + progress) : (2.0f - progress);
        float centerX = width / 2.0f;
        float centerY = height / 2.0f;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // 中心からの距離
                float srcX = (x - centerX) / scale + centerX;
                float srcY = (y - centerY) / scale + centerY;

                float4 color;

                if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                    // バイリニア補間（簡易版）
                    int x0 = static_cast<int>(srcX);
                    int y0 = static_cast<int>(srcY);
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;

                    if (x1 < width && y1 < height) {
                        float fx = srcX - x0;
                        float fy = srcY - y0;

                        float4 c00 = from->pixel(x0, y0);
                        float4 c10 = from->pixel(x1, y0);
                        float4 c01 = from->pixel(x0, y1);
                        float4 c11 = from->pixel(x1, y1);

                        color.x = (1 - fx) * (1 - fy) * c00.x + fx * (1 - fy) * c10.x + 
                                  (1 - fx) * fy * c01.x + fx * fy * c11.x;
                        color.y = (1 - fx) * (1 - fy) * c00.y + fx * (1 - fy) * c10.y + 
                                  (1 - fx) * fy * c01.y + fx * fy * c11.y;
                        color.z = (1 - fx) * (1 - fy) * c00.z + fx * (1 - fy) * c10.z + 
                                  (1 - fx) * fy * c01.z + fx * fy * c11.z;
                        color.w = (1 - fx) * (1 - fy) * c00.w + fx * (1 - fy) * c10.w + 
                                  (1 - fx) * fy * c01.w + fx * fy * c11.w;
                    } else {
                        color = from->pixel(x0, y0);
                    }
                } else {
                    color = to->pixel(x, y);
                }

                output->setPixel(x, y, color);
            }
        }

        return output;
    }

    // プッシュの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applyPush(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress, TransitionType direction)
    {
        if (!from || !to) {
            return applyCrossDissolve(from, to, progress);
        }

        int width = from->width();
        int height = from->height();

        auto output = std::make_shared<ImageF32x4_RGBA>(width, height);

        int offsetX = 0, offsetY = 0;

        switch (direction) {
        case TransitionType::PushLeft:
            offsetX = static_cast<int>(width * progress);
            break;
        case TransitionType::PushRight:
            offsetX = static_cast<int>(-width * progress);
            break;
        default:
            offsetX = static_cast<int>(width * progress);
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int fromX = x + offsetX;
                int toX = x - offsetX;

                float4 color;

                // toの方が奥（奥から手前に来る）
                if (toX >= 0 && toX < width) {
                    color = to->pixel(toX, y);
                } else if (fromX >= 0 && fromX < width) {
                    color = from->pixel(fromX, y);
                } else {
                    color = float4(0, 0, 0, 1);
                }

                output->setPixel(x, y, color);
            }
        }

        return output;
    }

    // カラーへのフェードの実装
    std::shared_ptr<ImageF32x4_RGBA> TransitionEffect::applyFadeToColor(
        const std::shared_ptr<ImageF32x4_RGBA>& from,
        const std::shared_ptr<ImageF32x4_RGBA>& to,
        float progress)
    {
        // ディップ色をパース
        QString colorStr = settings_.dipColor;
        float4 dipColor;

        if (colorStr.startsWith("#") && colorStr.length() >= 7) {
            bool ok;
            unsigned int r = colorStr.mid(1, 2).toUInt(&ok, 16);
            unsigned int g = colorStr.mid(3, 2).toUInt(&ok, 16);
            unsigned int b = colorStr.mid(5, 2).toUInt(&ok, 16);
            dipColor = float4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        } else {
            dipColor = float4(0, 0, 0, 1); // デフォルトは黒
        }

        // まずfromからdipColorへ、そのあとdipColorからtoへ
        float midProgress = progress * 2.0f;
        
        if (midProgress <= 1.0f) {
            // from -> dipColor
            if (from) {
                return applyCrossDissolve(from, nullptr, midProgress);
            }
            return nullptr;
        } else {
            // dipColor -> to
            if (to) {
                return applyCrossDissolve(nullptr, to, midProgress - 1.0f);
            }
            return nullptr;
        }
    }

    // トランジションタイプから名称を取得
    QString TransitionEffect::transitionTypeToName(TransitionType type)
    {
        switch (type) {
        case TransitionType::None: return "None";
        case TransitionType::CrossDissolve: return "Cross Dissolve";
        case TransitionType::DipToColor: return "Dip to Color";
        case TransitionType::FadeIn: return "Fade In";
        case TransitionType::FadeOut: return "Fade Out";
        case TransitionType::WipeLeft: return "Wipe Left";
        case TransitionType::WipeRight: return "Wipe Right";
        case TransitionType::WipeUp: return "Wipe Up";
        case TransitionType::WipeDown: return "Wipe Down";
        case TransitionType::SlideLeft: return "Slide Left";
        case TransitionType::SlideRight: return "Slide Right";
        case TransitionType::SlideUp: return "Slide Up";
        case TransitionType::SlideDown: return "Slide Down";
        case TransitionType::ZoomIn: return "Zoom In";
        case TransitionType::ZoomOut: return "Zoom Out";
        case TransitionType::PushLeft: return "Push Left";
        case TransitionType::PushRight: return "Push Right";
        case TransitionType::Morph: return "Morph";
        default: return "Unknown";
        }
    }

    // 名称からトランジションタイプを取得
    TransitionType TransitionEffect::nameToTransitionType(const QString& name)
    {
        QMap<QString, TransitionType> map = {
            {"None", TransitionType::None},
            {"Cross Dissolve", TransitionType::CrossDissolve},
            {"Dip to Color", TransitionType::DipToColor},
            {"Fade In", TransitionType::FadeIn},
            {"Fade Out", TransitionType::FadeOut},
            {"Wipe Left", TransitionType::WipeLeft},
            {"Wipe Right", TransitionType::WipeRight},
            {"Wipe Up", TransitionType::WipeUp},
            {"Wipe Down", TransitionType::WipeDown},
            {"Slide Left", TransitionType::SlideLeft},
            {"Slide Right", TransitionType::SlideRight},
            {"Slide Up", TransitionType::SlideUp},
            {"Slide Down", TransitionType::SlideDown},
            {"Zoom In", TransitionType::ZoomIn},
            {"Zoom Out", TransitionType::ZoomOut},
            {"Push Left", TransitionType::PushLeft},
            {"Push Right", TransitionType::PushRight},
            {"Morph", TransitionType::Morph},
        };
        
        return map.value(name, TransitionType::None);
    }

    // 全てのトランジションタイプ一覧を取得
    QList<TransitionType> TransitionEffect::getAllTransitionTypes()
    {
        return {
            TransitionType::None,
            TransitionType::CrossDissolve,
            TransitionType::DipToColor,
            TransitionType::FadeIn,
            TransitionType::FadeOut,
            TransitionType::WipeLeft,
            TransitionType::WipeRight,
            TransitionType::WipeUp,
            TransitionType::WipeDown,
            TransitionType::SlideLeft,
            TransitionType::SlideRight,
            TransitionType::SlideUp,
            TransitionType::SlideDown,
            TransitionType::ZoomIn,
            TransitionType::ZoomOut,
            TransitionType::PushLeft,
            TransitionType::PushRight,
            TransitionType::Morph,
        };
    }

    // イージング関数を適用
    float TransitionEffect::applyEasing(float t, EasingFunction easing)
    {
        t = std::max(0.0f, std::min(1.0f, t));

        switch (easing) {
        case EasingFunction::Linear:
            return t;

        case EasingFunction::EaseIn:
            return t * t;

        case EasingFunction::EaseOut:
            return t * (2.0f - t);

        case EasingFunction::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

        case EasingFunction::EaseInQuad:
            return t * t;

        case EasingFunction::EaseOutQuad:
            return t * (2.0f - t);

        case EasingFunction::EaseInOutQuad:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

        case EasingFunction::EaseInCubic:
            return t * t * t;

        case EasingFunction::EaseOutCubic:
            return (--t) * t * t + 1.0f;

        case EasingFunction::EaseInOutCubic:
            return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;

        case EasingFunction::Smooth:
            return t * t * (3.0f - 2.0f * t);

        default:
            return t;
        }
    }

    // 推奨トランジション時間を取得
    int TransitionEffect::getRecommendedDuration(TransitionType type)
    {
        switch (type) {
        case TransitionType::None:
            return 0;

        case TransitionType::CrossDissolve:
        case TransitionType::FadeIn:
        case TransitionType::FadeOut:
            return 500;

        case TransitionType::DipToColor:
            return 750;

        case TransitionType::WipeLeft:
        case TransitionType::WipeRight:
        case TransitionType::WipeUp:
        case TransitionType::WipeDown:
            return 400;

        case TransitionType::SlideLeft:
        case TransitionType::SlideRight:
        case TransitionType::SlideUp:
        case TransitionType::SlideDown:
            return 350;

        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
            return 600;

        case TransitionType::PushLeft:
        case TransitionType::PushRight:
            return 350;

        case TransitionType::Morph:
            return 1000;

        default:
            return 500;
        }
    }

    // EasingFunctionから名称を取得
    QString easingToName(EasingFunction easing)
    {
        switch (easing) {
        case EasingFunction::Linear: return "Linear";
        case EasingFunction::EaseIn: return "Ease In";
        case EasingFunction::EaseOut: return "Ease Out";
        case EasingFunction::EaseInOut: return "Ease In-Out";
        case EasingFunction::EaseInQuad: return "Ease In Quad";
        case EasingFunction::EaseOutQuad: return "Ease Out Quad";
        case EasingFunction::EaseInOutQuad: return "Ease In-Out Quad";
        case EasingFunction::EaseInCubic: return "Ease In Cubic";
        case EasingFunction::EaseOutCubic: return "Ease Out Cubic";
        case EasingFunction::EaseInOutCubic: return "Ease In-Out Cubic";
        case EasingFunction::Smooth: return "Smooth";
        default: return "Unknown";
        }
    }

    // EasingFunctionの説明を取得
    QString easingToDescription(EasingFunction easing)
    {
        switch (easing) {
        case EasingFunction::Linear: return "Constant speed";
        case EasingFunction::EaseIn: return "Starts slow, accelerates";
        case EasingFunction::EaseOut: return "Starts fast, decelerates";
        case EasingFunction::EaseInOut: return "Starts slow, accelerates, then decelerates";
        case EasingFunction::EaseInQuad: return "Quadratic acceleration";
        case EasingFunction::EaseOutQuad: return "Quadratic deceleration";
        case EasingFunction::EaseInOutQuad: return "Quadratic acceleration-deceleration";
        case EasingFunction::EaseInCubic: return "Cubic acceleration";
        case EasingFunction::EaseOutCubic: return "Cubic deceleration";
        case EasingFunction::EaseInOutCubic: return "Cubic acceleration-deceleration";
        case EasingFunction::Smooth: return "Smooth interpolation";
        default: return "";
        }
    }

} // namespace ArtifactCore
