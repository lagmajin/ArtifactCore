module;

#include <vector>
#include <memory>
#include <functional>
#include <QVariant>
#include "../Define/DllExportMacro.hpp"

export module Property.LinkManager;

import std;
import Property.Abstract;

export namespace ArtifactCore {

enum class LinkType {
    Direct,      // そのまま同期
    Inverse,     // 逆転 (1.0 - x または -x)
    Scale,       // 倍率指定
    Offset,      // オフセット追加
    Custom       // ユーザー定義関数 (コールバック)
};

struct PropertyLink {
    AbstractProperty* source = nullptr;
    AbstractProperty* target = nullptr;
    LinkType type = LinkType::Direct;
    float multiplier = 1.0f;
    float offset = 0.0f;
    std::function<QVariant(const QVariant&)> customMapper;
};

class LIBRARY_DLL_API PropertyLinkManager {
public:
    static PropertyLinkManager& instance();

    // リンクを追加
    void addLink(AbstractProperty* source, AbstractProperty* target, LinkType type = LinkType::Direct);
    
    // 詳細なリンクを追加
    void addLink(const PropertyLink& link);

    // 指定したソースプロパティが変更されたときにターゲットを更新
    void updateTargets(AbstractProperty* source);

    // すべてのリンクを解除
    void clear();

private:
    PropertyLinkManager() = default;
    std::vector<PropertyLink> links_;
};

} // namespace ArtifactCore
