module;

#include <cmath>
#include <random>

module Script.Expression.Evaluator;

import std;
import Script.Expression.Value;
import Script.Expression.Parser;

namespace ArtifactCore {

class ExpressionEvaluator::Impl {
public:
    std::map<std::string, ExpressionValue> variables_;
    std::map<std::string, BuiltinFunction> functions_;
    ExpressionParser parser_;
    std::string error_;
    
    ExpressionValue evaluateNode(const std::shared_ptr<ExprNode>& node);
};

ExpressionValue ExpressionEvaluator::Impl::evaluateNode(const std::shared_ptr<ExprNode>& node) {
    if (!node) {
        error_ = "Null AST node";
        return ExpressionValue();
    }
    
    // Access impl_ directly since we're in the implementation
    auto* impl = node->impl_;
    
    switch (node->type()) {
    case ExprNodeType::Number:
        return ExpressionValue(impl->numberValue_);
        
    case ExprNodeType::String:
        return ExpressionValue(impl->stringValue_);
        
    case ExprNodeType::Variable: {
        auto it = variables_.find(impl->stringValue_);
        if (it != variables_.end()) {
            return it->second;
        }
        error_ = "Undefined variable: " + impl->stringValue_;
        return ExpressionValue();
    }
    
    case ExprNodeType::Vector: {
        std::vector<double> components;
        for (const auto& child : impl->children_) {
            auto val = evaluateNode(child);
            components.push_back(val.asNumber());
        }
        if (components.size() == 2) return ExpressionValue(components[0], components[1]);
        if (components.size() == 3) return ExpressionValue(components[0], components[1], components[2]);
        if (components.size() == 4) return ExpressionValue(components[0], components[1], components[2], components[3]);
        return ExpressionValue();
    }
    
    case ExprNodeType::ArrayLiteral: {
        std::vector<ExpressionValue> elements;
        for (const auto& child : impl->children_) {
            elements.push_back(evaluateNode(child));
        }
        return ExpressionValue(elements);
    }
    
    case ExprNodeType::ArrayAccess: {
        if (impl->children_.size() < 2) {
            error_ = "Invalid array access";
            return ExpressionValue();
        }
        auto array = evaluateNode(impl->children_[0]);
        auto index = evaluateNode(impl->children_[1]);
        size_t idx = static_cast<size_t>(index.asNumber());
        return array.at(idx);
    }
    
    case ExprNodeType::BinaryOp: {
        if (impl->children_.size() < 2) {
            error_ = "Binary operator requires two operands";
            return ExpressionValue();
        }
        auto left = evaluateNode(impl->children_[0]);
        auto right = evaluateNode(impl->children_[1]);
        
        if (impl->operatorSymbol_ == "+") return left + right;
        if (impl->operatorSymbol_ == "-") return left - right;
        if (impl->operatorSymbol_ == "*") return left * right;
        if (impl->operatorSymbol_ == "/") return left / right;
        if (impl->operatorSymbol_ == "**") {
            // Power operator
            return ExpressionValue(std::pow(left.asNumber(), right.asNumber()));
        }
        if (impl->operatorSymbol_ == "//") {
            // Integer division
            double divisor = right.asNumber();
            if (divisor != 0.0) {
                return ExpressionValue(std::floor(left.asNumber() / divisor));
            }
            return ExpressionValue();
        }
        if (impl->operatorSymbol_ == "==") return ExpressionValue(left == right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == "!=") return ExpressionValue(left != right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == "<") return ExpressionValue(left < right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == "<=") return ExpressionValue(left <= right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == ">") return ExpressionValue(left > right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == ">=") return ExpressionValue(left >= right ? 1.0 : 0.0);
        if (impl->operatorSymbol_ == "&&" || impl->operatorSymbol_ == "and") {
            return ExpressionValue((left.asNumber() != 0.0 && right.asNumber() != 0.0) ? 1.0 : 0.0);
        }
        if (impl->operatorSymbol_ == "||" || impl->operatorSymbol_ == "or") {
            return ExpressionValue((left.asNumber() != 0.0 || right.asNumber() != 0.0) ? 1.0 : 0.0);
        }
        
        error_ = "Unknown binary operator: " + impl->operatorSymbol_;
        return ExpressionValue();
    }
    
    case ExprNodeType::UnaryOp: {
        if (impl->children_.empty()) {
            error_ = "Unary operator requires one operand";
            return ExpressionValue();
        }
        auto operand = evaluateNode(impl->children_[0]);
        
        if (impl->operatorSymbol_ == "-") {
            return ExpressionValue(-operand.asNumber());
        }
        if (impl->operatorSymbol_ == "!" || impl->operatorSymbol_ == "not") {
            return ExpressionValue(operand.asNumber() == 0.0 ? 1.0 : 0.0);
        }
        
        error_ = "Unknown unary operator: " + impl->operatorSymbol_;
        return ExpressionValue();
    }
    
    case ExprNodeType::FunctionCall: {
        auto it = functions_.find(impl->stringValue_);
        if (it == functions_.end()) {
            error_ = "Undefined function: " + impl->stringValue_;
            return ExpressionValue();
        }
        
        std::vector<ExpressionValue> args;
        for (const auto& child : impl->children_) {
            args.push_back(evaluateNode(child));
        }
        
        return it->second(args);
    }
    
    case ExprNodeType::Conditional: {
        if (impl->children_.size() < 3) {
            error_ = "Ternary operator requires three operands";
            return ExpressionValue();
        }
        auto condition = evaluateNode(impl->children_[0]);
        if (condition.asNumber() != 0.0) {
            return evaluateNode(impl->children_[1]);
        } else {
            return evaluateNode(impl->children_[2]);
        }
    }
    
    default:
        error_ = "Unknown node type";
        return ExpressionValue();
    }
}

ExpressionEvaluator::ExpressionEvaluator() : impl_(new Impl()) {
    registerStandardFunctions();
}

ExpressionEvaluator::~ExpressionEvaluator() {
    delete impl_;
}

ExpressionValue ExpressionEvaluator::evaluate(const std::string& expression) {
    impl_->error_.clear();
    auto ast = impl_->parser_.parse(expression);
    if (impl_->parser_.hasError()) {
        impl_->error_ = impl_->parser_.getError();
        return ExpressionValue();
    }
    return evaluateAST(ast);
}

ExpressionValue ExpressionEvaluator::evaluateAST(const std::shared_ptr<ExprNode>& node) {
    return impl_->evaluateNode(node);
}

void ExpressionEvaluator::setVariable(const std::string& name, const ExpressionValue& value) {
    impl_->variables_[name] = value;
}

ExpressionValue ExpressionEvaluator::getVariable(const std::string& name) const {
    auto it = impl_->variables_.find(name);
    if (it != impl_->variables_.end()) {
        return it->second;
    }
    return ExpressionValue();
}

bool ExpressionEvaluator::hasVariable(const std::string& name) const {
    return impl_->variables_.find(name) != impl_->variables_.end();
}

void ExpressionEvaluator::clearVariables() {
    impl_->variables_.clear();
}

void ExpressionEvaluator::registerFunction(const std::string& name, BuiltinFunction func) {
    impl_->functions_[name] = func;
}

void ExpressionEvaluator::registerStandardFunctions() {
    using namespace BuiltinFunctions;
    registerFunction("sin", Sin);
    registerFunction("cos", Cos);
    registerFunction("tan", Tan);
    registerFunction("sqrt", Sqrt);
    registerFunction("pow", Pow);
    registerFunction("abs", Abs);
    registerFunction("floor", Floor);
    registerFunction("ceil", Ceil);
    registerFunction("round", Round);
    registerFunction("min", Min);
    registerFunction("max", Max);
    registerFunction("clamp", Clamp);
    registerFunction("length", Length);
    registerFunction("normalize", Normalize);
    registerFunction("dot", Dot);
    registerFunction("cross", Cross);
    registerFunction("linear", Linear);
    registerFunction("ease", Ease);
    registerFunction("easeIn", EaseIn);
    registerFunction("easeOut", EaseOut);
    registerFunction("random", Random);
    registerFunction("noise", Noise);
    registerFunction("sum", Sum);
    registerFunction("average", Average);
}

std::string ExpressionEvaluator::getError() const {
    return impl_->error_;
}

bool ExpressionEvaluator::hasError() const {
    return !impl_->error_.empty();
}

// Built-in functions implementation
namespace BuiltinFunctions {

ExpressionValue Sin(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::sin(args[0].asNumber()));
}

ExpressionValue Cos(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::cos(args[0].asNumber()));
}

ExpressionValue Tan(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::tan(args[0].asNumber()));
}

ExpressionValue Sqrt(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::sqrt(args[0].asNumber()));
}

ExpressionValue Pow(const std::vector<ExpressionValue>& args) {
    if (args.size() < 2) return ExpressionValue();
    return ExpressionValue(std::pow(args[0].asNumber(), args[1].asNumber()));
}

ExpressionValue Abs(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::abs(args[0].asNumber()));
}

ExpressionValue Floor(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::floor(args[0].asNumber()));
}

ExpressionValue Ceil(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::ceil(args[0].asNumber()));
}

ExpressionValue Round(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    return ExpressionValue(std::round(args[0].asNumber()));
}

ExpressionValue Min(const std::vector<ExpressionValue>& args) {
    if (args.size() < 2) return ExpressionValue();
    return ExpressionValue(std::min(args[0].asNumber(), args[1].asNumber()));
}

ExpressionValue Max(const std::vector<ExpressionValue>& args) {
    if (args.size() < 2) return ExpressionValue();
    return ExpressionValue(std::max(args[0].asNumber(), args[1].asNumber()));
}

ExpressionValue Clamp(const std::vector<ExpressionValue>& args) {
    if (args.size() < 3) return ExpressionValue();
    double value = args[0].asNumber();
    double minVal = args[1].asNumber();
    double maxVal = args[2].asNumber();
    return ExpressionValue(std::clamp(value, minVal, maxVal));
}

ExpressionValue Length(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    auto vec = args[0].asVector();
    double sum = 0.0;
    for (double v : vec) {
        sum += v * v;
    }
    return ExpressionValue(std::sqrt(sum));
}

ExpressionValue Normalize(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    auto vec = args[0].asVector();
    double len = 0.0;
    for (double v : vec) {
        len += v * v;
    }
    len = std::sqrt(len);
    if (len == 0.0) return args[0];
    
    std::vector<double> result;
    for (double v : vec) {
        result.push_back(v / len);
    }
    
    if (result.size() == 2) return ExpressionValue(result[0], result[1]);
    if (result.size() == 3) return ExpressionValue(result[0], result[1], result[2]);
    if (result.size() == 4) return ExpressionValue(result[0], result[1], result[2], result[3]);
    return ExpressionValue();
}

ExpressionValue Dot(const std::vector<ExpressionValue>& args) {
    if (args.size() < 2) return ExpressionValue();
    auto v1 = args[0].asVector();
    auto v2 = args[1].asVector();
    double sum = 0.0;
    size_t size = std::min(v1.size(), v2.size());
    for (size_t i = 0; i < size; ++i) {
        sum += v1[i] * v2[i];
    }
    return ExpressionValue(sum);
}

ExpressionValue Cross(const std::vector<ExpressionValue>& args) {
    if (args.size() < 2) return ExpressionValue();
    auto v1 = args[0].asVector();
    auto v2 = args[1].asVector();
    if (v1.size() < 3 || v2.size() < 3) return ExpressionValue();
    
    return ExpressionValue(
        v1[1] * v2[2] - v1[2] * v2[1],
        v1[2] * v2[0] - v1[0] * v2[2],
        v1[0] * v2[1] - v1[1] * v2[0]
    );
}

ExpressionValue Linear(const std::vector<ExpressionValue>& args) {
    if (args.size() < 3) return ExpressionValue();
    double t = args[0].asNumber();
    double a = args[1].asNumber();
    double b = args[2].asNumber();
    return ExpressionValue(a + t * (b - a));
}

ExpressionValue Ease(const std::vector<ExpressionValue>& args) {
    if (args.size() < 3) return ExpressionValue();
    double t = args[0].asNumber();
    double a = args[1].asNumber();
    double b = args[2].asNumber();
    t = t * t * (3.0 - 2.0 * t);  // Smoothstep
    return ExpressionValue(a + t * (b - a));
}

ExpressionValue EaseIn(const std::vector<ExpressionValue>& args) {
    if (args.size() < 3) return ExpressionValue();
    double t = args[0].asNumber();
    double a = args[1].asNumber();
    double b = args[2].asNumber();
    t = t * t;
    return ExpressionValue(a + t * (b - a));
}

ExpressionValue EaseOut(const std::vector<ExpressionValue>& args) {
    if (args.size() < 3) return ExpressionValue();
    double t = args[0].asNumber();
    double a = args[1].asNumber();
    double b = args[2].asNumber();
    t = 1.0 - (1.0 - t) * (1.0 - t);
    return ExpressionValue(a + t * (b - a));
}

ExpressionValue Random(const std::vector<ExpressionValue>& args) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    
    if (args.empty()) {
        return ExpressionValue(dis(gen));
    }
    if (args.size() == 1) {
        return ExpressionValue(dis(gen) * args[0].asNumber());
    }
    if (args.size() >= 2) {
        double min = args[0].asNumber();
        double max = args[1].asNumber();
        return ExpressionValue(min + dis(gen) * (max - min));
    }
    return ExpressionValue();
}

ExpressionValue Noise(const std::vector<ExpressionValue>& args) {
    // Simplified noise (use proper Perlin/Simplex noise in production)
    if (args.empty()) return ExpressionValue();
    double x = args[0].asNumber();
    double y = args.size() > 1 ? args[1].asNumber() : 0.0;
    double z = args.size() > 2 ? args[2].asNumber() : 0.0;
    
    // Simple pseudo-random based on position
    double n = std::sin(x * 12.9898 + y * 78.233 + z * 45.164) * 43758.5453;
    return ExpressionValue(n - std::floor(n));
}

ExpressionValue Sum(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    
    if (args[0].isArray()) {
        auto arr = args[0].asArray();
        double sum = 0.0;
        for (const auto& val : arr) {
            sum += val.asNumber();
        }
        return ExpressionValue(sum);
    }
    
    double sum = 0.0;
    for (const auto& arg : args) {
        sum += arg.asNumber();
    }
    return ExpressionValue(sum);
}

ExpressionValue Average(const std::vector<ExpressionValue>& args) {
    if (args.empty()) return ExpressionValue();
    
    if (args[0].isArray()) {
        auto arr = args[0].asArray();
        if (arr.empty()) return ExpressionValue();
        double sum = 0.0;
        for (const auto& val : arr) {
            sum += val.asNumber();
        }
        return ExpressionValue(sum / arr.size());
    }
    
    double sum = 0.0;
    for (const auto& arg : args) {
        sum += arg.asNumber();
    }
    return ExpressionValue(sum / args.size());
}

}  // namespace BuiltinFunctions

}  // namespace ArtifactCore
