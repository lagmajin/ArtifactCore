module;

#include <string>
#include <vector>
#include <cstdint>

export module Data.TypedColumn;

import Data.ColumnType;
import Data.TypeInference;

export namespace ArtifactCore {

class TypedColumn {
public:
    TypedColumn() = default;

    TypedColumn(const std::string& name, const std::vector<std::string>& rawValues)
        : name_(name)
    {
        type_ = TypeInference::inferFromValues(rawValues);
        values_.reserve(rawValues.size());
        for (const auto& v : rawValues) {
            values_.push_back(parseValue(v));
        }
    }

    const std::string& name() const { return name_; }
    ColumnType type() const { return type_; }
    int size() const { return static_cast<int>(values_.size()); }

    std::string getString(int row) const {
        if (row < 0 || row >= size()) return "";
        return valueToString(values_[row]);
    }

    int64_t getInt(int row, int64_t fallback = 0) const {
        if (row < 0 || row >= size()) return fallback;
        return valueToInt(values_[row]);
    }

    double getFloat(int row, double fallback = 0.0) const {
        if (row < 0 || row >= size()) return fallback;
        return valueToFloat(values_[row]);
    }

    bool getBool(int row, bool fallback = false) const {
        if (row < 0 || row >= size()) return fallback;
        return valueToBool(values_[row]);
    }

private:
    struct ParsedValue {
        std::string strVal;
        int64_t intVal = 0;
        double floatVal = 0.0;
        bool boolVal = false;
    };

    ParsedValue parseValue(const std::string& s) const {
        ParsedValue pv;
        pv.strVal = s;

        switch (type_) {
        case ColumnType::Int:
            try { pv.intVal = std::stoll(s); } catch (...) { pv.intVal = 0; }
            pv.floatVal = static_cast<double>(pv.intVal);
            break;
        case ColumnType::Float:
            try { pv.floatVal = std::stod(s); } catch (...) { pv.floatVal = 0.0; }
            pv.intVal = static_cast<int64_t>(pv.floatVal);
            break;
        case ColumnType::Bool:
            pv.boolVal = (s == "true" || s == "1" || s == "yes" || s == "TRUE" || s == "Yes");
            pv.intVal = pv.boolVal ? 1 : 0;
            pv.floatVal = pv.boolVal ? 1.0 : 0.0;
            break;
        default:
            break;
        }

        return pv;
    }

    std::string valueToString(const ParsedValue& v) const { return v.strVal; }
    int64_t valueToInt(const ParsedValue& v) const { return v.intVal; }
    double valueToFloat(const ParsedValue& v) const { return v.floatVal; }
    bool valueToBool(const ParsedValue& v) const { return v.boolVal; }

    std::string name_;
    ColumnType type_ = ColumnType::Unknown;
    std::vector<ParsedValue> values_;
};

} // namespace ArtifactCore
