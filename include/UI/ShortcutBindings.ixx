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
    AnchorPointTool = 6,
    PlaybackToggle = 7,
    TimelineCopySelectedKeyframes = 8,
    TimelinePasteKeyframesAtPlayhead = 9,
    TimelineSelectAllKeyframes = 10,
    TimelineAddKeyframeAtPlayhead = 11,
    TimelineRemoveKeyframeAtPlayhead = 12,
    TimelineCleanKeyframes = 13,
    TimelineJumpToFirstKeyframe = 14,
    TimelineJumpToLastKeyframe = 15,
    TimelineJumpToNextKeyframe = 16,
    TimelineJumpToPreviousKeyframe = 17,
    LayerDeleteSelected = 18,
    TimelineZoomIn = 19,
    TimelineZoomOut = 20,
    TimelineZoomReset = 21,
    TimelineEaseIn = 22,
    TimelineEaseOut = 23,
    TimelineEaseInOut = 24,
    ImportPlacementNextSizeMode = 25,
    ImportPlacementPreviousSizeMode = 26,
    ImportPlacementConfirm = 27,
    ImportPlacementCancel = 28,
    ImportPlacementReset = 29,
    Count = 30
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
