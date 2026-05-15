module;

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

export module Data.DataTable;

import Data.CsvParser;

export namespace ArtifactCore {

using DataValue = std::variant<std::monostate, std::string, int64_t, double, bool>;

inline std::string dataValueToString(const DataValue& v) {
    if (std::holds_alternative<std::monostate>(v)) return "";
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    return "";
}

struct ColumnSchema {
    std::string name;
    int index = 0;
};

class DataTable {
public:
    DataTable() = default;

    explicit DataTable(const CsvParseResult& csv) {
        if (!csv.ok()) return;

        if (csv.headers.empty()) {
            for (int i = 0; i < csv.columnCount; ++i) {
                headers_.push_back(ColumnSchema{"col_" + std::to_string(i), i});
            }
        } else {
            for (int i = 0; i < csv.columnCount; ++i) {
                headers_.push_back(ColumnSchema{csv.headers[i], i});
            }
        }

        nameToIndex_.clear();
        for (int i = 0; i < static_cast<int>(headers_.size()); ++i) {
            nameToIndex_[headers_[i].name] = i;
        }

        rows_.resize(csv.rowCount);
        for (int r = 0; r < csv.rowCount; ++r) {
            rows_[r].resize(csv.columnCount);
            for (int c = 0; c < csv.columnCount; ++c) {
                rows_[r][c] = csv.rows[r][c];
            }
        }
    }

    int rowCount() const { return static_cast<int>(rows_.size()); }
    int columnCount() const { return static_cast<int>(headers_.size()); }

    const std::vector<ColumnSchema>& columns() const { return headers_; }

    int columnIndexByName(const std::string& name) const {
        auto it = nameToIndex_.find(name);
        if (it != nameToIndex_.end()) return it->second;
        return -1;
    }

    std::string columnName(int index) const {
        if (index < 0 || index >= static_cast<int>(headers_.size())) return "";
        return headers_[index].name;
    }

    DataValue at(int row, int col) const {
        if (row < 0 || row >= rowCount() || col < 0 || col >= columnCount()) {
            return std::monostate{};
        }
        return rows_[row][col];
    }

    DataValue at(int row, const std::string& colName) const {
        int col = columnIndexByName(colName);
        if (col < 0) return std::monostate{};
        return at(row, col);
    }

    std::string getString(int row, int col) const {
        auto v = at(row, col);
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        return dataValueToString(v);
    }

    std::string getString(int row, const std::string& colName) const {
        int col = columnIndexByName(colName);
        if (col < 0) return "";
        return getString(row, col);
    }

    int64_t getInt(int row, int col, int64_t fallback = 0) const {
        auto v = at(row, col);
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
        if (std::holds_alternative<std::string>(v)) {
            try { return std::stoll(std::get<std::string>(v)); } catch (...) {}
        }
        if (std::holds_alternative<double>(v)) return static_cast<int64_t>(std::get<double>(v));
        return fallback;
    }

    int64_t getInt(int row, const std::string& colName, int64_t fallback = 0) const {
        int col = columnIndexByName(colName);
        if (col < 0) return fallback;
        return getInt(row, col, fallback);
    }

    double getFloat(int row, int col, double fallback = 0.0) const {
        auto v = at(row, col);
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<int64_t>(v)) return static_cast<double>(std::get<int64_t>(v));
        if (std::holds_alternative<std::string>(v)) {
            try { return std::stod(std::get<std::string>(v)); } catch (...) {}
        }
        return fallback;
    }

    double getFloat(int row, const std::string& colName, double fallback = 0.0) const {
        int col = columnIndexByName(colName);
        if (col < 0) return fallback;
        return getFloat(row, col, fallback);
    }

    bool getBool(int row, int col, bool fallback = false) const {
        auto v = at(row, col);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
        if (std::holds_alternative<std::string>(v)) {
            const auto& s = std::get<std::string>(v);
            if (s == "true" || s == "1" || s == "yes") return true;
            if (s == "false" || s == "0" || s == "no") return false;
        }
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v) != 0;
        return fallback;
    }

    bool getBool(int row, const std::string& colName, bool fallback = false) const {
        int col = columnIndexByName(colName);
        if (col < 0) return fallback;
        return getBool(row, col, fallback);
    }

    const std::vector<DataValue>& row(int index) const {
        static const std::vector<DataValue> empty;
        if (index < 0 || index >= rowCount()) return empty;
        return rows_[index];
    }

    void set(int row, int col, const DataValue& value) {
        if (row < 0 || row >= rowCount() || col < 0 || col >= columnCount()) return;
        rows_[row][col] = value;
    }

    void addRow(const std::vector<DataValue>& rowData) {
        if (static_cast<int>(rowData.size()) != columnCount()) return;
        rows_.push_back(rowData);
    }

    void addColumn(const std::string& name) {
        if (nameToIndex_.count(name) > 0) return;
        int idx = static_cast<int>(headers_.size());
        headers_.push_back(ColumnSchema{name, idx});
        nameToIndex_[name] = idx;
        for (auto& row : rows_) {
            row.push_back(std::monostate{});
        }
    }

    void clear() {
        headers_.clear();
        nameToIndex_.clear();
        rows_.clear();
    }

    bool empty() const { return rows_.empty(); }

private:
    std::vector<ColumnSchema> headers_;
    std::unordered_map<std::string, int> nameToIndex_;
    std::vector<std::vector<DataValue>> rows_;
};

} // namespace ArtifactCore
