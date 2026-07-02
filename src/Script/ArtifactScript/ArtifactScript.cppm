module;
#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <string>

module Script.ArtifactScript;

namespace ArtifactCore {

namespace {

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::optional<ArtifactScriptHook> hookFromName(std::string_view name) {
    if (name == "OnCreate") return ArtifactScriptHook::OnCreate;
    if (name == "OnStart") return ArtifactScriptHook::OnStart;
    if (name == "OnEnable") return ArtifactScriptHook::OnEnable;
    if (name == "OnDisable") return ArtifactScriptHook::OnDisable;
    if (name == "OnUpdate") return ArtifactScriptHook::OnUpdate;
    if (name == "OnEvaluate") return ArtifactScriptHook::OnUpdate;
    if (name == "OnDestroy") return ArtifactScriptHook::OnDestroy;
    return std::nullopt;
}

ArtifactScriptValue parseDefaultValue(std::string_view text, ArtifactScriptValueType type) {
    const std::string value = trim(text);
    switch (type) {
    case ArtifactScriptValueType::Bool:
        return value == "true";
    case ArtifactScriptValueType::Int: {
        std::int64_t parsed = 0;
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        std::from_chars(begin, end, parsed);
        return parsed;
    }
    case ArtifactScriptValueType::Float:
        return std::stod(value);
    case ArtifactScriptValueType::String:
        return value.size() >= 2 && value.front() == '"' && value.back() == '"'
            ? value.substr(1, value.size() - 2)
            : value;
    default:
        return std::monostate{};
    }
}

ArtifactScriptValueType parseFieldType(std::string_view typeName) {
    if (typeName == "bool") return ArtifactScriptValueType::Bool;
    if (typeName == "int") return ArtifactScriptValueType::Int;
    if (typeName == "float" || typeName == "double") return ArtifactScriptValueType::Float;
    if (typeName == "string") return ArtifactScriptValueType::String;
    if (typeName == "Vec2") return ArtifactScriptValueType::Vec2;
    if (typeName == "Vec3") return ArtifactScriptValueType::Vec3;
    if (typeName == "Vec4") return ArtifactScriptValueType::Vec4;
    if (typeName == "Color") return ArtifactScriptValueType::Color;
    if (typeName == "ObjectRef") return ArtifactScriptValueType::ObjectRef;
    if (typeName == "AssetRef") return ArtifactScriptValueType::AssetRef;
    return ArtifactScriptValueType::Null;
}

}

ArtifactScriptDefinition ArtifactScriptParser::parse(std::string_view source) const {
    ArtifactScriptDefinition def;
    def.source = std::string(source);

    std::istringstream stream(def.source);
    std::string line;
    std::size_t lineNo = 0;
    bool inClass = false;

    while (std::getline(stream, line)) {
        ++lineNo;
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        if (!inClass) {
            if (starts_with(trimmed, "class ")) {
                const auto colon = trimmed.find(':');
                const auto brace = trimmed.find('{');
                const std::size_t nameEnd = colon == std::string::npos ? brace : colon;
                if (nameEnd == std::string::npos) {
                    def.diagnostics.push_back({lineNo, 1, "class declaration is incomplete"});
                    continue;
                }
                def.rootClass.name = trim(trimmed.substr(6, nameEnd - 6));
                def.rootClass.derivesFromBehaviour = trimmed.find("ArtifactBehaviour") != std::string::npos;
                inClass = true;
                continue;
            }
            continue;
        }

        if (trimmed == "}" || trimmed == "};") {
            inClass = false;
            continue;
        }

        const bool isPublic = starts_with(trimmed, "public ");
        const bool isPrivate = starts_with(trimmed, "private ");
        const bool isField = isPublic || isPrivate;
        const bool isMethod = trimmed.find('(') != std::string::npos && trimmed.find(')') != std::string::npos;

        if (isField) {
            const std::string body = trim(trimmed.substr(isPublic ? 7 : 8));
            const auto space = body.find(' ');
            const auto eq = body.find('=');
            if (space == std::string::npos) {
                def.diagnostics.push_back({lineNo, 1, "field declaration is incomplete"});
                continue;
            }
            const std::string typeName = body.substr(0, space);
            const std::string namePart = trim(body.substr(space + 1, eq == std::string::npos ? std::string::npos : eq - space - 1));
            ArtifactScriptField field;
            field.name = namePart;
            field.isPublic = isPublic;
            field.type = parseFieldType(typeName);
            if (eq != std::string::npos) {
                field.defaultValue = parseDefaultValue(body.substr(eq + 1), field.type);
            }
            def.rootClass.fields.push_back(std::move(field));
            continue;
        }

        if (isMethod) {
            ArtifactScriptMethod method;
            const auto paren = trimmed.find('(');
            const std::string before = trim(trimmed.substr(0, paren));
            const auto space = before.find_last_of(' ');
            method.name = space == std::string::npos ? before : trim(before.substr(space + 1));
            method.parameters.clear();
            if (const auto hook = hookFromName(method.name)) {
                method.isLifecycleHook = true;
                method.hook = *hook;
            }
            def.rootClass.methods.push_back(std::move(method));
            continue;
        }
    }

    if (def.rootClass.name.empty()) {
        def.diagnostics.push_back({0, 0, "no class declaration found"});
    }

    return def;
}

void ArtifactScriptComponent::setScriptClass(std::string className) {
    scriptClass_ = std::move(className);
}

const std::string& ArtifactScriptComponent::scriptClass() const {
    return scriptClass_;
}

ArtifactScriptSerializedFields& ArtifactScriptComponent::publicFields() {
    return publicFields_;
}

const ArtifactScriptSerializedFields& ArtifactScriptComponent::publicFields() const {
    return publicFields_;
}

void ArtifactScriptComponent::applyDefaults(const ArtifactScriptDefinition& definition) {
    if (!scriptClass_.empty() && !definition.rootClass.name.empty() && scriptClass_ != definition.rootClass.name) {
        return;
    }

    for (const auto& field : definition.rootClass.fields) {
        if (!field.isPublic) {
            continue;
        }
        if (publicFields_.find(field.name) != publicFields_.end()) {
            continue;
        }
        publicFields_.emplace(field.name, field.defaultValue);
    }
}

ArtifactScriptInstance::ArtifactScriptInstance(ArtifactScriptDefinition definition)
    : definition_(std::move(definition)) {
}

const ArtifactScriptDefinition& ArtifactScriptInstance::definition() const {
    return definition_;
}

ArtifactScriptDefinition& ArtifactScriptInstance::definition() {
    return definition_;
}

void ArtifactScriptInstance::bindComponent(const ArtifactScriptComponent& component) {
    component_ = &component;
}

const ArtifactScriptComponent* ArtifactScriptInstance::boundComponent() const {
    return component_;
}

bool ArtifactScriptInstance::hasMethod(std::string_view name) const {
    const auto& methods = definition_.rootClass.methods;
    return std::any_of(methods.begin(), methods.end(), [&](const ArtifactScriptMethod& method) {
        return method.name == name;
    });
}

bool ArtifactScriptInstance::hasHook(ArtifactScriptHook hook) const {
    const auto& methods = definition_.rootClass.methods;
    return std::any_of(methods.begin(), methods.end(), [&](const ArtifactScriptMethod& method) {
        return method.isLifecycleHook && method.hook == hook;
    });
}

bool ArtifactScriptInstance::invokeHook(ArtifactScriptHook hook) {
    if (!hasHook(hook)) {
        return false;
    }
    lastInvokedHook_ = hook;
    return true;
}

bool ArtifactScriptInstance::wasHookInvoked(ArtifactScriptHook hook) const {
    return lastInvokedHook_.has_value() && *lastInvokedHook_ == hook;
}

}
