module;

#include <cmath>
#include <random>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

module Script.Expression.Evaluator;

import Script.Expression.Value;
import Script.Expression.Parser;
import Math.Noise;

namespace ArtifactCore {

class ExpressionEvaluator::Impl {
public:
  ExpressionEvaluator* owner_;
  std::map<std::string, ExpressionValue> variables_;
  std::map<std::string, BuiltinFunction> functions_;
  ExpressionParser parser_;
  std::string error_;
  std::atomic<bool> cancelRequested_ = false;

  // Audio data for current context
  float audioRMS_ = 0.0f;
  float audioPeak_ = 0.0f;
  float audioLow_ = 0.0f;
  float audioMid_ = 0.0f;
  float audioHigh_ = 0.0f;

  ExpressionValue evaluateNode(const std::shared_ptr<ExprNode> &node);
};

// Helper to merge variables
static std::map<std::string, ExpressionValue>
mergeVariables(const std::map<std::string, ExpressionValue> &a,
               const std::map<std::string, ExpressionValue> &b) {
  auto out = a;
  for (const auto &kv : b)
    out[kv.first] = kv.second;
  return out;
}

static ExpressionValue interpolateValue(const ExpressionValue& from,
                                         const ExpressionValue& to,
                                         double t) {
  if (from.isVector() && to.isVector()) {
    const auto v1 = from.asVector();
    const auto v2 = to.asVector();
    const size_t size = std::min(v1.size(), v2.size());
    std::vector<double> result(size);
    for (size_t i = 0; i < size; ++i) {
      result[i] = v1[i] + t * (v2[i] - v1[i]);
    }
    if (size == 2) return ExpressionValue(result[0], result[1]);
    if (size == 3) return ExpressionValue(result[0], result[1], result[2]);
    if (size == 4) return ExpressionValue(result[0], result[1], result[2], result[3]);
  }

  const double a = from.asNumber();
  const double b = to.asNumber();
  return ExpressionValue(a + t * (b - a));
}

static double easeCurve(double t) {
  t = std::clamp(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

static double easeInCurve(double t) {
  t = std::clamp(t, 0.0, 1.0);
  return t * t;
}

static double easeOutCurve(double t) {
  t = std::clamp(t, 0.0, 1.0);
  return 1.0 - (1.0 - t) * (1.0 - t);
}

ExpressionValue
ExpressionEvaluator::Impl::evaluateNode(const std::shared_ptr<ExprNode> &node) {
  if (!node) {
    error_ = "Null AST node";
    return ExpressionValue();
  }

  // Access impl_ directly since we're in the implementation
  // access node impl via public API
  // Note: we use the ExprNode public accessors to avoid depending on Impl type

  switch (node->type()) {
  case ExprNodeType::Number:
    return ExpressionValue(node->numberValue());

  case ExprNodeType::String:
    return ExpressionValue(node->stringValue());

  case ExprNodeType::Variable: {
    auto name = node->stringValue();
    auto it = variables_.find(name);
    if (it != variables_.end())
      return it->second;
    error_ = "Undefined variable: " + name;
    return ExpressionValue();
  }

  case ExprNodeType::Vector: {
    std::vector<double> components;
    for (size_t i = 0; i < node->childCount(); ++i) {
      auto child = node->child(i);
      auto val = evaluateNode(child);
      components.push_back(val.asNumber());
    }
    if (components.size() == 2)
      return ExpressionValue(components[0], components[1]);
    if (components.size() == 3)
      return ExpressionValue(components[0], components[1], components[2]);
    if (components.size() == 4)
      return ExpressionValue(components[0], components[1], components[2],
                             components[3]);
    return ExpressionValue();
  }

  case ExprNodeType::ArrayLiteral: {
    std::vector<ExpressionValue> elements;
    for (size_t i = 0; i < node->childCount(); ++i) {
      elements.push_back(evaluateNode(node->child(i)));
    }
    return ExpressionValue(elements);
  }

  case ExprNodeType::ArrayAccess: {
    if (node->childCount() < 2) {
      error_ = "Invalid array access";
      return ExpressionValue();
    }
    auto array = evaluateNode(node->child(0));
    auto index = evaluateNode(node->child(1));
    size_t idx = static_cast<size_t>(index.asNumber());
    return array.at(idx);
  }

  case ExprNodeType::PropertyAccess: {
    if (node->childCount() < 1) {
      error_ = "Invalid property access";
      return ExpressionValue();
    }

    auto base = evaluateNode(node->child(0));
    const auto prop = node->stringValue();

    if (base.isObject()) {
      if (base.hasProperty(prop)) {
        return base.property(prop);
      }
      error_ = "Undefined property: " + prop;
      return ExpressionValue();
    }

    if (base.isVector()) {
      if (prop == "x" || prop == "r") return ExpressionValue(base.x());
      if (prop == "y" || prop == "g") return ExpressionValue(base.y());
      if (prop == "z" || prop == "b") return ExpressionValue(base.z());
      if (prop == "w" || prop == "a") return ExpressionValue(base.w());
    }

    if (base.isArray() && prop == "length") {
      return ExpressionValue(static_cast<double>(base.length()));
    }

    if (base.isString() && prop == "length") {
      return ExpressionValue(static_cast<double>(base.asString().size()));
    }

    error_ = "Unsupported property access: " + prop;
    return ExpressionValue();
  }

  case ExprNodeType::BinaryOp: {
    if (node->childCount() < 2) {
      error_ = "Binary operator requires two operands";
      return ExpressionValue();
    }
    auto left = evaluateNode(node->child(0));
    auto right = evaluateNode(node->child(1));
    auto op = node->operatorSymbol();
    if (op == "+")
      return left + right;
    if (op == "-")
      return left - right;
    if (op == "*")
      return left * right;
    if (op == "/")
      return left / right;
    if (op == "**")
      return ExpressionValue(std::pow(left.asNumber(), right.asNumber()));
    if (op == "//") {
      double divisor = right.asNumber();
      if (divisor != 0.0)
        return ExpressionValue(std::floor(left.asNumber() / divisor));
      return ExpressionValue();
    }
    if (op == "==")
      return ExpressionValue(left == right ? 1.0 : 0.0);
    if (op == "!=")
      return ExpressionValue(left != right ? 1.0 : 0.0);
    if (op == "<")
      return ExpressionValue(left < right ? 1.0 : 0.0);
    if (op == "<=")
      return ExpressionValue(left <= right ? 1.0 : 0.0);
    if (op == ">")
      return ExpressionValue(left > right ? 1.0 : 0.0);
    if (op == ">=")
      return ExpressionValue(left >= right ? 1.0 : 0.0);
    if (op == "&&" || op == "and")
      return ExpressionValue(
          (left.asNumber() != 0.0 && right.asNumber() != 0.0) ? 1.0 : 0.0);
    if (op == "||" || op == "or")
      return ExpressionValue(
          (left.asNumber() != 0.0 || right.asNumber() != 0.0) ? 1.0 : 0.0);

    error_ = std::string("Unknown binary operator: ") + op;
    return ExpressionValue();
  }

  case ExprNodeType::UnaryOp: {
    if (node->childCount() == 0) {
      error_ = "Unary operator requires one operand";
      return ExpressionValue();
    }
    auto operand = evaluateNode(node->child(0));
    auto opu = node->operatorSymbol();
    if (opu == "-")
      return ExpressionValue(-operand.asNumber());
    if (opu == "!" || opu == "not")
      return ExpressionValue(operand.asNumber() == 0.0 ? 1.0 : 0.0);

    error_ = std::string("Unknown unary operator: ") + opu;
    return ExpressionValue();
  }

  case ExprNodeType::FunctionCall: {
    auto fname = node->stringValue();
    auto it = functions_.find(fname);
    if (it == functions_.end()) {
      error_ = "Undefined function: " + fname;
      return ExpressionValue();
    }
    std::vector<ExpressionValue> args;
    for (size_t i = 0; i < node->childCount(); ++i)
      args.push_back(evaluateNode(node->child(i)));

    // Pass ExpressionEvaluator pointer to functions
    return it->second(
        args, static_cast<const ExpressionEvaluator *>(this->owner_));
  }

  case ExprNodeType::MethodCall: {
    if (node->childCount() < 1) {
      error_ = "Invalid method call";
      return ExpressionValue();
    }

    auto base = evaluateNode(node->child(0));
    const auto method = node->stringValue();
    std::vector<ExpressionValue> args;
    for (size_t i = 1; i < node->childCount(); ++i) {
      args.push_back(evaluateNode(node->child(i)));
    }

    if (base.isObject() && method == "layer") {
      auto layersValue = base.property("layers");
      if (!layersValue.isArray()) {
        error_ = "thisComp.layer() requires a layers catalog";
        return ExpressionValue();
      }

      if (args.empty()) {
        error_ = "thisComp.layer() requires an argument";
        return ExpressionValue();
      }

      const auto layers = layersValue.asArray();
      if (args[0].isNumber()) {
        const auto index = static_cast<std::size_t>(std::max(0.0, args[0].asNumber()));
        if (index >= 1 && index <= layers.size()) {
          return layers[index - 1];
        }
      }

      const std::string wantedName = args[0].asString();
      for (const auto& layer : layers) {
        if (layer.isObject() && layer.hasProperty("name") && layer.property("name").asString() == wantedName) {
          return layer;
        }
      }

      error_ = "Layer not found: " + wantedName;
      return ExpressionValue();
    }

    error_ = "Unsupported method call: " + method;
    return ExpressionValue();
  }

  case ExprNodeType::Conditional: {
    if (node->childCount() < 3) {
      error_ = "Ternary operator requires three operands";
      return ExpressionValue();
    }
    auto condition = evaluateNode(node->child(0));
    if (condition.asNumber() != 0.0)
      return evaluateNode(node->child(1));
    return evaluateNode(node->child(2));
  }

  default:
    error_ = "Unknown node type";
    return ExpressionValue();
  }
}

ExpressionEvaluator::ExpressionEvaluator() : impl_(new Impl()) {
  impl_->owner_ = this;
  registerStandardFunctions();
}

void ExpressionEvaluator::setAudioData(float rms, float peak, float low,
                                       float mid, float high) {
  impl_->audioRMS_ = rms;
  impl_->audioPeak_ = peak;
  impl_->audioLow_ = low;
  impl_->audioMid_ = mid;
  impl_->audioHigh_ = high;

  // また、便利な変数としても登録（JS風にアクセスできるように）
  setVariable("audio_rms", ExpressionValue(rms));
  setVariable("audio_peak", ExpressionValue(peak));
  setVariable("audio_low", ExpressionValue(low));
  setVariable("audio_mid", ExpressionValue(mid));
  setVariable("audio_high", ExpressionValue(high));
}

ExpressionEvaluator::~ExpressionEvaluator() { delete impl_; }

ExpressionValue ExpressionEvaluator::evaluate(const std::string &expression) {
  impl_->error_.clear();
  auto ast = impl_->parser_.parse(expression);
  if (impl_->parser_.hasError()) {
    impl_->error_ = impl_->parser_.getError();
    return ExpressionValue();
  }
  return evaluateAST(ast);
}

ExpressionValue
ExpressionEvaluator::evaluateAST(const std::shared_ptr<ExprNode> &node) {
  impl_->error_.clear();
  impl_->cancelRequested_ = false;
  return impl_->evaluateNode(node);
}

void ExpressionEvaluator::setVariable(const std::string &name,
                                      const ExpressionValue &value) {
  impl_->variables_[name] = value;
}

std::map<std::string, ExpressionValue>
ExpressionEvaluator::getVariablesCopy() const {
  return impl_->variables_;
}

void ExpressionEvaluator::setVariables(
    const std::map<std::string, ExpressionValue> &vars) {
  impl_->variables_ = vars;
}

void ExpressionEvaluator::requestCancel() { impl_->cancelRequested_ = true; }

void ExpressionEvaluator::clearCancel() { impl_->cancelRequested_ = false; }

ExpressionValue
ExpressionEvaluator::getVariable(const std::string &name) const {
  auto it = impl_->variables_.find(name);
  if (it != impl_->variables_.end()) {
    return it->second;
  }
  return ExpressionValue();
}

bool ExpressionEvaluator::hasVariable(const std::string &name) const {
  return impl_->variables_.find(name) != impl_->variables_.end();
}

void ExpressionEvaluator::clearVariables() { impl_->variables_.clear(); }

void ExpressionEvaluator::registerFunction(const std::string &name,
                                           BuiltinFunction func) {
  impl_->functions_[name] = func;
}

void ExpressionEvaluator::registerStandardFunctions() {
  using namespace BuiltinFunctions;
  registerFunction("sin", Sin);
  registerFunction("cos", Cos);
  registerFunction("tan", Tan);
  registerFunction("degToRad", DegToRad);
  registerFunction("radToDeg", RadToDeg);
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
  registerFunction("distance", Distance);
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
  registerFunction("wiggle", Wiggle);

  // Audio
  registerFunction("audio_rms", AudioRMS);
  registerFunction("audio_peak", AudioPeak);
  registerFunction("audio_low", AudioLow);
  registerFunction("audio_mid", AudioMid);
  registerFunction("audio_high", AudioHigh);
}

std::string ExpressionEvaluator::getError() const { return impl_->error_; }

bool ExpressionEvaluator::hasError() const { return !impl_->error_.empty(); }

namespace BuiltinFunctions {

ExpressionValue Sin(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::sin(args[0].asNumber()));
}

ExpressionValue Cos(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::cos(args[0].asNumber()));
}

ExpressionValue Tan(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::tan(args[0].asNumber()));
}

ExpressionValue DegToRad(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(args[0].asNumber() * (std::acos(-1.0) / 180.0));
}

ExpressionValue RadToDeg(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(args[0].asNumber() * (180.0 / std::acos(-1.0)));
}

ExpressionValue Sqrt(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::sqrt(args[0].asNumber()));
}

ExpressionValue Pow(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() < 2) return ExpressionValue();
  return ExpressionValue(std::pow(args[0].asNumber(), args[1].asNumber()));
}

ExpressionValue Abs(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::abs(args[0].asNumber()));
}

ExpressionValue Floor(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::floor(args[0].asNumber()));
}

ExpressionValue Ceil(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::ceil(args[0].asNumber()));
}

ExpressionValue Round(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return ExpressionValue(std::round(args[0].asNumber()));
}

ExpressionValue Min(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  double m = args[0].asNumber();
  for (size_t i = 1; i < args.size(); ++i) m = std::min(m, args[i].asNumber());
  return ExpressionValue(m);
}

ExpressionValue Max(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  double m = args[0].asNumber();
  for (size_t i = 1; i < args.size(); ++i) m = std::max(m, args[i].asNumber());
  return ExpressionValue(m);
}

ExpressionValue Clamp(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() < 3) return ExpressionValue();
  return ExpressionValue(std::clamp(args[0].asNumber(), args[1].asNumber(), args[2].asNumber()));
}

ExpressionValue Length(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue(0.0);

  const auto& value = args[0];
  if (value.isVector()) {
    const auto vec = value.asVector();
    double sumSq = 0.0;
    for (double component : vec) {
      sumSq += component * component;
    }
    return ExpressionValue(std::sqrt(sumSq));
  }

  if (value.isArray()) {
    return ExpressionValue(static_cast<double>(value.length()));
  }

  return ExpressionValue(std::abs(value.asNumber()));
}

ExpressionValue Distance(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() < 2) return ExpressionValue(0.0);

  const auto a = args[0];
  const auto b = args[1];
  if (a.isVector() && b.isVector()) {
    const auto av = a.asVector();
    const auto bv = b.asVector();
    const size_t size = std::min(av.size(), bv.size());
    double sumSq = 0.0;
    for (size_t i = 0; i < size; ++i) {
      const double delta = av[i] - bv[i];
      sumSq += delta * delta;
    }
    return ExpressionValue(std::sqrt(sumSq));
  }

  return ExpressionValue(std::abs(a.asNumber() - b.asNumber()));
}

ExpressionValue Normalize(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue();
  return args[0].normalized();
}

ExpressionValue Dot(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() < 2) return ExpressionValue(0.0);
  auto v1 = args[0].asVector();
  auto v2 = args[1].asVector();
  double sum = 0.0;
  size_t size = std::min(v1.size(), v2.size());
  for (size_t i = 0; i < size; ++i) sum += v1[i] * v2[i];
  return ExpressionValue(sum);
}

ExpressionValue Cross(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() < 2) return ExpressionValue();
  auto v1 = args[0].asVector();
  auto v2 = args[1].asVector();
  if (v1.size() < 3 || v2.size() < 3) return ExpressionValue();
  return ExpressionValue(v1[1] * v2[2] - v1[2] * v2[1],
                         v1[2] * v2[0] - v1[0] * v2[2],
                         v1[0] * v2[1] - v1[1] * v2[0]);
}

ExpressionValue Linear(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() == 3) {
    const double t = args[0].asNumber();
    return interpolateValue(args[1], args[2], t);
  }

  if (args.size() >= 5) {
    const double t = args[0].asNumber();
    const double tMin = args[1].asNumber();
    const double tMax = args[2].asNumber();
    const double range = tMax - tMin;
    if (std::abs(range) < 1e-12) {
      return args[3];
    }
    const double alpha = std::clamp((t - tMin) / range, 0.0, 1.0);
    return interpolateValue(args[3], args[4], alpha);
  }

  return ExpressionValue();
}

ExpressionValue Ease(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() == 3) {
    const double t = easeCurve(args[0].asNumber());
    return interpolateValue(args[1], args[2], t);
  }

  if (args.size() >= 5) {
    const double t = args[0].asNumber();
    const double tMin = args[1].asNumber();
    const double tMax = args[2].asNumber();
    const double range = tMax - tMin;
    if (std::abs(range) < 1e-12) {
      return args[3];
    }
    const double alpha = easeCurve((t - tMin) / range);
    return interpolateValue(args[3], args[4], alpha);
  }

  return ExpressionValue();
}

ExpressionValue EaseIn(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() == 3) {
    const double t = easeInCurve(args[0].asNumber());
    return interpolateValue(args[1], args[2], t);
  }

  if (args.size() >= 5) {
    const double t = args[0].asNumber();
    const double tMin = args[1].asNumber();
    const double tMax = args[2].asNumber();
    const double range = tMax - tMin;
    if (std::abs(range) < 1e-12) {
      return args[3];
    }
    const double alpha = easeInCurve((t - tMin) / range);
    return interpolateValue(args[3], args[4], alpha);
  }

  return ExpressionValue();
}

ExpressionValue EaseOut(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.size() == 3) {
    const double t = easeOutCurve(args[0].asNumber());
    return interpolateValue(args[1], args[2], t);
  }

  if (args.size() >= 5) {
    const double t = args[0].asNumber();
    const double tMin = args[1].asNumber();
    const double tMax = args[2].asNumber();
    const double range = tMax - tMin;
    if (std::abs(range) < 1e-12) {
      return args[3];
    }
    const double alpha = easeOutCurve((t - tMin) / range);
    return interpolateValue(args[3], args[4], alpha);
  }

  return ExpressionValue();
}

ExpressionValue Random(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  static std::mt19937 gen(std::random_device{}());
  double minV = 0.0, maxV = 1.0;
  if (args.size() == 1) maxV = args[0].asNumber();
  else if (args.size() >= 2) {
    minV = args[0].asNumber();
    maxV = args[1].asNumber();
  }
  std::uniform_real_distribution<double> dis(minV, maxV);
  return ExpressionValue(dis(gen));
}

ExpressionValue Noise(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue(0.0);
  double x = args[0].asNumber();
  double y = args.size() > 1 ? args[1].asNumber() : 0.0;
  double z = args.size() > 2 ? args[2].asNumber() : 0.0;
  float val = NoiseGenerator::perlin(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  return ExpressionValue(static_cast<double>(val));
}

ExpressionValue Wiggle(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *ctx) {
  if (args.size() < 2) return ExpressionValue(0.0);
  double freq = args[0].asNumber();
  double amp = args[1].asNumber();
  double time = ctx ? ctx->getVariable("time").asNumber() : 0.0;
  double noise = NoiseGenerator::perlin(static_cast<float>(time * freq), 0.0f, 0.0f);
  return ExpressionValue(noise * amp);
}

ExpressionValue Sum(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  double total = 0.0;
  for (const auto& arg : args) total += arg.asNumber();
  return ExpressionValue(total);
}

ExpressionValue Average(const std::vector<ExpressionValue> &args, const ExpressionEvaluator *) {
  if (args.empty()) return ExpressionValue(0.0);
  double total = 0.0;
  for (const auto& arg : args) total += arg.asNumber();
  return ExpressionValue(total / args.size());
}

ExpressionValue AudioRMS(const std::vector<ExpressionValue> &, const ExpressionEvaluator *ctx) {
  return ctx ? ctx->getVariable("audio_rms") : ExpressionValue(0.0);
}

ExpressionValue AudioPeak(const std::vector<ExpressionValue> &, const ExpressionEvaluator *ctx) {
  return ctx ? ctx->getVariable("audio_peak") : ExpressionValue(0.0);
}

ExpressionValue AudioLow(const std::vector<ExpressionValue> &, const ExpressionEvaluator *ctx) {
  return ctx ? ctx->getVariable("audio_low") : ExpressionValue(0.0);
}

ExpressionValue AudioMid(const std::vector<ExpressionValue> &, const ExpressionEvaluator *ctx) {
  return ctx ? ctx->getVariable("audio_mid") : ExpressionValue(0.0);
}

ExpressionValue AudioHigh(const std::vector<ExpressionValue> &, const ExpressionEvaluator *ctx) {
  return ctx ? ctx->getVariable("audio_high") : ExpressionValue(0.0);
}

} // namespace BuiltinFunctions

} // namespace ArtifactCore
