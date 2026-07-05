module;

#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <cstdint>

export module Data.TypeInference;

import Data.ColumnType;

export namespace ArtifactCore {

class TypeInference {
public:
    static ColumnType inferFromValues(const std::vector<std::string>& values) {
        bool allBool = true;
        bool allInt = true;
        bool allFloat = true;
        bool allDate = true;
        bool allTime = true;
        bool allDateTime = true;
        bool hasValid = false;

        for (const auto& v : values) {
            if (v.empty()) continue;
            hasValid = true;

            if (allBool && !isBool(v)) allBool = false;
            if (allInt && !isInt(v)) allInt = false;
            if (allFloat && !isFloat(v)) allFloat = false;
            if (allDate && !isDate(v)) allDate = false;
            if (allTime && !isTime(v)) allTime = false;
            if (allDateTime && !isDateTime(v)) allDateTime = false;

            if (!allBool && !allInt && !allFloat && !allDate && !allTime && !allDateTime) {
                return ColumnType::String;
            }
        }

        if (!hasValid) return ColumnType::Unknown;
        if (allDateTime) return ColumnType::DateTime;
        if (allDate) return ColumnType::Date;
        if (allTime) return ColumnType::Time;
        if (allBool) return ColumnType::Bool;
        if (allInt) return ColumnType::Int;
        if (allFloat) return ColumnType::Float;
        return ColumnType::String;
    }

private:
    static bool isBool(std::string_view s) {
        return s == "true" || s == "false" || s == "1" || s == "0" ||
               s == "yes" || s == "no" || s == "TRUE" || s == "FALSE" ||
               s == "Yes" || s == "No";
    }

    static bool isInt(std::string_view s) {
        if (s.empty()) return false;
        size_t start = 0;
        if (s[0] == '-' || s[0] == '+') start = 1;
        if (start >= s.size()) return false;
        for (size_t i = start; i < s.size(); ++i) {
            if (s[i] < '0' || s[i] > '9') return false;
        }
        return true;
    }

    static bool isFloat(std::string_view s) {
        if (s.empty()) return false;
        size_t start = 0;
        if (s[0] == '-' || s[0] == '+') start = 1;
        if (start >= s.size()) return false;
        bool hasDot = false;
        bool hasDigit = false;
        for (size_t i = start; i < s.size(); ++i) {
            if (s[i] == '.') {
                if (hasDot) return false;
                hasDot = true;
            } else if (s[i] >= '0' && s[i] <= '9') {
                hasDigit = true;
            } else if (s[i] == 'e' || s[i] == 'E') {
                if (i + 1 >= s.size()) return false;
                if (s[i + 1] == '-' || s[i + 1] == '+') ++i;
                if (i + 1 >= s.size()) return false;
                for (size_t j = i + 1; j < s.size(); ++j) {
                    if (s[j] < '0' || s[j] > '9') return false;
                }
                return hasDigit;
            } else {
                return false;
            }
        }
        return hasDigit;
    }

    static bool isDate(std::string_view s) {
        static const std::regex dateRegex(R"(\d{4}[-/]\d{1,2}[-/]\d{1,2})");
        static const std::regex dateRegex2(R"(\d{1,2}[-/]\d{1,2}[-/]\d{4})");
        return std::regex_match(std::string(s), dateRegex) ||
               std::regex_match(std::string(s), dateRegex2);
    }

    static bool isTime(std::string_view s) {
        static const std::regex timeRegex(R"(\d{1,2}:\d{2}(:\d{2})?(\.\d+)?)");
        return std::regex_match(std::string(s), timeRegex);
    }

    static bool isDateTime(std::string_view s) {
        static const std::regex dtRegex(R"(\d{4}[-/]\d{1,2}[-/]\d{1,2}[T ]\d{1,2}:\d{2}(:\d{2})?(\.\d+)?)");
        return std::regex_match(std::string(s), dtRegex);
    }
};

} // namespace ArtifactCore
