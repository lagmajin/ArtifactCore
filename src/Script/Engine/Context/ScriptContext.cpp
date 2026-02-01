module;

module Script.Engine.Context;

import std;
import Script.Expression.Parser;
import Script.Expression.Value;

namespace ArtifactCore {

class ScriptContext::Impl {
public:
    std::mutex mutex_;
    std::unordered_map<std::string, ExpressionValue> variables_;
    std::unordered_map<std::string, std::shared_ptr<ExprNode>> astCache_;
    ExpressionParser parser_;

    Impl() {}
    ~Impl() {}
};


ScriptContext::ScriptContext() : impl_(new Impl())
{
}

ScriptContext::~ScriptContext()
{
    delete impl_;
}

void ScriptContext::setVariable(const std::string& name, const ExpressionValue& value)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->variables_[name] = value;
}

ExpressionValue ScriptContext::getVariable(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->variables_.find(name);
    if (it != impl_->variables_.end()) return it->second;
    return ExpressionValue();
}

bool ScriptContext::hasVariable(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->variables_.find(name) != impl_->variables_.end();
}

std::shared_ptr<ExprNode> ScriptContext::getOrParseAST(const std::string& expression)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->astCache_.find(expression);
    if (it != impl_->astCache_.end()) return it->second;
    auto ast = impl_->parser_.parse(expression);
    if (ast) impl_->astCache_[expression] = ast;
    return ast;
}

void ScriptContext::clear()
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->variables_.clear();
    impl_->astCache_.clear();
}

std::unordered_map<std::string, ExpressionValue> ScriptContext::getAllVariables() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->variables_;
}

};
