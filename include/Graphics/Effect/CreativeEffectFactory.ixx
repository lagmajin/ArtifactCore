module;
#include <string>
#include <map>
#include <memory>
#include <functional>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Factory;

import Graphics.Effect.Creative;

export namespace ArtifactCore {

/**
 * @brief 文字列名からクリエイティブエフェクトを生成するファクトリ
 */
class LIBRARY_DLL_API CreativeEffectFactory {
public:
    static std::shared_ptr<CreativeEffect> create(const std::string& name);
    static std::vector<std::string> getAvailableEffects();
};

inline std::shared_ptr<CreativeEffect> CreativeEffectFactory::create(const std::string& name) {
    (void)name;
    return nullptr;
}

inline std::vector<std::string> CreativeEffectFactory::getAvailableEffects() {
    return {};
}

} // namespace ArtifactCore
