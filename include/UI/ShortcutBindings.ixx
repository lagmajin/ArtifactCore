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
    PlaybackToggle = 6,
    TimelineCopySelectedKeyframes = 7,
    TimelinePasteKeyframesAtPlayhead = 8,
    TimelineSelectAllKeyframes = 9,
    TimelineAddKeyframeAtPlayhead = 10,
    TimelineRemoveKeyframeAtPlayhead = 11,
    TimelineCleanKeyframes = 12,
    TimelineJumpToFirstKeyframe = 13,
    TimelineJumpToLastKeyframe = 14,
    TimelineJumpToNextKeyframe = 15,
    TimelineJumpToPreviousKeyframe = 16,
    LayerDeleteSelected = 17,
    TimelineZoomIn = 18,
    TimelineZoomOut = 19,
    TimelineZoomReset = 20,
    TimelineEaseIn = 21,
    TimelineEaseOut = 22,
    TimelineEaseInOut = 23,
    ImportPlacementNextSizeMode = 24,
    ImportPlacementPreviousSizeMode = 25,
    ImportPlacementConfirm = 26,
    ImportPlacementCancel = 27,
    ImportPlacementReset = 28,
    Count = 29
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
