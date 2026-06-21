module;
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <utility>

export module Script.ArtifactScript;

export namespace ArtifactCore {

enum class ArtifactScriptValueType {
    Null,
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    ObjectRef,
    AssetRef
};

enum class ArtifactScriptHook {
    OnCreate,
    OnStart,
    OnEnable,
    OnDisable,
    OnUpdate,
    OnDestroy
};

struct ArtifactScriptVec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct ArtifactScriptVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ArtifactScriptVec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct ArtifactScriptColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

struct ArtifactScriptRef {
    std::string id;
};

using ArtifactScriptValue = std::variant<
    std::monostate,
    bool,
    std::int64_t,
    double,
    std::string,
    ArtifactScriptVec2,
    ArtifactScriptVec3,
    ArtifactScriptVec4,
    ArtifactScriptColor,
    ArtifactScriptRef
>;

struct ArtifactScriptField {
    std::string name;
    bool isPublic = true;
    ArtifactScriptValueType type = ArtifactScriptValueType::Null;
    ArtifactScriptValue defaultValue{};
};

struct ArtifactScriptMethod {
    std::string name;
    std::vector<std::string> parameters;
    bool isLifecycleHook = false;
    ArtifactScriptHook hook = ArtifactScriptHook::OnUpdate;
};

struct ArtifactScriptClass {
    std::string name;
    bool derivesFromBehaviour = false;
    std::vector<ArtifactScriptField> fields;
    std::vector<ArtifactScriptMethod> methods;
};

struct ArtifactScriptDiagnostic {
    std::size_t line = 0;
    std::size_t column = 0;
    std::string message;
};

struct ArtifactScriptDefinition {
    std::string source;
    ArtifactScriptClass rootClass;
    std::vector<ArtifactScriptDiagnostic> diagnostics;
};

class ArtifactScriptParser {
public:
    ArtifactScriptDefinition parse(std::string_view source) const;
};

using ArtifactScriptSerializedFields = std::unordered_map<std::string, ArtifactScriptValue>;

class ArtifactScriptComponent {
public:
    void setScriptClass(std::string className);
    const std::string& scriptClass() const;

    ArtifactScriptSerializedFields& publicFields();
    const ArtifactScriptSerializedFields& publicFields() const;
    void applyDefaults(const ArtifactScriptDefinition& definition);

private:
    std::string scriptClass_;
    ArtifactScriptSerializedFields publicFields_;
};

class ArtifactScriptInstance {
public:
    ArtifactScriptInstance() = default;
    explicit ArtifactScriptInstance(ArtifactScriptDefinition definition);

    const ArtifactScriptDefinition& definition() const;
    ArtifactScriptDefinition& definition();

    void bindComponent(const ArtifactScriptComponent& component);
    const ArtifactScriptComponent* boundComponent() const;
    bool hasMethod(std::string_view name) const;
    bool hasHook(ArtifactScriptHook hook) const;
    bool invokeHook(ArtifactScriptHook hook);
    bool wasHookInvoked(ArtifactScriptHook hook) const;

private:
    ArtifactScriptDefinition definition_;
    const ArtifactScriptComponent* component_ = nullptr;
    std::optional<ArtifactScriptHook> lastInvokedHook_;
};

}
