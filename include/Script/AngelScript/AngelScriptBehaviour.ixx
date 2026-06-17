module;
#include "../../Define/DllExportMacro.hpp"
#include <string>
#include <utility>

export module Script.AngelScript.Behaviour;

import Script.AngelScript.Engine;

export namespace ArtifactCore {

/**
 * @brief Small Unity-style script wrapper for free-function lifecycle hooks.
 *
 * This is the first scaffold for a MonoBehaviour-like flow:
 * - `void Start()`
 * - `void Update(float dt)`
 * - `void OnDestroy()`
 *
 * Scripts are compiled into a named AngelScript module. The wrapper then
 * calls the corresponding free functions by declaration.
 */
class LIBRARY_DLL_API AngelScriptBehaviour {
public:
    AngelScriptBehaviour() = default;

    bool load(const std::string& moduleName, const std::string& source);
    bool start();
    bool update(float deltaTime);
    bool destroy();

    const std::string& moduleName() const { return moduleName_; }
    bool isLoaded() const { return loaded_; }

private:
    std::string moduleName_;
    bool started_ = false;
    bool destroyed_ = false;
    bool loaded_ = false;
};

} // namespace ArtifactCore
