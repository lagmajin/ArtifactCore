module;

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

export module Data.ExpressionDataFunctions;

import Data.DataPropertyBridge;

export namespace ArtifactCore {

using ExpressionDataFunc = std::function<double(const std::vector<double>& args)>;

class ExpressionDataFunctions {
public:
    static void registerFunctions(std::unordered_map<std::string, ExpressionDataFunc>& registry) {
        registry["dataGet"] = [](const std::vector<double>& args) -> double {
            if (args.size() < 3) return 0.0;
            std::string uri;
            int row = static_cast<int>(args[0]);
            int colIndex = static_cast<int>(args[1]);

            if (args.size() >= 4) {
                int uriIdx = static_cast<int>(args[3]);
                uri = uriFromIndex(uriIdx);
            }

            if (uri.empty()) return 0.0;
            return DataPropertyBridge::getValue(uri, row, std::to_string(colIndex)).empty()
                       ? 0.0
                       : std::stod(DataPropertyBridge::getValue(uri, row, std::to_string(colIndex)));
        };

        registry["dataRowCount"] = [](const std::vector<double>& args) -> double {
            if (args.empty()) return 0.0;
            std::string uri = uriFromIndex(static_cast<int>(args[0]));
            if (uri.empty()) return 0.0;
            return static_cast<double>(DataPropertyBridge::getRowCount(uri));
        };

        registry["dataColCount"] = [](const std::vector<double>& args) -> double {
            if (args.empty()) return 0.0;
            std::string uri = uriFromIndex(static_cast<int>(args[0]));
            if (uri.empty()) return 0.0;
            return static_cast<double>(DataPropertyBridge::getColumnCount(uri));
        };
    }

    static void registerDataSource(const std::string& uri, int index) {
        uriMap_[index] = uri;
        DataPropertyBridge::registerDataSource(uri);
    }

private:
    static std::string uriFromIndex(int index) {
        auto it = uriMap_.find(index);
        if (it != uriMap_.end()) return it->second;
        return "";
    }

    static std::unordered_map<int, std::string> uriMap_;
};

inline std::unordered_map<int, std::string> ExpressionDataFunctions::uriMap_;

} // namespace ArtifactCore
