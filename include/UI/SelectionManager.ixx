module;
#include <QObject>
#include <vector>
#include <QString>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include "../Define/DllExportMacro.hpp"

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
export module UI.SelectionManager;

import Utils.Id;
import Event.Bus;

export namespace ArtifactCore {

struct SelectionManagerActiveCompositionChangedEvent {
    Id id;
};

struct SelectionManagerLayerSelectionChangedEvent {
    std::vector<Id> ids;
};

struct SelectionManagerAssetSelectionChangedEvent {
    std::vector<Id> ids;
};

/**
 * @brief Singleton to manage global selection state.
 * Syncs between Project View, Timeline, and Inspector.
 */
class LIBRARY_DLL_API SelectionManager : public QObject {
    W_OBJECT(SelectionManager)
private:
    Id activeCompositionId_;
    std::vector<Id> selectedLayerIds_;
    std::vector<Id> selectedAssetIds_;

public:
    static SelectionManager& instance() {
        static SelectionManager s;
        return s;
    }

    // Composition
    void setActiveComposition(const Id& id) {
        if (activeCompositionId_ != id) {
            activeCompositionId_ = id;
            globalEventBus().publish<
                SelectionManagerActiveCompositionChangedEvent>({id});
        }
    }
    Id activeComposition() const { return activeCompositionId_; }

    // Layers
    void selectLayers(const std::vector<Id>& ids) {
        selectedLayerIds_ = ids;
        globalEventBus().publish<SelectionManagerLayerSelectionChangedEvent>(
            {ids});
    }
    void clearLayerSelection() {
        selectedLayerIds_.clear();
        globalEventBus().publish<SelectionManagerLayerSelectionChangedEvent>(
            {std::vector<Id>{}});
    }
    const std::vector<Id>& selectedLayers() const { return selectedLayerIds_; }
    bool isLayerSelected(const Id& id) const {
        return std::find(selectedLayerIds_.begin(), selectedLayerIds_.end(), id) != selectedLayerIds_.end();
    }

    // Assets
    void selectAssets(const std::vector<Id>& ids) {
        selectedAssetIds_ = ids;
        globalEventBus().publish<SelectionManagerAssetSelectionChangedEvent>(
            {ids});
    }
    const std::vector<Id>& selectedAssets() const { return selectedAssetIds_; }

};

W_OBJECT_IMPL(SelectionManager)

}
