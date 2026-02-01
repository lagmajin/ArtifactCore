module;
#include "../../../Define/DllExportMacro.hpp"
export module Script.Engine.Context;

import std;
import Script.Expression.Parser;
import Script.Expression.Value;

export namespace ArtifactCore {


 class ScriptContext;

 typedef std::shared_ptr<ScriptContext> ScriptContextPtr;
 typedef std::weak_ptr<ScriptContext> ScriptContextWeakPtr;

 class ScriptContext
 {
 private:
  class Impl;
  Impl* impl_;
  ScriptContext(const ScriptContext&) = delete;
 public:
  ScriptContext();
  ~ScriptContext();

  // Variable table (per-composition)
  void setVariable(const std::string& name, const ExpressionValue& value);
  ExpressionValue getVariable(const std::string& name) const;
  bool hasVariable(const std::string& name) const;

  // AST cache: parse or get cached AST for an expression string
  std::shared_ptr<ExprNode> getOrParseAST(const std::string& expression);
  
  // Clear cache / variables
  void clear();
  
  // Return a copy of all variables (thread-safe)
  std::unordered_map<std::string, ExpressionValue> getAllVariables() const;
 };



};