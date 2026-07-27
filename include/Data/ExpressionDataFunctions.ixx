module;

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

export module Data.ExpressionDataFunctions;

import Data.DataPropertyBridge;
import Core.ArtifactString;

export namespace ArtifactCore {

using ExpressionDataFunc = std::function<double(const std::vector<double>& args)>;

class ExpressionDataFunctions {
public:
    static void registerFunctions(std::unordered_map<std::string, ExpressionDataFunc>& registry) {
        registry["dataGet"] = [](const std::vector<double>& args) -> double {
            if (args.size() < 3) return 0.0;
            String uri;
            int row = static_cast<int>(args[0]);
            int colIndex = static_cast<int>(args[1]);

            if (args.size() >= 4) {
                int uriIdx = static_cast<int>(args[3]);
                uri = uriFromIndex(uriIdx);
            }

            if (uri.isEmpty()) return 0.0;
            const auto value = DataPropertyBridge::getValue(uri, row, String(std::to_string(colIndex)));
            return value.isEmpty() ? 0.0 : std::stod(toStdString(value));
        };

        registry["dataRowCount"] = [](const std::vector<double>& args) -> double {
            if (args.empty()) return 0.0;
            String uri = uriFromIndex(static_cast<int>(args[0]));
            if (uri.isEmpty()) return 0.0;
            return static_cast<double>(DataPropertyBridge::getRowCount(uri));
        };

        registry["dataColCount"] = [](const std::vector<double>& args) -> double {
            if (args.empty()) return 0.0;
            String uri = uriFromIndex(static_cast<int>(args[0]));
            if (uri.isEmpty()) return 0.0;
            return static_cast<double>(DataPropertyBridge::getColumnCount(uri));
        };
    }

    static void registerDataSource(const String& uri, int index) {
        uriMap_[index] = toStdString(uri);
        DataPropertyBridge::registerDataSource(uri);
    }

private:
    static String uriFromIndex(int index) {
        auto it = uriMap_.find(index);
        if (it != uriMap_.end()) return String(it->second);
        return {};
    }

    static std::unordered_map<int, std::string> uriMap_;
};

inline std::unordered_map<int, std::string> ExpressionDataFunctions::uriMap_;

} // namespace ArtifactCore
