module;

export module Script.Expression.Evaluator;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Script.Expression.Value;
import Script.Expression.Parser;
import Audio.Segment;


export namespace ArtifactCore {

// Built-in function signature
using BuiltinFunction = std::function<ExpressionValue(const std::vector<ExpressionValue>&)>;

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
    ExpressionValue Sin(const std::vector<ExpressionValue>& args);
    ExpressionValue Cos(const std::vector<ExpressionValue>& args);
    ExpressionValue Tan(const std::vector<ExpressionValue>& args);
    ExpressionValue Sqrt(const std::vector<ExpressionValue>& args);
    ExpressionValue Pow(const std::vector<ExpressionValue>& args);
    ExpressionValue Abs(const std::vector<ExpressionValue>& args);
    ExpressionValue Floor(const std::vector<ExpressionValue>& args);
    ExpressionValue Ceil(const std::vector<ExpressionValue>& args);
    ExpressionValue Round(const std::vector<ExpressionValue>& args);
    ExpressionValue Min(const std::vector<ExpressionValue>& args);
    ExpressionValue Max(const std::vector<ExpressionValue>& args);
    ExpressionValue Clamp(const std::vector<ExpressionValue>& args);
    
    // Vector functions
    ExpressionValue Length(const std::vector<ExpressionValue>& args);  // Vector magnitude
    ExpressionValue Normalize(const std::vector<ExpressionValue>& args);
    ExpressionValue Dot(const std::vector<ExpressionValue>& args);
    ExpressionValue Cross(const std::vector<ExpressionValue>& args);
    
    // Interpolation
    ExpressionValue Linear(const std::vector<ExpressionValue>& args);  // lerp
    ExpressionValue Ease(const std::vector<ExpressionValue>& args);
    ExpressionValue EaseIn(const std::vector<ExpressionValue>& args);
    ExpressionValue EaseOut(const std::vector<ExpressionValue>& args);
    
    // Random
    ExpressionValue Random(const std::vector<ExpressionValue>& args);
    ExpressionValue Noise(const std::vector<ExpressionValue>& args);
    
    // Array functions
    ExpressionValue Sum(const std::vector<ExpressionValue>& args);
    ExpressionValue Average(const std::vector<ExpressionValue>& args);
    
    // Audio functions
    ExpressionValue AudioRMS(const std::vector<ExpressionValue>& args);
    ExpressionValue AudioPeak(const std::vector<ExpressionValue>& args);
    ExpressionValue AudioLow(const std::vector<ExpressionValue>& args);
    ExpressionValue AudioMid(const std::vector<ExpressionValue>& args);
    ExpressionValue AudioHigh(const std::vector<ExpressionValue>& args);
}

}
