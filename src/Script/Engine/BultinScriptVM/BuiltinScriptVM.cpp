module;
#include <future>
#include <chrono>

module Script.Engine.BuiltinVM;

import Script.Expression.Value;
import Script.Expression.Evaluator;
import Script.Engine.Context;

namespace ArtifactCore {

 class  BuiltinScriptVM::Impl
 {
 private:

 public:
  ExpressionEvaluator evaluator_;
  
  Impl();
  ~Impl();

 };

 BuiltinScriptVM::Impl::Impl() {}
 BuiltinScriptVM::Impl::~Impl() {}

 BuiltinScriptVM::BuiltinScriptVM() : impl_(new Impl())
 {

 }

 BuiltinScriptVM::~BuiltinScriptVM()
 {
  delete impl_;
 }
 
 ExpressionValue BuiltinScriptVM::evaluate(const std::string& expression) {
  return impl_->evaluator_.evaluate(expression);
 }

ExpressionValue BuiltinScriptVM::evaluate(const std::string& expression, ScriptContext& context, int timeoutMs)
{
    // Get or parse AST from context
    auto ast = context.getOrParseAST(expression);
    if (!ast) return ExpressionValue();

    // Snapshot variables and inject into evaluator
    auto uvars = context.getAllVariables();
    // convert to std::map
    std::map<std::string, ExpressionValue> vars;
    for (const auto& kv : uvars) vars[kv.first] = kv.second;
    // Backup existing evaluator variables
    auto backup = impl_->evaluator_.getVariablesCopy();
    impl_->evaluator_.setVariables(vars);

    ExpressionValue result;
    if (timeoutMs > 0) {
        // Run evaluation in async with timeout
        auto fut = std::async(std::launch::async, [this, ast]() {
            return impl_->evaluator_.evaluateAST(ast);
        });
        if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready) {
            result = fut.get();
        } else {
            // Timeout - request cancel on evaluator if supported
            impl_->evaluator_.requestCancel();
            result = ExpressionValue();
        }
    } else {
        result = impl_->evaluator_.evaluateAST(ast);
    }

    // Restore evaluator variables
    impl_->evaluator_.setVariables(backup);
    impl_->evaluator_.clearCancel();

    return result;
}

ExpressionValue BuiltinScriptVM::evaluateAST(const std::shared_ptr<ExprNode>& ast)
{
    return impl_->evaluator_.evaluateAST(ast);
}
 
 void BuiltinScriptVM::setVariable(const std::string& name, const ExpressionValue& value) {
  impl_->evaluator_.setVariable(name, value);
 }
 
 ExpressionValue BuiltinScriptVM::getVariable(const std::string& name) const {
  return impl_->evaluator_.getVariable(name);
 }
 
 std::string BuiltinScriptVM::getError() const {
  return impl_->evaluator_.getError();
 }
 
 bool BuiltinScriptVM::hasError() const {
  return impl_->evaluator_.hasError();
 }

};
