module;

export module Script.Expression.Parser;

import std;
import Script.Expression.Value;

export namespace ArtifactCore {

// Language style for expression syntax
enum class ExpressionLanguageStyle {
    Flexible,      // Allow multiple styles simultaneously (default)
    JavaScript,    // JS/C-style: condition ? a : b, &&, ||, !
    Python,        // Python-style: a if condition else b, and, or, not, **
    Ruby,          // Ruby-style: condition ? a : b, unless, if-then-end
    VBScript       // VB-style: IIf(condition, a, b), And, Or, Not
};

// AST Node types
enum class ExprNodeType {
    Number,
    Vector,
    String,
    Variable,
    ArrayLiteral,
    ArrayAccess,
    BinaryOp,
    UnaryOp,
    FunctionCall,
    Conditional,  // Ternary: condition ? true_expr : false_expr
    Assignment
};

// AST Node base
class ExprNode {
private:
    class Impl;
    Impl* impl_;

public:
    ExprNode(ExprNodeType type);
    virtual ~ExprNode();
    
    ExprNodeType type() const;
    void setType(ExprNodeType type);
    // setters for builder (parser)
    void setChildren(const std::vector<std::shared_ptr<ExprNode>>& children);
    void setOperatorSymbol(const std::string& op);
    void setNumberValue(double v);
    void setStringValue(const std::string& s);
    
    // Accessors for AST inspection (usable by other modules)
    std::size_t childCount() const;
    std::shared_ptr<ExprNode> child(std::size_t index) const;
    std::string operatorSymbol() const;
    double numberValue() const;
    std::string stringValue() const;
};

// Parser for expression syntax
class ExpressionParser {
private:
    class Impl;
    Impl* impl_;

public:
    ExpressionParser();
    ~ExpressionParser();

    // Set language style
    void setLanguageStyle(ExpressionLanguageStyle style);
    ExpressionLanguageStyle languageStyle() const;

    // Parse expression string into AST
    std::shared_ptr<ExprNode> parse(const std::string& expression);
    
    // Get last error message
    std::string getError() const;
    bool hasError() const;
};

}
