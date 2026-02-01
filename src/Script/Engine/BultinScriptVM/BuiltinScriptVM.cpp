module ;

module Script.Engine.BuiltinVM;

import Script.Expression.Value;
import Script.Expression.Evaluator;

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
