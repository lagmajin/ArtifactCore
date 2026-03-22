module;
#include <QVariant>
#include <QString>
#include <QDebug>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module UI.StandardActions;




import Input.Operator;

export namespace ArtifactCore {

/**
 * @brief Register standard project and asset operations.
 * UI components can query these by category ("Project", "Asset") to build menus.
 */
class StandardActionRegistry {
public:
    static void registerAll() {
        auto* am = ActionManager::instance();

        // Project / File Operations
        auto* import = am->registerAction("artifact.asset.import", "Import...", "Import files to project", "Project");
        import->setIconName("qrc:/icons/import.svg");
        import->setShortcutText("Ctrl+I");
        import->setExecuteCallback([](const QVariantMap& params) {
            qDebug() << "Executing Asset Import Action";
        });

        auto* newComp = am->registerAction("artifact.comp.new", "New Composition", "Create a new composition", "Project");
        newComp->setIconName("qrc:/icons/new_comp.svg");
        newComp->setShortcutText("Ctrl+N");

        // Selected Asset Operations
        auto* del = am->registerAction("artifact.asset.delete", "Delete", "Delete selected assets", "Asset");
        del->setIconName("qrc:/icons/delete.svg");
        del->setShortcutText("Del");

        auto* rename = am->registerAction("artifact.asset.rename", "Rename", "Rename selected asset", "Asset");
        rename->setShortcutText("F2");

        auto* duplicate = am->registerAction("artifact.asset.duplicate", "Duplicate", "Duplicate selected assets", "Asset");
        duplicate->setShortcutText("Ctrl+D");

        // Layer Operations
        auto* addLayer = am->registerAction("artifact.layer.add", "Add Layer", "Add a new layer to the current composition", "Layer");
        addLayer->setIconName("qrc:/icons/add_layer.svg");
        addLayer->setShortcutText("Ctrl+Alt+Y");

        auto* delLayer = am->registerAction("artifact.layer.delete", "Delete Layer", "Delete selected layers", "Layer");
        delLayer->setShortcutText("Ctrl+Alt+Del");

        auto* moveUp = am->registerAction("artifact.layer.move_up", "Move Layer Up", "Bring layer to front", "Layer");
        moveUp->setShortcutText("Ctrl+Alt+Up");

        auto* moveDown = am->registerAction("artifact.layer.move_down", "Move Layer Down", "Send layer to back", "Layer");
        moveDown->setShortcutText("Ctrl+Alt+Down");

        auto* createFromAsset = am->registerAction("artifact.layer.create_from_asset", "Create Layer From Asset", "Generate new layer from internal asset", "Layer");
        createFromAsset->setIconName("qrc:/icons/asset_layer.svg");

        // Composition Operations
        auto* openComp = am->registerAction("artifact.comp.open", "Open Composition", "Open selected composition in viewer", "Composition");
        openComp->setIconName("qrc:/icons/open_comp.svg");
        openComp->setShortcutText("Enter");

        auto* addToRender = am->registerAction("artifact.comp.add_to_render_queue", "Add to Render Queue", "Add selected composition to render queue", "Composition");
        addToRender->setIconName("qrc:/icons/render_queue.svg");
        addToRender->setShortcutText("Ctrl+M");

        qDebug() << "Standard Actions Registered";
    }
};

}
