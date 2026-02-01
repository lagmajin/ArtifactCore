module;
export module Script.Engine.BuiltinVM;

import std;
import Script.Expression.Value;
import Script.Expression.Evaluator;

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