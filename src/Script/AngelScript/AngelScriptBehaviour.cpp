module;
#include <string>

module Script.AngelScript.Behaviour;

namespace ArtifactCore {

bool AngelScriptBehaviour::load(const std::string& moduleName, const std::string& source) {
    auto& engine = AngelScriptEngine::instance();
    if (!engine.isInitialized() && !engine.initialize()) {
        loaded_ = false;
        return false;
    }
    if (!engine.compileModule(moduleName, source)) {
        loaded_ = false;
        return false;
    }

    moduleName_ = moduleName;
    started_ = false;
    destroyed_ = false;
    loaded_ = true;
    return true;
}

bool AngelScriptBehaviour::start() {
    if (!loaded_ || started_ || destroyed_) {
        return false;
    }
    auto& engine = AngelScriptEngine::instance();
    if (!engine.runFunction(moduleName_, "void Start()")) {
        return false;
    }
    started_ = true;
    return true;
}

bool AngelScriptBehaviour::update(float deltaTime) {
    if (!loaded_ || destroyed_) {
        return false;
    }
    auto& engine = AngelScriptEngine::instance();
    return engine.runFunction(moduleName_, "void Update(float)");
}

bool AngelScriptBehaviour::destroy() {
    if (!loaded_ || destroyed_) {
        return false;
    }
    auto& engine = AngelScriptEngine::instance();
    const bool ok = engine.runFunction(moduleName_, "void OnDestroy()");
    destroyed_ = true;
    return ok;
}

} // namespace ArtifactCore
