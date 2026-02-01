module;

module Script.Expression.Value;

import std;

namespace ArtifactCore {

class ExpressionValue::Impl {
public:
    ExprValueType type_ = ExprValueType::Null;
    double number_ = 0.0;
    std::vector<double> vector_;
    std::vector<ExpressionValue> array_;
    std::string string_;
};

ExpressionValue::ExpressionValue() : impl_(new Impl()) {}

ExpressionValue::ExpressionValue(double value) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::Number;
    impl_->number_ = value;
}

ExpressionValue::ExpressionValue(double x, double y) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::Vec2;
    impl_->vector_ = {x, y};
}

ExpressionValue::ExpressionValue(double x, double y, double z) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::Vec3;
    impl_->vector_ = {x, y, z};
}

ExpressionValue::ExpressionValue(double x, double y, double z, double w) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::Vec4;
    impl_->vector_ = {x, y, z, w};
}

ExpressionValue::ExpressionValue(const std::vector<ExpressionValue>& array) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::Array;
    impl_->array_ = array;
}

ExpressionValue::ExpressionValue(const std::string& str) : impl_(new Impl()) {
    impl_->type_ = ExprValueType::String;
    impl_->string_ = str;
}

ExpressionValue::ExpressionValue(const ExpressionValue& other) : impl_(new Impl()) {
    impl_->type_ = other.impl_->type_;
    impl_->number_ = other.impl_->number_;
    impl_->vector_ = other.impl_->vector_;
    impl_->array_ = other.impl_->array_;
    impl_->string_ = other.impl_->string_;
}

ExpressionValue::ExpressionValue(ExpressionValue&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

ExpressionValue& ExpressionValue::operator=(const ExpressionValue& other) {
    if (this != &other) {
        impl_->type_ = other.impl_->type_;
        impl_->number_ = other.impl_->number_;
        impl_->vector_ = other.impl_->vector_;
        impl_->array_ = other.impl_->array_;
        impl_->string_ = other.impl_->string_;
    }
    return *this;
}

ExpressionValue& ExpressionValue::operator=(ExpressionValue&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

ExpressionValue::~ExpressionValue() {
    delete impl_;
}

ExprValueType ExpressionValue::type() const { return impl_->type_; }
bool ExpressionValue::isNull() const { return impl_->type_ == ExprValueType::Null; }
bool ExpressionValue::isNumber() const { return impl_->type_ == ExprValueType::Number; }
bool ExpressionValue::isVector() const { 
    return impl_->type_ == ExprValueType::Vec2 || 
           impl_->type_ == ExprValueType::Vec3 || 
           impl_->type_ == ExprValueType::Vec4; 
}
bool ExpressionValue::isArray() const { return impl_->type_ == ExprValueType::Array; }
bool ExpressionValue::isString() const { return impl_->type_ == ExprValueType::String; }

double ExpressionValue::asNumber() const {
    if (impl_->type_ == ExprValueType::Number) return impl_->number_;
    if (impl_->type_ == ExprValueType::Vec2 || 
        impl_->type_ == ExprValueType::Vec3 || 
        impl_->type_ == ExprValueType::Vec4) {
        return impl_->vector_.empty() ? 0.0 : impl_->vector_[0];
    }
    return 0.0;
}

std::vector<double> ExpressionValue::asVector() const {
    if (isVector()) return impl_->vector_;
    if (impl_->type_ == ExprValueType::Number) return {impl_->number_};
    return {};
}

std::vector<ExpressionValue> ExpressionValue::asArray() const {
    if (impl_->type_ == ExprValueType::Array) return impl_->array_;
    return {};
}

std::string ExpressionValue::asString() const {
    if (impl_->type_ == ExprValueType::String) return impl_->string_;
    return toString();
}

double ExpressionValue::x() const { return impl_->vector_.size() > 0 ? impl_->vector_[0] : 0.0; }
double ExpressionValue::y() const { return impl_->vector_.size() > 1 ? impl_->vector_[1] : 0.0; }
double ExpressionValue::z() const { return impl_->vector_.size() > 2 ? impl_->vector_[2] : 0.0; }
double ExpressionValue::w() const { return impl_->vector_.size() > 3 ? impl_->vector_[3] : 0.0; }

size_t ExpressionValue::length() const {
    if (impl_->type_ == ExprValueType::Array) return impl_->array_.size();
    if (isVector()) return impl_->vector_.size();
    return 0;
}

ExpressionValue ExpressionValue::at(size_t index) const {
    if (impl_->type_ == ExprValueType::Array && index < impl_->array_.size()) {
        return impl_->array_[index];
    }
    if (isVector() && index < impl_->vector_.size()) {
        return ExpressionValue(impl_->vector_[index]);
    }
    return ExpressionValue();
}

void ExpressionValue::push(const ExpressionValue& value) {
    if (impl_->type_ == ExprValueType::Null) {
        impl_->type_ = ExprValueType::Array;
    }
    if (impl_->type_ == ExprValueType::Array) {
        impl_->array_.push_back(value);
    }
}

ExpressionValue ExpressionValue::operator+(const ExpressionValue& rhs) const {
    if (isNumber() && rhs.isNumber()) {
        return ExpressionValue(asNumber() + rhs.asNumber());
    }
    if (isVector() && rhs.isVector()) {
        auto lv = asVector();
        auto rv = rhs.asVector();
        size_t size = std::min(lv.size(), rv.size());
        std::vector<double> result(size);
        for (size_t i = 0; i < size; ++i) {
            result[i] = lv[i] + rv[i];
        }
        if (size == 2) return ExpressionValue(result[0], result[1]);
        if (size == 3) return ExpressionValue(result[0], result[1], result[2]);
        if (size == 4) return ExpressionValue(result[0], result[1], result[2], result[3]);
    }
    return ExpressionValue();
}

ExpressionValue ExpressionValue::operator-(const ExpressionValue& rhs) const {
    if (isNumber() && rhs.isNumber()) {
        return ExpressionValue(asNumber() - rhs.asNumber());
    }
    if (isVector() && rhs.isVector()) {
        auto lv = asVector();
        auto rv = rhs.asVector();
        size_t size = std::min(lv.size(), rv.size());
        std::vector<double> result(size);
        for (size_t i = 0; i < size; ++i) {
            result[i] = lv[i] - rv[i];
        }
        if (size == 2) return ExpressionValue(result[0], result[1]);
        if (size == 3) return ExpressionValue(result[0], result[1], result[2]);
        if (size == 4) return ExpressionValue(result[0], result[1], result[2], result[3]);
    }
    return ExpressionValue();
}

ExpressionValue ExpressionValue::operator*(const ExpressionValue& rhs) const {
    if (isNumber() && rhs.isNumber()) {
        return ExpressionValue(asNumber() * rhs.asNumber());
    }
    // Scalar * Vector
    if (isNumber() && rhs.isVector()) {
        double scalar = asNumber();
        auto vec = rhs.asVector();
        for (auto& v : vec) v *= scalar;
        if (vec.size() == 2) return ExpressionValue(vec[0], vec[1]);
        if (vec.size() == 3) return ExpressionValue(vec[0], vec[1], vec[2]);
        if (vec.size() == 4) return ExpressionValue(vec[0], vec[1], vec[2], vec[3]);
    }
    // Vector * Scalar
    if (isVector() && rhs.isNumber()) {
        double scalar = rhs.asNumber();
        auto vec = asVector();
        for (auto& v : vec) v *= scalar;
        if (vec.size() == 2) return ExpressionValue(vec[0], vec[1]);
        if (vec.size() == 3) return ExpressionValue(vec[0], vec[1], vec[2]);
        if (vec.size() == 4) return ExpressionValue(vec[0], vec[1], vec[2], vec[3]);
    }
    // Component-wise multiplication
    if (isVector() && rhs.isVector()) {
        auto lv = asVector();
        auto rv = rhs.asVector();
        size_t size = std::min(lv.size(), rv.size());
        std::vector<double> result(size);
        for (size_t i = 0; i < size; ++i) {
            result[i] = lv[i] * rv[i];
        }
        if (size == 2) return ExpressionValue(result[0], result[1]);
        if (size == 3) return ExpressionValue(result[0], result[1], result[2]);
        if (size == 4) return ExpressionValue(result[0], result[1], result[2], result[3]);
    }
    return ExpressionValue();
}

ExpressionValue ExpressionValue::operator/(const ExpressionValue& rhs) const {
    if (isNumber() && rhs.isNumber()) {
        double divisor = rhs.asNumber();
        if (divisor != 0.0) return ExpressionValue(asNumber() / divisor);
    }
    if (isVector() && rhs.isNumber()) {
        double divisor = rhs.asNumber();
        if (divisor != 0.0) {
            auto vec = asVector();
            for (auto& v : vec) v /= divisor;
            if (vec.size() == 2) return ExpressionValue(vec[0], vec[1]);
            if (vec.size() == 3) return ExpressionValue(vec[0], vec[1], vec[2]);
            if (vec.size() == 4) return ExpressionValue(vec[0], vec[1], vec[2], vec[3]);
        }
    }
    return ExpressionValue();
}

bool ExpressionValue::operator==(const ExpressionValue& rhs) const {
    if (impl_->type_ != rhs.impl_->type_) return false;
    if (isNumber()) return asNumber() == rhs.asNumber();
    if (isVector()) return asVector() == rhs.asVector();
    if (isString()) return asString() == rhs.asString();
    return false;
}

bool ExpressionValue::operator!=(const ExpressionValue& rhs) const {
    return !(*this == rhs);
}

bool ExpressionValue::operator<(const ExpressionValue& rhs) const {
    if (isNumber() && rhs.isNumber()) return asNumber() < rhs.asNumber();
    return false;
}

bool ExpressionValue::operator<=(const ExpressionValue& rhs) const {
    return *this < rhs || *this == rhs;
}

bool ExpressionValue::operator>(const ExpressionValue& rhs) const {
    return !(*this <= rhs);
}

bool ExpressionValue::operator>=(const ExpressionValue& rhs) const {
    return !(*this < rhs);
}

std::string ExpressionValue::toString() const {
    switch (impl_->type_) {
    case ExprValueType::Null:
        return "null";
    case ExprValueType::Number:
        return std::to_string(impl_->number_);
    case ExprValueType::Vec2:
    case ExprValueType::Vec3:
    case ExprValueType::Vec4: {
        std::string result = "[";
        for (size_t i = 0; i < impl_->vector_.size(); ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(impl_->vector_[i]);
        }
        result += "]";
        return result;
    }
    case ExprValueType::Array: {
        std::string result = "[";
        for (size_t i = 0; i < impl_->array_.size(); ++i) {
            if (i > 0) result += ", ";
            result += impl_->array_[i].toString();
        }
        result += "]";
        return result;
    }
    case ExprValueType::String:
        return "\"" + impl_->string_ + "\"";
    }
    return "";
}

}
