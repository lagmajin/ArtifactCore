module;
#include <QKeyEvent>
#include <QKeySequence>
#include <QJsonObject>
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
    TimelineCopySelectedKeyframes = 6,
    TimelinePasteKeyframesAtPlayhead = 7,
    TimelineSelectAllKeyframes = 8,
    TimelineAddKeyframeAtPlayhead = 9,
    TimelineRemoveKeyframeAtPlayhead = 10,
    TimelineCleanKeyframes = 11,
    TimelineJumpToFirstKeyframe = 12,
    TimelineJumpToLastKeyframe = 13,
    TimelineJumpToNextKeyframe = 14,
    TimelineJumpToPreviousKeyframe = 15,
    LayerDeleteSelected = 16,
    TimelineZoomIn = 17,
    TimelineZoomOut = 18,
    TimelineZoomReset = 19,
    ImportPlacementNextSizeMode = 20,
    ImportPlacementPreviousSizeMode = 21,
    ImportPlacementConfirm = 22,
    ImportPlacementCancel = 23,
    ImportPlacementReset = 24,
    RepeatLastAction = 25,
    TimelineJumpToInPoint = 26,
    TimelineJumpToOutPoint = 27,
    Count = 28
};

QString shortcutDisplayName(ShortcutId id);
std::array<ShortcutId, static_cast<std::size_t>(ShortcutId::Count)> allShortcutIds();

class ShortcutBindings {
public:
    static ShortcutBindings& instance();

    QKeySequence defaultShortcut(ShortcutId id) const;
    QKeySequence shortcut(ShortcutId id) const;
    void setShortcut(ShortcutId id, const QKeySequence& sequence);
    void resetToDefaults();
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject& json);

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
