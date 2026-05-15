module;
#include <utility>
#include <vector>
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
export module Layer.Matte;

import Utils.Id;

export namespace ArtifactCore {

enum class MatteMode {
    None,
    Alpha,
    AlphaInverted,
    Luminance,
    LuminanceInverted
};

enum class MatteStackMode {
    Add,
    Common,
    Subtract
};

inline QString matteStackModeToString(MatteStackMode mode);
inline MatteStackMode matteStackModeFromString(const QString& str);

inline QString matteStackModeToString(MatteStackMode mode) {
    switch (mode) {
    case MatteStackMode::Add:     return QStringLiteral("Add");
    case MatteStackMode::Common:  return QStringLiteral("Common");
    case MatteStackMode::Subtract: return QStringLiteral("Subtract");
    }
    return QStringLiteral("Unknown");
}

inline MatteStackMode matteStackModeFromString(const QString& str) {
    QString s = str.toLower().trimmed();
    if (s == "common") return MatteStackMode::Common;
    if (s == "subtract") return MatteStackMode::Subtract;
    return MatteStackMode::Add;
}

class MatteModeUtils {
public:
    static QString toString(MatteMode mode) {
        switch (mode) {
            case MatteMode::None: return "None";
            case MatteMode::Alpha: return "Alpha Matte";
            case MatteMode::AlphaInverted: return "Alpha Inverted Matte";
            case MatteMode::Luminance: return "Luma Matte";
            case MatteMode::LuminanceInverted: return "Luma Inverted Matte";
            default: return "Unknown";
        }
    }

    static MatteMode fromString(const QString& str) {
        QString s = str.toLower().trimmed().replace(" ", "");
        if (s == "none") return MatteMode::None;
        if (s == "alpha" || s == "alphamatte") return MatteMode::Alpha;
        if (s == "alphainverted" || s == "alphainvertedmatte") return MatteMode::AlphaInverted;
        if (s == "luminance" || s == "lumamatte") return MatteMode::Luminance;
        if (s == "luminanceinverted" || s == "lumainvertedmatte") return MatteMode::LuminanceInverted;
        return MatteMode::None;
    }

    static bool isInverted(MatteMode mode) {
        return mode == MatteMode::AlphaInverted || mode == MatteMode::LuminanceInverted;
    }

    static bool isLuminance(MatteMode mode) {
        return mode == MatteMode::Luminance || mode == MatteMode::LuminanceInverted;
    }
};

struct MatteNodeId : public Id {
    using Id::Id;
};

class MatteNode {
public:
    MatteNode()
        : id_(MatteNodeId())
        , sourceLayerId_()
        , mode_(MatteMode::Alpha)
        , enabled_(true)
        , order_(0)
    {}

    const MatteNodeId& id() const { return id_; }
    const Id& sourceLayerId() const { return sourceLayerId_; }
    void setSourceLayerId(const Id& id) { sourceLayerId_ = id; }

    MatteMode mode() const { return mode_; }
    void setMode(MatteMode mode) { mode_ = mode; }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    int order() const { return order_; }
    void setOrder(int order) { order_ = order; }

    QString debugString() const {
        return QStringLiteral("MatteNode(id=%1, source=%2, mode=%3, enabled=%4)")
            .arg(id_.toString())
            .arg(sourceLayerId_.toString())
            .arg(MatteModeUtils::toString(mode_))
            .arg(enabled_);
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), id_.toString());
        obj.insert(QStringLiteral("sourceLayerId"), sourceLayerId_.toString());
        obj.insert(QStringLiteral("mode"), MatteModeUtils::toString(mode_));
        obj.insert(QStringLiteral("enabled"), enabled_);
        obj.insert(QStringLiteral("order"), order_);
        return obj;
    }

    static MatteNode fromJson(const QJsonObject& obj) {
        MatteNode node;
        node.id_ = MatteNodeId(obj["id"].toString());
        node.sourceLayerId_ = Id(obj["sourceLayerId"].toString());
        node.mode_ = MatteModeUtils::fromString(obj["mode"].toString());
        node.enabled_ = obj["enabled"].toBool(true);
        node.order_ = obj["order"].toInt(0);
        return node;
    }

private:
    MatteNodeId id_;
    Id sourceLayerId_;
    MatteMode mode_;
    bool enabled_;
    int order_;
};

export class MatteStack {
public:
    MatteStack()
        : stackMode_(MatteStackMode::Add)
    {}

    void addNode(const MatteNode& node) {
        nodes_.push_back(node);
    }

    void removeNode(const MatteNodeId& id) {
        nodes_.erase(
            std::remove_if(nodes_.begin(), nodes_.end(),
                [&id](const MatteNode& n) { return n.id() == id; }),
            nodes_.end());
    }

    void clear() { nodes_.clear(); }

    const std::vector<MatteNode>& nodes() const { return nodes_; }

    MatteStackMode stackMode() const { return stackMode_; }
    void setStackMode(MatteStackMode mode) { stackMode_ = mode; }

    bool isEmpty() const {
        for (const auto& n : nodes_) {
            if (n.isEnabled()) return false;
        }
        return true;
    }

    std::vector<Id> sourceLayerIds() const {
        std::vector<Id> ids;
        for (const auto& n : nodes_) {
            if (n.isEnabled() && !n.sourceLayerId().isNil()) {
                ids.push_back(n.sourceLayerId());
            }
        }
        return ids;
    }

    bool hasCycleWithLayer(const Id& layerId) const {
        for (const auto& n : nodes_) {
            if (n.isEnabled() && n.sourceLayerId() == layerId) {
                return true;
            }
        }
        return false;
    }

    QString debugString() const {
        QString result = QStringLiteral("MatteStack(mode=%1, nodes=[")
            .arg(matteStackModeToString(stackMode_));
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (i > 0) result += ", ";
            result += nodes_[i].debugString();
        }
        result += "])";
        return result;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert(QStringLiteral("stackMode"), matteStackModeToString(stackMode_));
        QJsonArray arr = QJsonArray();
        for (const auto& n : nodes_) {
            arr.append(n.toJson());
        }
        obj.insert(QStringLiteral("nodes"), arr);
        return obj;
    }

    static MatteStack fromJson(const QJsonObject& obj) {
        MatteStack stack;
        stack.setStackMode(matteStackModeFromString(obj.value(QStringLiteral("stackMode")).toString()));
        const QJsonArray arr = obj.value(QStringLiteral("nodes")).toArray();
        for (const auto& val : arr) {
            stack.addNode(MatteNode::fromJson(val.toObject()));
        }
        return stack;
    }

private:
    std::vector<MatteNode> nodes_;
    MatteStackMode stackMode_;
};

/**
 * @brief matte 評価の結果（アルファマスク）
 */
struct MatteEvaluationResult {
    std::vector<float> alphaMask;
    int width = 0;
    int height = 0;

    bool isValid() const { return width > 0 && height > 0 && !alphaMask.empty(); }
    float sampleAlpha(int x, int y) const {
        if (!isValid() || x < 0 || x >= width || y < 0 || y >= height) {
            return 0.0f;
        }
        return alphaMask[static_cast<size_t>(y) * width + x];
    }
};

/**
 * @brief MatteStack を評価してアルファマスクを生成する
 *
 * 各 matte source の画像を受け取り、mode に従ってアルファチャンネルまたは
 * 輝度チャンネルを抽出し、stackMode で合成する。
 *
 * @param sources matte source layer の画像（matteStack.nodes() と同じ順序）
 * @param stack 評価対象の matte stack
 * @param targetWidth 対象画像の幅
 * @param targetHeight 対象画像の高さ
 */
inline MatteEvaluationResult evaluateMatteStack(
    const std::vector<std::vector<float>>& sources,
    const MatteStack& stack,
    int targetWidth,
    int targetHeight)
{
    MatteEvaluationResult result;
    result.width = targetWidth;
    result.height = targetHeight;
    if (targetWidth <= 0 || targetHeight <= 0) {
        return result;
    }

    const size_t pixelCount = static_cast<size_t>(targetWidth) * targetHeight;
    result.alphaMask.assign(pixelCount, 0.0f);

    const auto& nodes = stack.nodes();
    int sourceIndex = 0;

    for (const auto& node : nodes) {
        if (!node.isEnabled()) {
            continue;
        }
        if (sourceIndex >= static_cast<int>(sources.size())) {
            break;
        }

        const auto& sourceAlpha = sources[sourceIndex];
        ++sourceIndex;

        std::vector<float> matteMask(pixelCount, 0.0f);
        for (size_t i = 0; i < pixelCount; ++i) {
            float v = (i < sourceAlpha.size()) ? sourceAlpha[i] : 0.0f;
            if (MatteModeUtils::isInverted(node.mode())) {
                v = 1.0f - v;
            }
            matteMask[i] = std::clamp(v, 0.0f, 1.0f);
        }

        switch (stack.stackMode()) {
        case MatteStackMode::Add:
            for (size_t i = 0; i < pixelCount; ++i) {
                result.alphaMask[i] = std::min(1.0f, result.alphaMask[i] + matteMask[i]);
            }
            break;
        case MatteStackMode::Common:
            for (size_t i = 0; i < pixelCount; ++i) {
                result.alphaMask[i] = std::min(result.alphaMask[i], matteMask[i]);
            }
            break;
        case MatteStackMode::Subtract:
            for (size_t i = 0; i < pixelCount; ++i) {
                result.alphaMask[i] = std::max(0.0f, result.alphaMask[i] - matteMask[i]);
            }
            break;
        }
    }

    return result;
}

} // namespace ArtifactCore
