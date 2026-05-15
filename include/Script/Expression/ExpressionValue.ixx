module;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Script.Expression.Value;

export namespace ArtifactCore {

// Expression value type (similar to AE's expression values)
enum class ExprValueType {
    Null,
    Number,     // Single number
    Vec2,       // 2D vector [x, y]
    Vec3,       // 3D vector [x, y, z]
    Vec4,       // 4D vector (color RGBA)
    Array,      // Array of values
    String,     // String value
    Object      // Named property bag
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
    explicit ExpressionValue(const std::map<std::string, ExpressionValue>& object);
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
    bool isObject() const;

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
    bool hasProperty(const std::string& name) const;
    ExpressionValue property(const std::string& name) const;
    
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

    // Vector operations
    ExpressionValue normalized() const;
};

}
