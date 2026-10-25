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

import Memory.SharedPtr;
import Container.NamedVector;
import Script.Expression.Value;

namespace ArtifactCore {

class ExprNode::Impl {
public:
    ExprNodeType type_;

    // Node data (union-like storage)
    double numberValue_ = 0.0;
    std::string stringValue_;
    NamedVector<SharedPtr<ExprNode>> children_;
    std::string operatorSymbol_;

    // Half-open source range [startOffset_, endOffset_) in the parsed
    // expression. Both stay 0 for nodes the parser did not build.
    std::size_t startOffset_ = 0;
    std::size_t endOffset_ = 0;
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

SharedPtr<ExprNode> ExprNode::child(std::size_t index) const {
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

void ExprNode::setChildren(const std::vector<SharedPtr<ExprNode>>& children) {
    impl_->children_.clear();
    impl_->children_.reserve(children.size());
    for (const auto& child : children) {
        impl_->children_.push_back(child);
    }
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

void ExprNode::setSourceRange(std::size_t start, std::size_t end) {
    impl_->startOffset_ = start;
    impl_->endOffset_ = end;
}

std::size_t ExprNode::startOffset() const {
    return impl_->startOffset_;
}

std::size_t ExprNode::endOffset() const {
    return impl_->endOffset_;
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
    size_t position;  // inclusive start offset
    size_t end;       // exclusive end offset
};

class ExpressionParser::Impl {
public:
    std::string expression_;
    NamedVector<Token> tokens_;
    size_t currentToken_ = 0;
    std::string error_;
    size_t errorPosition_ = std::string::npos;
    size_t errorLength_ = 0;
    ExpressionLanguageStyle languageStyle_ = ExpressionLanguageStyle::Flexible;

    void tokenize();
    Token currentToken();
    Token advance();
    // End offset of the most recently consumed token, used to span new nodes.
    size_t lastConsumedTokenEnd() const;
    void finishNode(const SharedPtr<ExprNode>& node, size_t start) const;
    bool match(TokenType type);
    bool isKeyword(const std::string& str, TokenType& outType);
    void setError(const std::string& message, size_t position, size_t length = 1);
    
    SharedPtr<ExprNode> parseExpression();
    SharedPtr<ExprNode> parseTernary();
    SharedPtr<ExprNode> parsePythonIfElse();  // Python: a if cond else b
    SharedPtr<ExprNode> parseLogicalOr();
    SharedPtr<ExprNode> parseLogicalAnd();
    SharedPtr<ExprNode> parseComparison();
    SharedPtr<ExprNode> parseAddSub();
    SharedPtr<ExprNode> parseMulDiv();
    SharedPtr<ExprNode> parsePower();  // Python: **
    SharedPtr<ExprNode> parseUnary();
    SharedPtr<ExprNode> parsePrimary();
    SharedPtr<ExprNode> parseArrayLiteral(size_t openBracketOffset);
    SharedPtr<ExprNode> parseFunctionCall(const std::string& funcName);
    SharedPtr<ExprNode> parseCallExpression(SharedPtr<ExprNode> callee, bool methodCall);
    SharedPtr<ExprNode> parsePostfix(SharedPtr<ExprNode> base);
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

    // Records a token together with its half-open source range. `end` is the
    // exclusive offset so a node can be spanned without re-scanning the source.
    const auto emit = [this](TokenType type, std::string value,
                             size_t start, size_t end) {
        tokens_.push_back({type, std::move(value), start, end});
    };

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
            emit(TokenType::Number, expression_.substr(start, pos - start), start, pos);
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
                emit(keywordType, word, start, pos);
            } else {
                emit(TokenType::Identifier, word, start, pos);
            }
            continue;
        }

        // String literals. The token range covers both quotes while `value`
        // stays unquoted, so diagnostics can underline the whole literal.
        if (c == '"' || c == '\'') {
            char quote = c;
            size_t openQuote = pos;
            ++pos;
            size_t start = pos;
            while (pos < expression_.size() && expression_[pos] != quote) {
                ++pos;
            }
            const std::string content = expression_.substr(start, pos - start);
            if (pos < expression_.size()) ++pos;  // Skip closing quote
            // Range covers both quotes so a diagnostic underlines the literal.
            emit(TokenType::String, content, openQuote, pos);
            continue;
        }

        // Operators and punctuation
        switch (c) {
        case '+': emit(TokenType::Plus, "+", pos, pos + 1); break;
        case '-': emit(TokenType::Minus, "-", pos, pos + 1); break;
        case '*':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '*') {
                emit(TokenType::DoubleStar, "**", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Star, "*", pos, pos + 1);
            }
            break;
        case '/':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '/') {
                emit(TokenType::DoubleSlash, "//", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Slash, "/", pos, pos + 1);
            }
            break;
        case '(': emit(TokenType::LParen, "(", pos, pos + 1); break;
        case ')': emit(TokenType::RParen, ")", pos, pos + 1); break;
        case '[': emit(TokenType::LBracket, "[", pos, pos + 1); break;
        case ']': emit(TokenType::RBracket, "]", pos, pos + 1); break;
        case '.': emit(TokenType::Unknown, ".", pos, pos + 1); break;
        case ',': emit(TokenType::Comma, ",", pos, pos + 1); break;
        case '?': emit(TokenType::Question, "?", pos, pos + 1); break;
        case ':': emit(TokenType::Colon, ":", pos, pos + 1); break;
        case '=':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                emit(TokenType::EqualEqual, "==", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Equal, "=", pos, pos + 1);
            }
            break;
        case '<':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                emit(TokenType::LessEqual, "<=", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Less, "<", pos, pos + 1);
            }
            break;
        case '>':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                emit(TokenType::GreaterEqual, ">=", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Greater, ">", pos, pos + 1);
            }
            break;
        case '!':
            if (pos + 1 < expression_.size() && expression_[pos + 1] == '=') {
                emit(TokenType::NotEqual, "!=", pos, pos + 2);
                ++pos;
            } else {
                emit(TokenType::Not, "!", pos, pos + 1);
            }
            break;
        default:
            emit(TokenType::Unknown, std::string(1, c), pos, pos + 1);
        }
        ++pos;
    }

    emit(TokenType::EndOfFile, "", pos, pos);
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
    return {TokenType::EndOfFile, "", 0, 0};
}

Token ExpressionParser::Impl::advance() {
    if (currentToken_ < tokens_.size()) {
        return tokens_[currentToken_++];
    }
    return {TokenType::EndOfFile, "", 0, 0};
}

size_t ExpressionParser::Impl::lastConsumedTokenEnd() const {
    return currentToken_ > 0 ? tokens_[currentToken_ - 1].end : 0;
}

// Spans a freshly built node across the tokens it consumed. `start` is
// supplied by the caller (usually the left operand's own start offset) and
// `end` is taken from the token stream, so a node never depends on mutable
// state that a backtrack could have invalidated.
void ExpressionParser::Impl::finishNode(const SharedPtr<ExprNode>& node,
                                        size_t start) const {
    if (!node) return;
    const size_t end = lastConsumedTokenEnd();
    node->setSourceRange(start, end > start ? end : start);
}

bool ExpressionParser::Impl::match(TokenType type) {
    if (currentToken().type == type) {
        advance();
        return true;
    }
    return false;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseExpression() {
    // Try Python-style "a if condition else b" first in flexible mode
    auto expr = parsePythonIfElse();
    if (expr) return expr;
    
    // Otherwise try JS-style ternary
    return parseTernary();
}

SharedPtr<ExprNode> ExpressionParser::Impl::parsePythonIfElse() {
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
        auto node = makeShared<ExprNode>(ExprNodeType::Conditional);
        node->setChildren({conditionExpr, valueExpr, elseExpr});
        finishNode(node, valueExpr ? valueExpr->startOffset() : 0);
        return node;
    }
    
    // Not a Python if-else, restore position
    currentToken_ = savedPos;
    return nullptr;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseTernary() {
    auto expr = parseLogicalOr();
    
    // JS-style: condition ? true_val : false_val
    if (match(TokenType::Question)) {
        auto trueExpr = parseExpression();
        if (!match(TokenType::Colon)) {
            setError("Expected ':' in ternary operator", currentToken().position);
            return nullptr;
        }
        auto falseExpr = parseExpression();
        auto node = makeShared<ExprNode>(ExprNodeType::Conditional);
        node->setChildren({expr, trueExpr, falseExpr});
        finishNode(node, expr ? expr->startOffset() : 0);
        return node;
    }

    return expr;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseLogicalOr() {
    auto left = parseLogicalAnd();

    // Support both || and "or"
    while (match(TokenType::Or) ||
           (currentToken().type == TokenType::Identifier && currentToken().value == "or" && (advance(), true))) {
        auto right = parseLogicalAnd();
        auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("||");
        node->setChildren({left, right});
        finishNode(node, left ? left->startOffset() : 0);
        left = node;
    }

    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseLogicalAnd() {
    auto left = parseComparison();
    
    // Support both && and "and"
    while (match(TokenType::And) || 
           (currentToken().type == TokenType::Identifier && currentToken().value == "and" && (advance(), true))) {
        auto right = parseComparison();
        auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("&&");
        node->setChildren({left, right});
        finishNode(node, left ? left->startOffset() : 0);
        left = node;
    }
    
    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseComparison() {
    auto left = parseAddSub();
    
    while (true) {
        Token op = currentToken();
        if (op.type == TokenType::EqualEqual || op.type == TokenType::NotEqual ||
            op.type == TokenType::Less || op.type == TokenType::LessEqual ||
            op.type == TokenType::Greater || op.type == TokenType::GreaterEqual) {
            advance();
            auto right = parseAddSub();
            auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
            finishNode(node, left ? left->startOffset() : 0);
            left = node;
        } else {
            break;
        }
    }
    
    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseAddSub() {
    auto left = parseMulDiv();
    
    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = tokens_[currentToken_ - 1];
        auto right = parseMulDiv();
            auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
            finishNode(node, left ? left->startOffset() : 0);
        left = node;
    }
    
    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseMulDiv() {
    auto left = parsePower();  // Parse power first (higher precedence)
    
    while (match(TokenType::Star) || match(TokenType::Slash) || match(TokenType::DoubleSlash)) {
        Token op = tokens_[currentToken_ - 1];
        auto right = parsePower();
            auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
            node->setOperatorSymbol(op.value);
            node->setChildren({left, right});
            finishNode(node, left ? left->startOffset() : 0);
        left = node;
    }
    
    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parsePower() {
    auto left = parseUnary();
    
    // Right-associative: 2**3**2 = 2**(3**2) = 512
    if (match(TokenType::DoubleStar)) {
        auto right = parsePower();  // Recursive for right-associativity
        auto node = makeShared<ExprNode>(ExprNodeType::BinaryOp);
        node->setOperatorSymbol("**");
        node->setChildren({left, right});
        finishNode(node, left ? left->startOffset() : 0);
        return node;
    }
    
    return left;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseUnary() {
    if (match(TokenType::Minus) || match(TokenType::Not)) {
        Token op = tokens_[currentToken_ - 1];
        auto expr = parseUnary();
            auto node = makeShared<ExprNode>(ExprNodeType::UnaryOp);
        node->setOperatorSymbol(op.value);
        node->setChildren({expr});
        finishNode(node, op.position);
        return node;
    }

    return parsePrimary();
}

SharedPtr<ExprNode> ExpressionParser::Impl::parsePrimary() {
    SharedPtr<ExprNode> base;

    // Number
    if (match(TokenType::Number)) {
        const Token& token = tokens_[currentToken_ - 1];
        base = makeShared<ExprNode>(ExprNodeType::Number);
        base->setNumberValue(std::stod(token.value));
        base->setSourceRange(token.position, token.end);
        return parsePostfix(base);
    }

    // String
    if (match(TokenType::String)) {
        const Token& token = tokens_[currentToken_ - 1];
        base = makeShared<ExprNode>(ExprNodeType::String);
        base->setStringValue(token.value);
        base->setSourceRange(token.position, token.end);
        return parsePostfix(base);
    }

    // Array literal [1, 2, 3]
    if (match(TokenType::LBracket)) {
        const size_t openBracket = tokens_[currentToken_ - 1].position;
        base = parseArrayLiteral(openBracket);
        return parsePostfix(base);
    }

    // Parenthesized expression or vector literal
    if (match(TokenType::LParen)) {
        const size_t openParen = tokens_[currentToken_ - 1].position;
        auto expr = parseExpression();
        if (!expr) {
            return nullptr;
        }

        // Vector literal: (x, y, z)
        if (match(TokenType::Comma)) {
            std::vector<SharedPtr<ExprNode>> elements = {expr};
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

            auto node = makeShared<ExprNode>(ExprNodeType::Vector);
            node->setChildren(elements);
            finishNode(node, openParen);
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
        const Token nameToken = tokens_[currentToken_ - 1];
        std::string name = nameToken.value;

        // Function call
        if (match(TokenType::LParen)) {
            base = parseFunctionCall(name);
            if (base) {
                base->setSourceRange(nameToken.position, lastConsumedTokenEnd());
            }
            return parsePostfix(base);
        }

        // Array/vector access: variable[index]
        if (match(TokenType::LBracket)) {
            auto index = parseExpression();
            if (!match(TokenType::RBracket)) {
                setError("Expected ']' after array index", currentToken().position);
                return nullptr;
            }
            auto varNode = makeShared<ExprNode>(ExprNodeType::Variable);
            varNode->setStringValue(name);
            varNode->setSourceRange(nameToken.position, nameToken.end);
            auto node = makeShared<ExprNode>(ExprNodeType::ArrayAccess);
            node->setChildren({varNode, index});
            finishNode(node, nameToken.position);
            return parsePostfix(node);
        }

        // Variable
        base = makeShared<ExprNode>(ExprNodeType::Variable);
        base->setStringValue(name);
        base->setSourceRange(nameToken.position, nameToken.end);
        return parsePostfix(base);
    }
    
    setError("Unexpected token in expression", currentToken().position);
    return nullptr;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parsePostfix(SharedPtr<ExprNode> base) {
    while (true) {
        if (currentToken().type == TokenType::Unknown && currentToken().value == ".") {
            advance();
            if (!match(TokenType::Identifier)) {
                setError("Expected property name after '.'", currentToken().position);
                return nullptr;
            }
            const Token& propertyToken = tokens_[currentToken_ - 1];
            auto node = makeShared<ExprNode>(ExprNodeType::PropertyAccess);
            node->setStringValue(propertyToken.value);
            node->setChildren({base});
            node->setSourceRange(base ? base->startOffset() : propertyToken.position,
                                 propertyToken.end);
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
            auto node = makeShared<ExprNode>(ExprNodeType::ArrayAccess);
            node->setChildren({base, index});
            finishNode(node, base ? base->startOffset() : 0);
            base = node;
            continue;
        }

        break;
    }

    return base;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseArrayLiteral(size_t openBracketOffset) {
    NamedVector<SharedPtr<ExprNode>> elements;
    
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
    
    auto node = makeShared<ExprNode>(ExprNodeType::ArrayLiteral);
    node->setChildren(elements.toStdVector());
    finishNode(node, openBracketOffset);
    return node;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseFunctionCall(const std::string& funcName) {
    NamedVector<SharedPtr<ExprNode>> args;
    
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
    
    auto node = makeShared<ExprNode>(ExprNodeType::FunctionCall);
    node->setStringValue(funcName);
    node->setChildren(args.toStdVector());
    // The name token was consumed by the caller; span from here to the closing
    // paren. The caller refines the start offset to cover the name as well.
    finishNode(node, currentToken_ > 0 ? tokens_[currentToken_ - 1].end : 0);
    return node;
}

SharedPtr<ExprNode> ExpressionParser::Impl::parseCallExpression(SharedPtr<ExprNode> callee, bool methodCall) {
    NamedVector<SharedPtr<ExprNode>> args;

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
        auto node = makeShared<ExprNode>(ExprNodeType::MethodCall);
        node->setStringValue(callee ? callee->stringValue() : std::string());
        NamedVector<SharedPtr<ExprNode>> children;
        children.append(callee);
        for (const auto& arg : args) {
            children.append(arg);
        }
        node->setChildren(children.toStdVector());
        finishNode(node, callee ? callee->startOffset() : 0);
        return node;
    }

    auto node = makeShared<ExprNode>(ExprNodeType::FunctionCall);
    node->setStringValue(callee ? callee->stringValue() : std::string());
    node->setChildren(args.toStdVector());
    finishNode(node, callee ? callee->startOffset() : 0);
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

SharedPtr<ExprNode> ExpressionParser::parse(const std::string& expression) {
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
