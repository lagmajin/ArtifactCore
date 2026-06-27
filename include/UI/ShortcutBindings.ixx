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
    TimelineSplitLayerAtPlayhead = 19,
    TimelineZoomIn = 20,
    TimelineZoomOut = 21,
    TimelineZoomReset = 22,
    TimelineEaseIn = 23,
    TimelineEaseOut = 24,
    TimelineEaseInOut = 25,
    TimelineToggleCurveEditor = 26,
    TimelineToggleGraphMode = 27,
    TimelineSelectionTool = 28,
    TimelineHandTool = 29,
    TimelineZoomTool = 30,
    TimelineRotateTool = 31,
    TimelineSlideTool = 32,
    TimelineAddMarker = 33,
    TimelineNextMarker = 34,
    TimelinePreviousMarker = 35,
    ContentsViewerFit = 36,
    ContentsViewerReset = 37,
    ContentsViewerCompareSwap = 38,
    ContentsViewerAssignCompareA = 39,
    ContentsViewerAssignCompareB = 40,
    ContentsViewerViewer1 = 41,
    ContentsViewerViewer2 = 42,
    ContentsViewerViewer3 = 43,
    ContentsViewerViewer4 = 44,
    ProjectExpandAll = 45,
    ProjectCollapseAll = 46,
    ProjectFocusSearch = 47,
    ProjectRenameSelected = 48,
    ProjectDeleteSelected = 49,
    AnimationAddKeyframe = 50,
    AnimationRemoveKeyframe = 51,
    AnimationSelectAllKeyframes = 52,
    AnimationCopyKeyframes = 53,
    AnimationPasteKeyframes = 54,
    AnimationLinearInterpolation = 55,
    AnimationHoldInterpolation = 56,
    AnimationBezierInterpolation = 57,
    AnimationEaseIn = 58,
    AnimationEaseOut = 59,
    AnimationEaseInOut = 60,
    AnimationShowGraphEditor = 61,
    AnimationToggleValueGraph = 62,
    AnimationToggleVelocityGraph = 63,
    AnimationGoToNextKeyframe = 64,
    AnimationGoToPreviousKeyframe = 65,
    AnimationGoToFirstKeyframe = 66,
    AnimationGoToLastKeyframe = 67,
    EffectShowInspector = 68,
    CompositionCreate = 69,
    CompositionColor = 70,
    ViewZoomIn = 71,
    ViewZoomOut = 72,
    ViewDefaultZoom = 73,
    ViewFitToScreen = 74,
    ViewShowGrid = 75,
    ViewSnapToGrid = 76,
    ViewShowGuides = 77,
    ViewSnapToGuides = 78,
    ViewShowRulers = 79,
    ViewSecondaryPreview = 80,
    RenderAddCurrentToQueue = 81,
    RenderShowQueue = 82,
    RenderShowManager = 83,
    RenderSettings = 84,
    RenderStart = 85,
    LayerCreateSolid = 86,
    LayerCreateNull = 87,
    LayerCreateAdjust = 88,
    LayerCreateText = 89,
    LayerCreateLayerCycleForward = 90,
    LayerCreateLayerCycleReverse = 91,
    LayerCreateShapeCycleForward = 92,
    LayerCreateShapeCycleReverse = 93,
    LayerDuplicate = 94,
    LayerRename = 95,
    LayerDelete = 96,
    LayerBringToFront = 97,
    LayerBringForward = 98,
    LayerSendBackward = 99,
    LayerSendToBack = 100,
    LayerAlignLeft = 101,
    LayerAlignHCenter = 102,
    LayerAlignRight = 103,
    LayerAlignTop = 104,
    LayerAlignVCenter = 105,
    LayerAlignBottom = 106,
    LayerDistributeHCenter = 107,
    LayerDistributeVCenter = 108,
    LayerDistributeSpacing = 109,
    ImportPlacementNextSizeMode = 110,
    ImportPlacementPreviousSizeMode = 111,
    ImportPlacementConfirm = 112,
    ImportPlacementCancel = 113,
    ImportPlacementReset = 114,
    Count = 115
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
