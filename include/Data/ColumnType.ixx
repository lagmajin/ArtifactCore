module;

#include <string>
#include <cstdint>

export module Data.ColumnType;

export namespace ArtifactCore {

enum class ColumnType {
    Unknown,
    String,
    Int,
    Float,
    Bool,
    Date,
    Time,
    DateTime
};

inline const char* columnTypeToString(ColumnType t) {
    switch (t) {
    case ColumnType::Unknown:   return "Unknown";
    case ColumnType::String:    return "String";
    case ColumnType::Int:       return "Int";
    case ColumnType::Float:     return "Float";
    case ColumnType::Bool:      return "Bool";
    case ColumnType::Date:      return "Date";
    case ColumnType::Time:      return "Time";
    case ColumnType::DateTime:  return "DateTime";
    }
    return "Unknown";
}

inline ColumnType columnTypeFromString(const std::string& s) {
    if (s == "String" || s == "string") return ColumnType::String;
    if (s == "Int" || s == "int" || s == "Integer" || s == "integer") return ColumnType::Int;
    if (s == "Float" || s == "float" || s == "Double" || s == "double" || s == "Number" || s == "number") return ColumnType::Float;
    if (s == "Bool" || s == "bool" || s == "Boolean" || s == "boolean") return ColumnType::Bool;
    if (s == "Date" || s == "date") return ColumnType::Date;
    if (s == "Time" || s == "time") return ColumnType::Time;
    if (s == "DateTime" || s == "datetime" || s == "Timestamp" || s == "timestamp") return ColumnType::DateTime;
    return ColumnType::Unknown;
}

} // namespace ArtifactCore
