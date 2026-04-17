module;
#include <QKeyEvent>
#include <QKeySequence>
#include <QString>
#include <cstddef>
#include <array>

export module UI.ShortcutBindings;

export namespace ArtifactCore {

enum class ShortcutId {
    Undo = 0,
    Redo = 1,
    SelectionTool = 2,
    HandTool = 3,
    ZoomTool = 4,
    RotateTool = 5,
    Count = 6
};

class ShortcutBindings {
public:
    static ShortcutBindings& instance();

    QKeySequence shortcut(ShortcutId id) const;
    void setShortcut(ShortcutId id, const QKeySequence& sequence);
    void resetToDefaults();

    bool matches(const QKeyEvent* event, ShortcutId id) const;
    QString shortcutText(ShortcutId id) const;

private:
    ShortcutBindings();

    static constexpr std::size_t index(ShortcutId id) {
        return static_cast<std::size_t>(id);
    }

    std::array<QKeySequence, static_cast<std::size_t>(ShortcutId::Count)> defaults_{};
    std::array<QKeySequence, static_cast<std::size_t>(ShortcutId::Count)> overrides_{};
};

} // namespace ArtifactCore
