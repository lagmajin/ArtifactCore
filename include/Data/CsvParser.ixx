module;

#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <cstdint>

export module Data.CsvParser;

export namespace ArtifactCore {

enum class CsvParseError {
    None,
    FileNotFound,
    FileReadError,
    EmptyFile,
    EncodingError,
    MalformedRow,
    InconsistentColumnCount
};

inline const char* csvParseErrorToString(CsvParseError err) {
    switch (err) {
    case CsvParseError::None: return "None";
    case CsvParseError::FileNotFound: return "FileNotFound";
    case CsvParseError::FileReadError: return "FileReadError";
    case CsvParseError::EmptyFile: return "EmptyFile";
    case CsvParseError::EncodingError: return "EncodingError";
    case CsvParseError::MalformedRow: return "MalformedRow";
    case CsvParseError::InconsistentColumnCount: return "InconsistentColumnCount";
    }
    return "Unknown";
}

enum class CsvDelimiter {
    Comma,
    Semicolon,
    Tab,
    Pipe,
    Auto
};

inline char csvDelimiterToChar(CsvDelimiter d) {
    switch (d) {
    case CsvDelimiter::Comma: return ',';
    case CsvDelimiter::Semicolon: return ';';
    case CsvDelimiter::Tab: return '\t';
    case CsvDelimiter::Pipe: return '|';
    case CsvDelimiter::Auto: return ',';
    }
    return ',';
}

struct CsvParseOptions {
    CsvDelimiter delimiter = CsvDelimiter::Auto;
    bool hasHeader = true;
    bool trimWhitespace = true;
    bool skipEmptyRows = true;
    char quoteChar = '"';
    char escapeChar = '\0';
};

struct CsvParseResult {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> headers;
    CsvParseError error = CsvParseError::None;
    std::string errorMessage;
    int rowCount = 0;
    int columnCount = 0;

    bool ok() const { return error == CsvParseError::None; }
};

class CsvParser {
public:
    static CsvParseResult parseFile(const std::filesystem::path& path,
                                     const CsvParseOptions& options = CsvParseOptions()) {
        CsvParseResult result;

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            result.error = CsvParseError::FileNotFound;
            result.errorMessage = "Cannot open file: " + path.string();
            return result;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        if (content.empty()) {
            result.error = CsvParseError::EmptyFile;
            result.errorMessage = "File is empty";
            return result;
        }

        return parseString(content, options);
    }

    static CsvParseResult parseString(std::string_view input,
                                       const CsvParseOptions& options = CsvParseOptions()) {
        CsvParseResult result;

        if (input.empty()) {
            result.error = CsvParseError::EmptyFile;
            result.errorMessage = "Input is empty";
            return result;
        }

        char delim = options.delimiter == CsvDelimiter::Auto
                         ? detectDelimiter(input)
                         : csvDelimiterToChar(options.delimiter);

        std::vector<std::vector<std::string>> rawRows = parseRows(input, delim, options);

        if (rawRows.empty()) {
            result.error = CsvParseError::EmptyFile;
            result.errorMessage = "No rows found";
            return result;
        }

        if (options.skipEmptyRows) {
            rawRows.erase(
                std::remove_if(rawRows.begin(), rawRows.end(),
                               [](const std::vector<std::string>& row) {
                                   return row.empty() ||
                                          (row.size() == 1 && row[0].empty());
                               }),
                rawRows.end());
        }

        if (rawRows.empty()) {
            result.error = CsvParseError::EmptyFile;
            result.errorMessage = "No data rows after filtering";
            return result;
        }

        int expectedCols = static_cast<int>(rawRows[0].size());
        for (size_t i = 1; i < rawRows.size(); ++i) {
            if (static_cast<int>(rawRows[i].size()) != expectedCols) {
                result.error = CsvParseError::InconsistentColumnCount;
                result.errorMessage = "Row " + std::to_string(i + 1) +
                                      " has " + std::to_string(rawRows[i].size()) +
                                      " columns, expected " + std::to_string(expectedCols);
                return result;
            }
        }

        if (options.hasHeader && !rawRows.empty()) {
            result.headers = std::move(rawRows[0]);
            result.rows.assign(rawRows.begin() + 1, rawRows.end());
        } else {
            result.rows = std::move(rawRows);
        }

        result.rowCount = static_cast<int>(result.rows.size());
        result.columnCount = result.headers.empty()
                                 ? (result.rows.empty() ? 0 : static_cast<int>(result.rows[0].size()))
                                 : static_cast<int>(result.headers.size());

        return result;
    }

    static char detectDelimiter(std::string_view input) {
        int commaCount = 0;
        int semicolonCount = 0;
        int tabCount = 0;
        int pipeCount = 0;

        bool inQuotes = false;
        for (char c : input) {
            if (c == '"') {
                inQuotes = !inQuotes;
                continue;
            }
            if (inQuotes) continue;

            if (c == ',') ++commaCount;
            else if (c == ';') ++semicolonCount;
            else if (c == '\t') ++tabCount;
            else if (c == '|') ++pipeCount;
        }

        int maxCount = commaCount;
        char best = ',';

        if (semicolonCount > maxCount) { maxCount = semicolonCount; best = ';'; }
        if (tabCount > maxCount) { maxCount = tabCount; best = '\t'; }
        if (pipeCount > maxCount) { maxCount = pipeCount; best = '|'; }

        return best;
    }

private:
    static std::vector<std::vector<std::string>> parseRows(
        std::string_view input, char delimiter, const CsvParseOptions& options)
    {
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> currentRow;
        std::string currentField;
        bool inQuotes = false;
        bool fieldStarted = false;

        size_t i = 0;
        const size_t len = input.size();

        auto finishField = [&]() {
            std::string field = currentField;
            if (options.trimWhitespace) {
                size_t start = field.find_first_not_of(" \t\r\n");
                size_t end = field.find_last_not_of(" \t\r\n");
                if (start == std::string::npos) {
                    field = "";
                } else {
                    field = field.substr(start, end - start + 1);
                }
            }
            currentRow.push_back(field);
            currentField.clear();
            fieldStarted = false;
        };

        auto finishRow = [&]() {
            if (!currentRow.empty() || fieldStarted) {
                finishField();
                rows.push_back(std::move(currentRow));
                currentRow = {};
            }
        };

        while (i < len) {
            char c = input[i];

            if (inQuotes) {
                if (c == options.quoteChar) {
                    if (i + 1 < len && input[i + 1] == options.quoteChar) {
                        currentField += options.quoteChar;
                        ++i;
                    } else {
                        inQuotes = false;
                    }
                } else if (options.escapeChar != '\0' && c == options.escapeChar) {
                    if (i + 1 < len) {
                        currentField += input[i + 1];
                        ++i;
                    }
                } else {
                    currentField += c;
                }
            } else {
                if (c == options.quoteChar && !fieldStarted) {
                    inQuotes = true;
                    fieldStarted = true;
                } else if (c == delimiter) {
                    finishField();
                } else if (c == '\r') {
                    if (i + 1 < len && input[i + 1] == '\n') {
                        finishRow();
                        ++i;
                    } else {
                        finishRow();
                    }
                } else if (c == '\n') {
                    finishRow();
                } else {
                    currentField += c;
                    fieldStarted = true;
                }
            }

            ++i;
        }

        finishRow();

        return rows;
    }
};

} // namespace ArtifactCore
