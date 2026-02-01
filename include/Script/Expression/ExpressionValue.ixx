module;

export module Script.Expression.Value;

import std;

export namespace ArtifactCore {

// Expression value type (similar to AE's expression values)
enum class ExprValueType {
    Null,
    Number,     // Single number
    Vec2,       // 2D vector [x, y]
    Vec3,       // 3D vector [x, y, z]
    Vec4,       // 4D vector (color RGBA)
    Array,      // Array of values
    String      // String value
};

class ExpressionValue {
private:
    class Impl;
    Impl* impl_;

public:
    ExpressionValue();
    explicit ExpressionValue(double value);
    ExpressionValue(double x, double y);
    ExpressionValue(double x, double y, double z);
    ExpressionValue(double x, double y, double z, double w);
    explicit ExpressionValue(const std::vector<ExpressionValue>& array);
    explicit ExpressionValue(const std::string& str);
    ExpressionValue(const ExpressionValue& other);
    ExpressionValue(ExpressionValue&& other) noexcept;
    ExpressionValue& operator=(const ExpressionValue& other);
    ExpressionValue& operator=(ExpressionValue&& other) noexcept;
    ~ExpressionValue();

    // Type checking
    ExprValueType type() const;
    bool isNull() const;
    bool isNumber() const;
    bool isVector() const;
    bool isArray() const;
    bool isString() const;

    // Conversion
    double asNumber() const;
    std::vector<double> asVector() const;
    std::vector<ExpressionValue> asArray() const;
    std::string asString() const;

    // Vector component access
    double x() const;
    double y() const;
    double z() const;
    double w() const;
    
    // Array operations
    size_t length() const;
    ExpressionValue at(size_t index) const;
    void push(const ExpressionValue& value);

    // Arithmetic operators
    ExpressionValue operator+(const ExpressionValue& rhs) const;
    ExpressionValue operator-(const ExpressionValue& rhs) const;
    ExpressionValue operator*(const ExpressionValue& rhs) const;
    ExpressionValue operator/(const ExpressionValue& rhs) const;
    
    // Comparison operators
    bool operator==(const ExpressionValue& rhs) const;
    bool operator!=(const ExpressionValue& rhs) const;
    bool operator<(const ExpressionValue& rhs) const;
    bool operator<=(const ExpressionValue& rhs) const;
    bool operator>(const ExpressionValue& rhs) const;
    bool operator>=(const ExpressionValue& rhs) const;

    // String representation for debugging
    std::string toString() const;
};

}
