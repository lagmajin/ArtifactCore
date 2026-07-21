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

import Core.ArtifactString;

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
    ArtifactScriptMethodBodyPtr body;  // compiled method body
};


// ─── AST Node Types ───

enum class ArtifactScriptBinaryOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Gt, Le, Ge,
    And, Or
};

enum class ArtifactScriptUnaryOp {
    Neg, Not
};

struct ArtifactScriptExpr;
using ArtifactScriptExprPtr = std::unique_ptr<ArtifactScriptExpr>;

struct ArtifactScriptExpr {
    enum class Kind { Literal, Variable, Binary, Unary, Call, FieldAccess };
    Kind kind = Kind::Literal;

    // Literal
    ArtifactScriptValue literalValue;

    // Variable / FieldAccess
    std::string variableName;

    // Binary
    ArtifactScriptBinaryOp binaryOp;
    ArtifactScriptExprPtr left;
    ArtifactScriptExprPtr right;

    // Unary
    ArtifactScriptUnaryOp unaryOp;
    ArtifactScriptExprPtr operand;

    // Call
    std::string callName;
    std::vector<ArtifactScriptExprPtr> callArgs;
};

struct ArtifactScriptStmt;
using ArtifactScriptStmtPtr = std::unique_ptr<ArtifactScriptStmt>;

struct ArtifactScriptStmt {
    enum class Kind { Expr, Assign, If, Return, Block, Decl, While };
    Kind kind = Kind::Expr;

    // Expr
    ArtifactScriptExprPtr expr;

    // Assign
    std::string assignTarget;
    ArtifactScriptExprPtr assignValue;

    // If
    ArtifactScriptExprPtr ifCond;
    ArtifactScriptStmtPtr ifThen;
    ArtifactScriptStmtPtr ifElse;

    // Block
    std::vector<ArtifactScriptStmtPtr> blockStmts;

    // Decl: variable declaration ("float x = expr")
    std::string declName;
    ArtifactScriptValueType declType = ArtifactScriptValueType::Float;
    ArtifactScriptExprPtr declInit;

    // While: while loop
    ArtifactScriptExprPtr whileCond;
    ArtifactScriptStmtPtr whileBody;
};

// Per-method compiled body
struct ArtifactScriptMethodBody {
    std::vector<ArtifactScriptStmtPtr> statements;
    std::vector<std::string> parameters;
};

// Make ArtifactScriptMethod hold a body
using ArtifactScriptMethodBodyPtr = std::unique_ptr<ArtifactScriptMethodBody>;

// Extend ArtifactScriptMethod with compiled body
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
    void setScriptClass(const ZeroString& className);
    void setScriptClass(std::string className);
    void setScriptClass(std::string_view className);
    void setScriptClass(const char* className);
    const ZeroString& scriptClassZero() const;
    const std::string& scriptClass() const;

    ArtifactScriptSerializedFields& publicFields();
    const ArtifactScriptSerializedFields& publicFields() const;
    void applyDefaults(const ArtifactScriptDefinition& definition);

private:
    ZeroString scriptClass_;
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

// ─── Evaluator ───

class ArtifactScriptEvaluator {
public:
    ArtifactScriptEvaluator();
    bool execute(const ArtifactScriptMethodBody& body,
                 const std::vector<ArtifactScriptValue>& args,
                 ArtifactScriptSerializedFields& fields);
    std::string getLastError() const;
    bool hasError() const;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ─── Hot Reload ───

struct ArtifactScriptReloadResult {
    bool success = false;
    std::string errorMessage;
    ArtifactScriptDefinition definition;
    ArtifactScriptSerializedFields migratedFields;
};

class ArtifactScriptHotReload {
public:
    ArtifactScriptHotReload();
    ArtifactScriptReloadResult reload(std::string_view newSource,
                                       const ArtifactScriptDefinition* previousDef,
                                       const ArtifactScriptSerializedFields* previousFields);
    bool watchFile(const std::string& path);
    void unwatchFile(const std::string& path);
    std::vector<std::string> pollChanges();
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
