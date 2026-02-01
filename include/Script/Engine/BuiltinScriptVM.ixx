module;
export module Script.Engine.BuiltinVM;

import std;
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