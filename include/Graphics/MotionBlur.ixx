module;
#include <QImage>
#include <QPainter>
#include <cmath>
#include <algorithm>

export module Graphics.MotionBlur;

export namespace ArtifactCore {

// ============================================================
// Motion Blur Parameters
// ============================================================

struct MotionBlurParams {
    bool enabled = false;
    float shutterAngle = 0.5f;        // 0.0 ~ 1.0 (0.5 = 180度)
    int shutterSamples = 8;            // 1 ~ 32
    float shutterPhase = -0.25f;       // -0.5 ~ 0.5
    int adaptiveSampleLimit = 16;
};

// ============================================================
// Motion Blur Processor
// ============================================================

class MotionBlurProcessor {
public:
    // Compute velocity from two transforms
    // Returns normalized velocity in pixels per frame
    struct Velocity {
        float vx = 0.0f;
        float vy = 0.0f;
        float magnitude = 0.0f;
        float angle = 0.0f;
    };

    static Velocity computeVelocity(
        float prevX, float prevY, float prevRotation, float prevScaleX, float prevScaleY,
        float currX, float currY, float currRotation, float currScaleX, float currScaleY,
        float deltaTime)
    {
        Velocity v;
        float dx = (currX - prevX);
        float dy = (currY - prevY);

        // Include rotation velocity
        float dRot = currRotation - prevRotation;
        // Approximate: rotation contributes to tangential velocity

        // Include scale velocity
        float dScaleX = currScaleX - prevScaleX;
        float dScaleY = currScaleY - prevScaleY;

        if (deltaTime > 0.0001f) {
            v.vx = dx / deltaTime;
            v.vy = dy / deltaTime;
        }

        v.magnitude = std::sqrt(v.vx * v.vx + v.vy * v.vy);
        v.angle = std::atan2(v.vy, v.vx);
        return v;
    }

    // Apply directional motion blur to a QImage
    static QImage applyMotionBlur(const QImage& source, const Velocity& velocity, const MotionBlurParams& params)
    {
        if (!params.enabled || velocity.magnitude < 0.5f) {
            return source;
        }

        const int w = source.width();
        const int h = source.height();
        const int samples = std::clamp(params.shutterSamples, 1, 32);

        // Directional blur kernel
        float blurLength = velocity.magnitude * params.shutterAngle;
        float cosA = std::cos(velocity.angle);
        float sinA = std::sin(velocity.angle);

        QImage result(w, h, QImage::Format_ARGB32_Premultiplied);
        result.fill(0);

        // Multi-sample accumulation
        for (int s = 0; s < samples; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(samples - 1);
            float offset = (t - 0.5f) * blurLength;

            int dx = static_cast<int>(std::round(cosA * offset));
            int dy = static_cast<int>(std::round(sinA * offset));

            QPainter painter(&result);
            painter.setOpacity(1.0 / samples);
            painter.drawImage(dx, dy, source);
            painter.end();
        }

        return result;
    }

    // Check if motion blur should be applied (based on velocity threshold)
    static bool shouldApply(const Velocity& velocity, float threshold = 0.5f) {
        return velocity.magnitude >= threshold;
    }
};

}
