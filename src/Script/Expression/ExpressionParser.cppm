module;
#include <cctype>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

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
module Script.Expression.Parser;

import Script.Expression.Value;

namespace ArtifactCore {

class ExprNode::Impl {
public:
    ExprNodeType type_;
    
    // Node data (union-like storage)
    double numberValue_ = 0.0;
    std::string stringValue_;
    std::vector<std::shared_ptr<ExprNode>> children_;
    std::string operatorSymbol_;
};

ExprNode::ExprNode(ExprNodeType type) : impl_(new Impl()) {
    impl_->type_ = type;
}

ExprNode::~ExprNode() {
    delete impl_;
}

ExprNodeType ExprNode::type() const {
    return impl_->type_;
}

void ExprNode::setType(ExprNodeType type) {
    impl_->type_ = type;
}

std::size_t ExprNode::childCount() const {
    return impl_->children_.size();
}

std::shared_ptr<ExprNode> ExprNode::child(std::size_t index) const {
    if (index < impl_->children_.size()) return impl_->children_[index];
    return nullptr;
}

std::string ExprNode::operatorSymbol() const {
    return impl_->operatorSymbol_;
}

double ExprNode::numberValue() const {
    return impl_->numberValue_;
}

std::string ExprNode::stringValue() const {
    return impl_->stringValue_;
}

void ExprNode::setChildren(const std::vector<std::shared_ptr<ExprNode>>& children) {
    impl_->children_ = children;
}

void ExprNode::setOperatorSymbol(const std::string& op) {
    impl_->operatorSymbol_ = op;
}

void ExprNode::setNumberValue(double v) {
    impl_->numberValue_ = v;
}

void ExprNode::setStringValue(const std::string& s) {
    impl_->stringValue_ = s;
}

// Token types for lexer
enum class TokenType {
    Number,
    String,
    Identifier,
    Plus, Minus, Star, Slash, DoubleStar, DoubleSlash,  // **, // for Python
    LParen, RParen,
    LBracket, RBracket,
    Comma,
    Question, Colon,  // Ternary operator
    Equal,
    EqualEqual, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    And, Or, Not,  // Logical operators (symbols or keywords)
    If, Then, Else, ElsIf, End, Unless,  // Keywords for conditional
    EndOfFile,
    Unknown
};

struct Token {
    TokenType type;
    std::string value;
    size_t position;
};

class ExpressionParser::Impl {
public:
    std::string expression_;
    std::vector<Token> tokens_;
    size_t currentToken_ = 0;
    std::string error_;
    size_t errorPosition_ = std::string::npos;
    size_t errorLength_ = 0;
    ExpressionLanguageStyle languageStyle_ = ExpressionLanguageStyle::Flexible;

    void tokenize();
    Token currentToken();
    Token advance();
    bool match(TokenType type);
    bool isKeyword(const std::string& str, TokenType& outType);
    void setError(const std::string& message, size_t position, size_t length = 1);
    
    std::shared_ptr<ExprNode> parseExpression();
    std::shared_ptr<ExprNode> parseTernary();
    std::shared_ptr<ExprNode> parsePythonIfElse();  // Python: a if cond else b
    std::shared_ptr<ExprNode> parseLogicalOr();
    std::shared_ptr<ExprNode> parseLogicalAnd();
    std::shared_ptr<ExprNode> parseComparison();
    std::shared_ptr<ExprNode> parseAddSub();
    std::shared_ptr<ExprNode> parseMulDiv();
    std::shared_ptr<ExprNode> parsePower();  // Python: **
    std::shared_ptr<ExprNode> parseUnary();
    std::shared_ptr<ExprNode> parsePrimary();
    std::shared_ptr<ExprNode> parseArrayLiteral();
    std::shared_ptr<ExprNode> parseFunctionCall(const std::string& funcName);
    std::shared_ptr<ExprNode> parseCallExpression(std::shared_ptr<ExprNode> callee, bool methodCall);
    std::shared_ptr<ExprNode> parsePostfix(std::shared_ptr<ExprNode> base);
};

void ExpressionParser::Impl::setError(const std::string& message,
                                      size_t position, size_t length) {
    error_ = message;
    errorPosition_ = position;
    errorLength_ = std::max<std::size_t>(1, length);
}

void ExpressionParser::Impl::tokenize() {
    tokens_.clear();
    size_t pos = 0;
    
    while (pos < expression_.size()) {
        char c = expression_[pos];
        
        // Skip whitespace
        if (std::isspace(c)) {
            ++pos;
            continue;
        }
        
        // Numbers
        if (std::isdigit(c) || c == '.') {
            size_t start = pos;
            while (pos < expression_.size() && (std::isdigit(expression_[pos]) || expression_[pos] == '.')) {
                ++pos;
            }
            tokens_.push_back({TokenType::Number, expression_.substr(start, pos - start), start});
            continue;
        }
        
        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            size_t start = pos;
            while (pos < expression_.size() && (std::isalnum(expression_[pos]) || expression_[pos] == '_')) {
                ++pos;
            }
            std::string word = expression_.substr(start, pos - start);
            TokenType keywordType;
            if (isKeyword(word, keywordType)) {
                tokens_.push_back({keywordType, word, start});
            } else {
                tokens_.push_back({TokenType::Identifier, word, start});
            }
            continue;
        }
        
        // String literals
        if (c == '"' || c == '\'') {
            char quote = c;
            ++pos;
            size_t start = pos;
            while (pos < expression_.size() && expression_[pos] != quote) {
                ++pos;
            }
            tokens_.push_back({TokenType::String, expression_.substr(start, pos - start), start});
            if (pos < expression_.size()) ++pos;  // Skip closing quote
            continue;
        }
        
        // Operators and punctuation
        switch (c) {
        case '+': tokens_.push_back({TokenType::Plus, "+", pos}); break;
        case '-': tokens_.push_back({TokenType::Minus, "-", pos}); break;
        case '*':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '*') {
                tokens_.push_back({TokenType::DoubleStar, "**", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Star, "*", pos});
            }
            break;
        case '/':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '/') {
                tokens_.push_back({TokenType::DoubleSlash, "//", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Slash, "/", pos});
            }
            break;
        case '(': tokens_.push_back({TokenType::LParen, "(", pos}); break;
        case ')': tokens_.push_back({TokenType::RParen, ")", pos}); break;
        case '[': tokens_.push_back({TokenType::LBracket, "[", pos}); break;
        case ']': tokens_.push_back({TokenType::RBracket, "]", pos}); break;
        case '.': tokens_.push_back({TokenType::Unknown, ".", pos}); break;
        case ',': tokens_.push_back({TokenType::Comma, ",", pos}); break;
        case '?': tokens_.push_back({TokenType::Question, "?", pos}); break;
        case ':': tokens_.push_back({TokenType::Colon, ":", pos}); break;
        case '=':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                tokens_.push_back({TokenType::EqualEqual, "==", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Equal, "=", pos});
            }
            break;
        case '<':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                tokens_.push_back({TokenType::LessEqual, "<=", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Less, "<", pos});
            }
            break;
        case '>':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                tokens_.push_back({TokenType::GreaterEqual, ">=", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Greater, ">", pos});
            }
            break;
        case '!':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                tokens_.push_back({TokenType::NotEqual, "!=", pos});
                ++pos;
            } else {
                tokens_.push_back({TokenType::Not, "!", pos});
            }
            break;
        default:
            tokens_.push_back({TokenType::Unknown, std::string(1, c), pos});
        }
        ++pos;
    }
    
    tokens_.push_back({TokenType::EndOfFile, "", pos});
}

bool ExpressionParser::Impl::isKeyword(const std::string& str, TokenType& outType) {
    // Common keywords across styles (Flexible mode supports all)
    static const std::map<std::string, TokenType> keywords = {
        // Python/English style
        {"and", TokenType::And},
        {"or", TokenType::Or},
        {"not", TokenType::Not},
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"elif", TokenType::ElsIf},
        // Ruby style
        {"then", TokenType::Then},
        {"elsif", TokenType::ElsIf},
        {"end", TokenType::End},
        {"unless", TokenType::Unless},
        // VB style (case-insensitive handled below)
        {"And", TokenType::And},
        {"Or", TokenType::Or},
        {"Not", TokenType::Not}
    };
    
    auto it = keywords.find(str);
    if (it != keywords.end()) {
        outType = it->second;
        return true;
    }
    
    // Case-insensitive check for VB-style
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    it = keywords.find(lower);
    if (it != keywords.end()) {
        outType = it->second;
        return true;
    }
    
    return false;
}

Token ExpressionParser::Impl::currentToken() {
    if (currentToken_ < tokens_.size()) {
        return tokens_[currentToken_];
    }
    return {TokenType::EndOfFile, "", 0};
}

Token ExpressionParser::Impl::advance() {
    if (currentToken_ < tokens_.size()) {
        return tokens_[currentToken_++];
    }
    return {TokenType::EndOfFile, "", 0};
}

bool ExpressionParser::Impl::match(TokenType type) {
    if (currentToken().type == type) {
        advance();
        return true;
    }
    return false;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseExpression() {
    // Try Python-style "a if condition else b" first in flexible mode
    auto expr = parsePythonIfElse();
    if (expr) return expr;
    
    // Otherwise try JS-style ternary
    return parseTernary();
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parsePythonIfElse() {
    // Save position in case this isn't a Python if-else
    size_t savedPos = currentToken_;
    
    // Parse value expression
    auto valueExpr = parseTernary();
    
    // Check for "if" keyword
    if (match(TokenType::If)) {
        auto conditionExpr = parseTernary();
        
        // Must have "else"
        if (!match(TokenType::Else)) {
            // Not a valid Python if-else, restore and return
            currentToken_ = savedPos;
            return nullptr;
        }
        
        auto elseExpr = parsePythonIfElse();  // Recursive for chaining
        if (!elseExpr) {
            elseExpr = parseTernary();
        }
        
        // Build conditional node: condition, trueExpr, falseExpr
        auto node = std::make_shared<ExprNode>(ExprNodeType::Conditional);
        node->setChildren({conditionExpr, valueExpr, elseExpr});
        return node;
    }
    
    // Not a Python if-else, restore position
    currentToken_ = savedPos;
    return nullptr;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseTernary() {
    auto expr = parseLogicalOr();
    
    // JS-style: condition ? true_val : false_val
    if (match(TokenType::Question)) {
        auto trueExpr = parseExpression();
        if (!match(TokenType::Colon)) {
            setError("Expected ':' in ternary operator", currentToken().position);
            return nullptr;
        }
        auto falseExpr = parseExpression();
        auto node = std::make_shared<ExprNode>(ExprNodeType::Conditional);
        node->setChildren({expr, trueExpr, falseExpr});
        return node;
    }
    
    return expr;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseLogicalOr() {
    auto left = parseLogicalAnd();
    
    // Support both || and "or"
    while (match(TokenType::Or) || 
           (currentToken().type == TokenType::Identifier && currentToken().value == "or" && (advance(), true))) {
        auto right = parseLogicalAnd();
        auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("||");
        node->setChildren({left, right});
        left = node;
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseLogicalAnd() {
    auto left = parseComparison();
    
    // Support both && and "and"
    while (match(TokenType::And) || 
           (currentToken().type == TokenType::Identifier && currentToken().value == "and" && (advance(), true))) {
        auto right = parseComparison();
        auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("&&");
        node->setChildren({left, right});
        left = node;
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseComparison() {
    auto left = parseAddSub();
    
    while (true) {
        Token op = currentToken();
        if (op.type == TokenType::EqualEqual || op.type == TokenType::NotEqual ||
            op.type == TokenType::Less || op.type == TokenType::LessEqual ||
            op.type == TokenType::Greater || op.type == TokenType::GreaterEqual) {
            advance();
            auto right = parseAddSub();
            auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
            left = node;
        } else {
            break;
        }
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseAddSub() {
    auto left = parseMulDiv();
    
    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = tokens_[currentToken_ - 1];
        auto right = parseMulDiv();
            auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
        left = node;
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseMulDiv() {
    auto left = parsePower();  // Parse power first (higher precedence)
    
    while (match(TokenType::Star) || match(TokenType::Slash) || match(TokenType::DoubleSlash)) {
        Token op = tokens_[currentToken_ - 1];
        auto right = parsePower();
            auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
        left = node;
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parsePower() {
    auto left = parseUnary();
    
    // Right-associative: 2**3**2 = 2**(3**2) = 512
    if (match(TokenType::DoubleStar)) {
        auto right = parsePower();  // Recursive for right-associativity
        auto node = std::make_shared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("**");
        node->setChildren({left, right});
        return node;
    }
    
    return left;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseUnary() {
    if (match(TokenType::Minus) || match(TokenType::Not)) {
        Token op = tokens_[currentToken_ - 1];
        auto expr = parseUnary();
            auto node = std::make_shared<ExprNode>(ExprNodeType::UnaryOp);
        node->setOperatorSymbol(op.value);
        node->setChildren({expr});
        return node;
    }
    
    return parsePrimary();
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parsePrimary() {
    std::shared_ptr<ExprNode> base;

    // Number
    if (match(TokenType::Number)) {
        base = std::make_shared<ExprNode>(ExprNodeType::Number);
        base->setNumberValue(std::stod(tokens_[currentToken_ - 1].value));
        return parsePostfix(base);
    }
    
    // String
    if (match(TokenType::String)) {
        base = std::make_shared<ExprNode>(ExprNodeType::String);
        base->setStringValue(tokens_[currentToken_ - 1].value);
        return parsePostfix(base);
    }
    
    // Array literal [1, 2, 3]
    if (match(TokenType::LBracket)) {
        base = parseArrayLiteral();
        return parsePostfix(base);
    }
    
    // Parenthesized expression or vector literal
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        if (!expr) {
            return nullptr;
        }

        // Vector literal: (x, y, z)
        if (match(TokenType::Comma)) {
            std::vector<std::shared_ptr<ExprNode>> elements = {expr};
            do {
                auto element = parseExpression();
                if (!element) {
                    return nullptr;
                }
                elements.push_back(element);
            } while (match(TokenType::Comma));

            if (!match(TokenType::RParen)) {
                setError("Expected ')' after vector literal", currentToken().position);
                return nullptr;
            }

            auto node = std::make_shared<ExprNode>(ExprNodeType::Vector);
            node->setChildren(elements);
            return parsePostfix(node);
        }
        
        if (!match(TokenType::RParen)) {
            setError("Expected ')' after expression", currentToken().position);
            return nullptr;
        }

        return parsePostfix(expr);
    }
    
    // Identifier (variable or function call)
    if (match(TokenType::Identifier)) {
        std::string name = tokens_[currentToken_ - 1].value;
        
        // Function call
        if (match(TokenType::LParen)) {
            base = parseFunctionCall(name);
            return parsePostfix(base);
        }
        
        // Array/vector access: variable[index]
        if (match(TokenType::LBracket)) {
            auto index = parseExpression();
            if (!match(TokenType::RBracket)) {
                setError("Expected ']' after array index", currentToken().position);
                return nullptr;
            }
            auto varNode = std::make_shared<ExprNode>(ExprNodeType::Variable);
            varNode->setStringValue(name);
            auto node = std::make_shared<ExprNode>(ExprNodeType::ArrayAccess);
            node->setChildren({varNode, index});
            return parsePostfix(node);
        }
        
        // Variable
        base = std::make_shared<ExprNode>(ExprNodeType::Variable);
        base->setStringValue(name);
        return parsePostfix(base);
    }
    
    setError("Unexpected token in expression", currentToken().position);
    return nullptr;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parsePostfix(std::shared_ptr<ExprNode> base) {
    while (true) {
        if (currentToken().type == TokenType::Unknown && currentToken().value == ".") {
            advance();
            if (!match(TokenType::Identifier)) {
                setError("Expected property name after '.'", currentToken().position);
                return nullptr;
            }
            auto node = std::make_shared<ExprNode>(ExprNodeType::PropertyAccess);
            node->setStringValue(tokens_[currentToken_ - 1].value);
            node->setChildren({base});
            base = node;
            continue;
        }

        if (match(TokenType::LParen)) {
            if (!base) {
                setError("Unexpected function call target", currentToken().position);
                return nullptr;
            }
            const bool methodCall = base->type() == ExprNodeType::PropertyAccess;
            if (!methodCall && base->type() != ExprNodeType::Variable) {
                setError("Unexpected function call target", currentToken().position);
                return nullptr;
            }
            base = parseCallExpression(base, methodCall);
            if (!base) {
                return nullptr;
            }
            continue;
        }

        if (match(TokenType::LBracket)) {
            auto index = parseExpression();
            if (!match(TokenType::RBracket)) {
                setError("Expected ']' after array index", currentToken().position);
                return nullptr;
            }
            auto node = std::make_shared<ExprNode>(ExprNodeType::ArrayAccess);
            node->setChildren({base, index});
            base = node;
            continue;
        }

        break;
    }

    return base;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseArrayLiteral() {
    std::vector<std::shared_ptr<ExprNode>> elements;
    
    if (!match(TokenType::RBracket)) {
        elements.push_back(parseExpression());
        while (match(TokenType::Comma)) {
            elements.push_back(parseExpression());
        }
        if (!match(TokenType::RBracket)) {
            setError("Expected ']' after array elements", currentToken().position);
            return nullptr;
        }
    }
    
    auto node = std::make_shared<ExprNode>(ExprNodeType::ArrayLiteral);
    node->setChildren(elements);
    return node;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseFunctionCall(const std::string& funcName) {
    std::vector<std::shared_ptr<ExprNode>> args;
    
    if (!match(TokenType::RParen)) {
        args.push_back(parseExpression());
        while (match(TokenType::Comma)) {
            args.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            setError("Expected ')' after function arguments", currentToken().position);
            return nullptr;
        }
    }
    
    auto node = std::make_shared<ExprNode>(ExprNodeType::FunctionCall);
    node->setStringValue(funcName);
    node->setChildren(args);
    return node;
}

std::shared_ptr<ExprNode> ExpressionParser::Impl::parseCallExpression(std::shared_ptr<ExprNode> callee, bool methodCall) {
    std::vector<std::shared_ptr<ExprNode>> args;

    if (!match(TokenType::RParen)) {
        auto firstArg = parseExpression();
        if (!firstArg) {
            return nullptr;
        }
        args.push_back(firstArg);
        while (match(TokenType::Comma)) {
            auto arg = parseExpression();
            if (!arg) {
                return nullptr;
            }
            args.push_back(arg);
        }
        if (!match(TokenType::RParen)) {
            setError("Expected ')' after function arguments", currentToken().position);
            return nullptr;
        }
    }

    if (methodCall) {
        auto node = std::make_shared<ExprNode>(ExprNodeType::MethodCall);
        node->setStringValue(callee ? callee->stringValue() : std::string());
        std::vector<std::shared_ptr<ExprNode>> children;
        children.push_back(callee);
        children.insert(children.end(), args.begin(), args.end());
        node->setChildren(children);
        return node;
    }

    auto node = std::make_shared<ExprNode>(ExprNodeType::FunctionCall);
    node->setStringValue(callee ? callee->stringValue() : std::string());
    node->setChildren(args);
    return node;
}

ExpressionParser::ExpressionParser() : impl_(new Impl()) {}

ExpressionParser::~ExpressionParser() {
    delete impl_;
}

void ExpressionParser::setLanguageStyle(ExpressionLanguageStyle style) {
    impl_->languageStyle_ = style;
}

ExpressionLanguageStyle ExpressionParser::languageStyle() const {
    return impl_->languageStyle_;
}

std::shared_ptr<ExprNode> ExpressionParser::parse(const std::string& expression) {
    impl_->expression_ = expression;
    impl_->currentToken_ = 0;
    impl_->error_.clear();
    impl_->errorPosition_ = std::string::npos;
    impl_->errorLength_ = 0;
    
    impl_->tokenize();
    auto result = impl_->parseExpression();
    if (!impl_->error_.empty()) {
        return nullptr;
    }
    if (result && impl_->currentToken().type != TokenType::EndOfFile) {
        impl_->setError("Unexpected token after expression", impl_->currentToken().position);
        return nullptr;
    }
    return result;
}

std::string ExpressionParser::getError() const {
    return impl_->error_;
}

std::size_t ExpressionParser::getErrorPosition() const {
    return impl_->errorPosition_;
}

std::size_t ExpressionParser::getErrorLength() const {
    return impl_->errorLength_;
}

bool ExpressionParser::hasError() const {
    return !impl_->error_.empty();
}

}
