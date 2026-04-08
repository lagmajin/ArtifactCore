module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Script.Expression.Evaluator;




import Script.Expression.Value;
import Script.Expression.Parser;
import Audio.Segment;


export namespace ArtifactCore {
class ExpressionEvaluator;

// Built-in function signature
using BuiltinFunction = std::function<ExpressionValue(const std::vector<ExpressionValue>&, const ExpressionEvaluator*)>;

// Expression evaluator with context
class ExpressionEvaluator {
private:
    class Impl;
    Impl* impl_;

public:
    ExpressionEvaluator();
    ~ExpressionEvaluator();

    // Evaluate expression string
    ExpressionValue evaluate(const std::string& expression);
    
    // Evaluate parsed AST
    ExpressionValue evaluateAST(const std::shared_ptr<ExprNode>& node);

    // Variable management
    void setVariable(const std::string& name, const ExpressionValue& value);
    ExpressionValue getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    void clearVariables();

    // Register built-in functions
    void registerFunction(const std::string& name, BuiltinFunction func);
    
    // Register standard AE-like built-in functions
    void registerStandardFunctions();

    // Error handling
    std::string getError() const;
    bool hasError() const;

    // Cancellation support
    void requestCancel();
    void clearCancel();

    // Variable snapshot (for temporary injection)
    std::map<std::string, ExpressionValue> getVariablesCopy() const;
    void setVariables(const std::map<std::string, ExpressionValue>& vars);

    // Audio Analysis Context
    void setAudioData(float rms, float peak, float low, float mid, float high);
};

// Standard built-in functions (AE-style)
namespace BuiltinFunctions {
    // Math functions
    ExpressionValue Sin(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Cos(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Tan(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Sqrt(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Pow(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Abs(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Floor(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Ceil(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Round(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Min(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Max(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Clamp(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    
    // Vector functions
    ExpressionValue Length(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);  // Vector magnitude
    ExpressionValue Normalize(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Dot(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Cross(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    
    // Interpolation
    ExpressionValue Linear(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);  // lerp
    ExpressionValue Ease(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue EaseIn(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue EaseOut(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    
    // Random
    ExpressionValue Random(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Noise(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Wiggle(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    
    // Array functions
    ExpressionValue Sum(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue Average(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    
    // Audio functions
    ExpressionValue AudioRMS(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue AudioPeak(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue AudioLow(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue AudioMid(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
    ExpressionValue AudioHigh(const std::vector<ExpressionValue>& args, const ExpressionEvaluator* ctx);
}

}
