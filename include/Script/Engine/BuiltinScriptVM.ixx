module;
export module Script.Engine.BuiltinVM;

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
import Script.Expression.Evaluator;
import Script.Expression.Parser;

// Forward-declare ScriptContext to avoid module cycle
export namespace ArtifactCore { class ScriptContext; }

export namespace ArtifactCore {
 
 class BuiltinScriptVM
 {
 private:
  class Impl;
  Impl* impl_;
	
 public:
  BuiltinScriptVM();
  ~BuiltinScriptVM();
  
  // Evaluate expression
  ExpressionValue evaluate(const std::string& expression);
  // Evaluate a parsed AST directly (caller may use ScriptContext to parse/cache AST)
  ExpressionValue evaluateAST(const std::shared_ptr<ExprNode>& ast);
  
  // Evaluate with ScriptContext and optional timeout (ms). timeoutMs <= 0 means no timeout.
  ExpressionValue evaluate(const std::string& expression, ScriptContext& context, int timeoutMs = 0);
  
  // Variable management
  void setVariable(const std::string& name, const ExpressionValue& value);
  ExpressionValue getVariable(const std::string& name) const;
  
  // Error handling
  std::string getError() const;
  bool hasError() const;
 };

 typedef std::shared_ptr<BuiltinScriptVM> BuiltinScriptVMPtr;
 typedef std::weak_ptr<BuiltinScriptVM>	  BuiltinScriptVMWeakPtr;

};