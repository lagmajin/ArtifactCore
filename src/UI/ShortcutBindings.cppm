module;
#include <QKeySequence>
#include <QKeyEvent>
#include <QJsonObject>
#include <QString>

module UI.ShortcutBindings;

namespace ArtifactCore {

namespace {

bool hasOverride(const QKeySequence& sequence)
{
    return !sequence.isEmpty();
}

QKeySequence eventSequence(const QKeyEvent* event)
{
    if (!event) {
        return {};
    }

    return QKeySequence(event->key() | event->modifiers());
}

QString shortcutIdKey(ShortcutId id)
{
    switch (id) {
    case ShortcutId::Undo:
        return QStringLiteral("Undo");
    case ShortcutId::Redo:
        return QStringLiteral("Redo");
    case ShortcutId::SelectionTool:
        return QStringLiteral("SelectionTool");
    case ShortcutId::HandTool:
        return QStringLiteral("HandTool");
    case ShortcutId::ZoomTool:
        return QStringLiteral("ZoomTool");
    case ShortcutId::RotateTool:
        return QStringLiteral("RotateTool");
    case ShortcutId::AnchorPointTool:
        return QStringLiteral("AnchorPointTool");
    case ShortcutId::PlaybackToggle:
        return QStringLiteral("PlaybackToggle");
    case ShortcutId::TimelineCopySelectedKeyframes:
        return QStringLiteral("TimelineCopySelectedKeyframes");
    case ShortcutId::TimelinePasteKeyframesAtPlayhead:
        return QStringLiteral("TimelinePasteKeyframesAtPlayhead");
    case ShortcutId::TimelineSelectAllKeyframes:
        return QStringLiteral("TimelineSelectAllKeyframes");
    case ShortcutId::TimelineAddKeyframeAtPlayhead:
        return QStringLiteral("TimelineAddKeyframeAtPlayhead");
    case ShortcutId::TimelineRemoveKeyframeAtPlayhead:
        return QStringLiteral("TimelineRemoveKeyframeAtPlayhead");
    case ShortcutId::TimelineCleanKeyframes:
        return QStringLiteral("TimelineCleanKeyframes");
    case ShortcutId::TimelineJumpToFirstKeyframe:
        return QStringLiteral("TimelineJumpToFirstKeyframe");
    case ShortcutId::TimelineJumpToLastKeyframe:
        return QStringLiteral("TimelineJumpToLastKeyframe");
    case ShortcutId::TimelineJumpToNextKeyframe:
        return QStringLiteral("TimelineJumpToNextKeyframe");
    case ShortcutId::TimelineJumpToPreviousKeyframe:
        return QStringLiteral("TimelineJumpToPreviousKeyframe");
    case ShortcutId::LayerDeleteSelected:
        return QStringLiteral("LayerDeleteSelected");
    case ShortcutId::TimelineSplitLayerAtPlayhead:
        return QStringLiteral("TimelineSplitLayerAtPlayhead");
    case ShortcutId::TimelineZoomIn:
        return QStringLiteral("TimelineZoomIn");
    case ShortcutId::TimelineZoomOut:
        return QStringLiteral("TimelineZoomOut");
    case ShortcutId::TimelineZoomReset:
        return QStringLiteral("TimelineZoomReset");
    case ShortcutId::TimelineEaseIn:
        return QStringLiteral("TimelineEaseIn");
    case ShortcutId::TimelineEaseOut:
        return QStringLiteral("TimelineEaseOut");
    case ShortcutId::TimelineEaseInOut:
        return QStringLiteral("TimelineEaseInOut");
    case ShortcutId::TimelineToggleCurveEditor:
        return QStringLiteral("TimelineToggleCurveEditor");
    case ShortcutId::TimelineToggleGraphMode:
        return QStringLiteral("TimelineToggleGraphMode");
    case ShortcutId::TimelineSelectionTool:
        return QStringLiteral("TimelineSelectionTool");
    case ShortcutId::TimelineHandTool:
        return QStringLiteral("TimelineHandTool");
    case ShortcutId::TimelineZoomTool:
        return QStringLiteral("TimelineZoomTool");
    case ShortcutId::TimelineRotateTool:
        return QStringLiteral("TimelineRotateTool");
    case ShortcutId::TimelineSlideTool:
        return QStringLiteral("TimelineSlideTool");
    case ShortcutId::TimelineAddMarker:
        return QStringLiteral("TimelineAddMarker");
    case ShortcutId::TimelineNextMarker:
        return QStringLiteral("TimelineNextMarker");
    case ShortcutId::TimelinePreviousMarker:
        return QStringLiteral("TimelinePreviousMarker");
    case ShortcutId::ContentsViewerFit:
        return QStringLiteral("ContentsViewerFit");
    case ShortcutId::ContentsViewerReset:
        return QStringLiteral("ContentsViewerReset");
    case ShortcutId::ContentsViewerCompareSwap:
        return QStringLiteral("ContentsViewerCompareSwap");
    case ShortcutId::ContentsViewerAssignCompareA:
        return QStringLiteral("ContentsViewerAssignCompareA");
    case ShortcutId::ContentsViewerAssignCompareB:
        return QStringLiteral("ContentsViewerAssignCompareB");
    case ShortcutId::ContentsViewerViewer1:
        return QStringLiteral("ContentsViewerViewer1");
    case ShortcutId::ContentsViewerViewer2:
        return QStringLiteral("ContentsViewerViewer2");
    case ShortcutId::ContentsViewerViewer3:
        return QStringLiteral("ContentsViewerViewer3");
    case ShortcutId::ContentsViewerViewer4:
        return QStringLiteral("ContentsViewerViewer4");
    case ShortcutId::ProjectExpandAll:
        return QStringLiteral("ProjectExpandAll");
    case ShortcutId::ProjectCollapseAll:
        return QStringLiteral("ProjectCollapseAll");
    case ShortcutId::ProjectFocusSearch:
        return QStringLiteral("ProjectFocusSearch");
    case ShortcutId::ProjectRenameSelected:
        return QStringLiteral("ProjectRenameSelected");
    case ShortcutId::ProjectDeleteSelected:
        return QStringLiteral("ProjectDeleteSelected");
    case ShortcutId::AnimationAddKeyframe:
        return QStringLiteral("AnimationAddKeyframe");
    case ShortcutId::AnimationRemoveKeyframe:
        return QStringLiteral("AnimationRemoveKeyframe");
    case ShortcutId::AnimationSelectAllKeyframes:
        return QStringLiteral("AnimationSelectAllKeyframes");
    case ShortcutId::AnimationCopyKeyframes:
        return QStringLiteral("AnimationCopyKeyframes");
    case ShortcutId::AnimationPasteKeyframes:
        return QStringLiteral("AnimationPasteKeyframes");
    case ShortcutId::AnimationLinearInterpolation:
        return QStringLiteral("AnimationLinearInterpolation");
    case ShortcutId::AnimationHoldInterpolation:
        return QStringLiteral("AnimationHoldInterpolation");
    case ShortcutId::AnimationBezierInterpolation:
        return QStringLiteral("AnimationBezierInterpolation");
    case ShortcutId::AnimationEaseIn:
        return QStringLiteral("AnimationEaseIn");
    case ShortcutId::AnimationEaseOut:
        return QStringLiteral("AnimationEaseOut");
    case ShortcutId::AnimationEaseInOut:
        return QStringLiteral("AnimationEaseInOut");
    case ShortcutId::AnimationShowGraphEditor:
        return QStringLiteral("AnimationShowGraphEditor");
    case ShortcutId::AnimationToggleValueGraph:
        return QStringLiteral("AnimationToggleValueGraph");
    case ShortcutId::AnimationToggleVelocityGraph:
        return QStringLiteral("AnimationToggleVelocityGraph");
    case ShortcutId::AnimationGoToNextKeyframe:
        return QStringLiteral("AnimationGoToNextKeyframe");
    case ShortcutId::AnimationGoToPreviousKeyframe:
        return QStringLiteral("AnimationGoToPreviousKeyframe");
    case ShortcutId::AnimationGoToFirstKeyframe:
        return QStringLiteral("AnimationGoToFirstKeyframe");
    case ShortcutId::AnimationGoToLastKeyframe:
        return QStringLiteral("AnimationGoToLastKeyframe");
    case ShortcutId::EffectShowInspector:
        return QStringLiteral("EffectShowInspector");
    case ShortcutId::CompositionCreate:
        return QStringLiteral("CompositionCreate");
    case ShortcutId::CompositionColor:
        return QStringLiteral("CompositionColor");
    case ShortcutId::ViewZoomIn:
        return QStringLiteral("ViewZoomIn");
    case ShortcutId::ViewZoomOut:
        return QStringLiteral("ViewZoomOut");
    case ShortcutId::ViewDefaultZoom:
        return QStringLiteral("ViewDefaultZoom");
    case ShortcutId::ViewFitToScreen:
        return QStringLiteral("ViewFitToScreen");
    case ShortcutId::ViewShowGrid:
        return QStringLiteral("ViewShowGrid");
    case ShortcutId::ViewSnapToGrid:
        return QStringLiteral("ViewSnapToGrid");
    case ShortcutId::ViewShowGuides:
        return QStringLiteral("ViewShowGuides");
    case ShortcutId::ViewSnapToGuides:
        return QStringLiteral("ViewSnapToGuides");
    case ShortcutId::ViewShowRulers:
        return QStringLiteral("ViewShowRulers");
    case ShortcutId::ViewSecondaryPreview:
        return QStringLiteral("ViewSecondaryPreview");
    case ShortcutId::RenderAddCurrentToQueue:
        return QStringLiteral("RenderAddCurrentToQueue");
    case ShortcutId::RenderShowQueue:
        return QStringLiteral("RenderShowQueue");
    case ShortcutId::RenderShowManager:
        return QStringLiteral("RenderShowManager");
    case ShortcutId::RenderSettings:
        return QStringLiteral("RenderSettings");
    case ShortcutId::RenderStart:
        return QStringLiteral("RenderStart");
    case ShortcutId::LayerCreateSolid:
        return QStringLiteral("LayerCreateSolid");
    case ShortcutId::LayerCreateNull:
        return QStringLiteral("LayerCreateNull");
    case ShortcutId::LayerCreateAdjust:
        return QStringLiteral("LayerCreateAdjust");
    case ShortcutId::LayerCreateText:
        return QStringLiteral("LayerCreateText");
    case ShortcutId::LayerCreateLayerCycleForward:
        return QStringLiteral("LayerCreateLayerCycleForward");
    case ShortcutId::LayerCreateLayerCycleReverse:
        return QStringLiteral("LayerCreateLayerCycleReverse");
    case ShortcutId::LayerCreateShapeCycleForward:
        return QStringLiteral("LayerCreateShapeCycleForward");
    case ShortcutId::LayerCreateShapeCycleReverse:
        return QStringLiteral("LayerCreateShapeCycleReverse");
    case ShortcutId::LayerDuplicate:
        return QStringLiteral("LayerDuplicate");
    case ShortcutId::LayerRename:
        return QStringLiteral("LayerRename");
    case ShortcutId::LayerDelete:
        return QStringLiteral("LayerDelete");
    case ShortcutId::LayerBringToFront:
        return QStringLiteral("LayerBringToFront");
    case ShortcutId::LayerBringForward:
        return QStringLiteral("LayerBringForward");
    case ShortcutId::LayerSendBackward:
        return QStringLiteral("LayerSendBackward");
    case ShortcutId::LayerSendToBack:
        return QStringLiteral("LayerSendToBack");
    case ShortcutId::LayerAlignLeft:
        return QStringLiteral("LayerAlignLeft");
    case ShortcutId::LayerAlignHCenter:
        return QStringLiteral("LayerAlignHCenter");
    case ShortcutId::LayerAlignRight:
        return QStringLiteral("LayerAlignRight");
    case ShortcutId::LayerAlignTop:
        return QStringLiteral("LayerAlignTop");
    case ShortcutId::LayerAlignVCenter:
        return QStringLiteral("LayerAlignVCenter");
    case ShortcutId::LayerAlignBottom:
        return QStringLiteral("LayerAlignBottom");
    case ShortcutId::LayerDistributeHCenter:
        return QStringLiteral("LayerDistributeHCenter");
    case ShortcutId::LayerDistributeVCenter:
        return QStringLiteral("LayerDistributeVCenter");
    case ShortcutId::LayerDistributeSpacing:
        return QStringLiteral("LayerDistributeSpacing");
    case ShortcutId::ImportPlacementNextSizeMode:
        return QStringLiteral("ImportPlacementNextSizeMode");
    case ShortcutId::ImportPlacementPreviousSizeMode:
        return QStringLiteral("ImportPlacementPreviousSizeMode");
    case ShortcutId::ImportPlacementConfirm:
        return QStringLiteral("ImportPlacementConfirm");
    case ShortcutId::ImportPlacementCancel:
        return QStringLiteral("ImportPlacementCancel");
    case ShortcutId::ImportPlacementReset:
        return QStringLiteral("ImportPlacementReset");
    case ShortcutId::Count:
        break;
    }
    return QStringLiteral("Shortcut");
}

} // namespace

ShortcutBindings::ShortcutBindings()
{
    resetToDefaults();
}

ShortcutBindings& ShortcutBindings::instance()
{
    static ShortcutBindings bindings;
    return bindings;
}

QString shortcutDisplayName(ShortcutId id)
{
    switch (id) {
    case ShortcutId::Undo:
        return QStringLiteral("Undo");
    case ShortcutId::Redo:
        return QStringLiteral("Redo");
    case ShortcutId::SelectionTool:
        return QStringLiteral("Selection Tool");
    case ShortcutId::HandTool:
        return QStringLiteral("Hand Tool");
    case ShortcutId::ZoomTool:
        return QStringLiteral("Zoom Tool");
    case ShortcutId::RotateTool:
        return QStringLiteral("Rotate Tool");
    case ShortcutId::AnchorPointTool:
        return QStringLiteral("Anchor Tool");
    case ShortcutId::PlaybackToggle:
        return QStringLiteral("Playback Toggle");
    case ShortcutId::TimelineCopySelectedKeyframes:
        return QStringLiteral("Timeline Copy Selected Keyframes");
    case ShortcutId::TimelinePasteKeyframesAtPlayhead:
        return QStringLiteral("Timeline Paste Keyframes at Playhead");
    case ShortcutId::TimelineSelectAllKeyframes:
        return QStringLiteral("Timeline Select All Keyframes");
    case ShortcutId::TimelineAddKeyframeAtPlayhead:
        return QStringLiteral("Timeline Add Keyframe at Playhead");
    case ShortcutId::TimelineRemoveKeyframeAtPlayhead:
        return QStringLiteral("Timeline Remove Keyframe at Playhead");
    case ShortcutId::TimelineCleanKeyframes:
        return QStringLiteral("Timeline Clean Keyframes");
    case ShortcutId::TimelineJumpToFirstKeyframe:
        return QStringLiteral("Timeline Jump to First Keyframe");
    case ShortcutId::TimelineJumpToLastKeyframe:
        return QStringLiteral("Timeline Jump to Last Keyframe");
    case ShortcutId::TimelineJumpToNextKeyframe:
        return QStringLiteral("Timeline Jump to Next Keyframe");
    case ShortcutId::TimelineJumpToPreviousKeyframe:
        return QStringLiteral("Timeline Jump to Previous Keyframe");
    case ShortcutId::LayerDeleteSelected:
        return QStringLiteral("Delete Selected Layers");
    case ShortcutId::TimelineSplitLayerAtPlayhead:
        return QStringLiteral("Split Layer at Playhead");
    case ShortcutId::TimelineZoomIn:
        return QStringLiteral("Timeline Zoom In");
    case ShortcutId::TimelineZoomOut:
        return QStringLiteral("Timeline Zoom Out");
    case ShortcutId::TimelineZoomReset:
        return QStringLiteral("Timeline Zoom Reset");
    case ShortcutId::TimelineEaseIn:
        return QStringLiteral("Timeline Ease In");
    case ShortcutId::TimelineEaseOut:
        return QStringLiteral("Timeline Ease Out");
    case ShortcutId::TimelineEaseInOut:
        return QStringLiteral("Timeline Ease In/Out");
    case ShortcutId::TimelineToggleCurveEditor:
        return QStringLiteral("Timeline Toggle Curve Editor");
    case ShortcutId::TimelineToggleGraphMode:
        return QStringLiteral("Timeline Toggle Graph Mode");
    case ShortcutId::TimelineSelectionTool:
        return QStringLiteral("Timeline Selection Tool");
    case ShortcutId::TimelineHandTool:
        return QStringLiteral("Timeline Hand Tool");
    case ShortcutId::TimelineZoomTool:
        return QStringLiteral("Timeline Zoom Tool");
    case ShortcutId::TimelineRotateTool:
        return QStringLiteral("Timeline Rotate Tool");
    case ShortcutId::TimelineSlideTool:
        return QStringLiteral("Timeline Slide Tool");
    case ShortcutId::TimelineAddMarker:
        return QStringLiteral("Timeline Add Marker");
    case ShortcutId::TimelineNextMarker:
        return QStringLiteral("Timeline Next Marker");
    case ShortcutId::TimelinePreviousMarker:
        return QStringLiteral("Timeline Previous Marker");
    case ShortcutId::ContentsViewerFit:
        return QStringLiteral("Contents Viewer Fit");
    case ShortcutId::ContentsViewerReset:
        return QStringLiteral("Contents Viewer Reset");
    case ShortcutId::ContentsViewerCompareSwap:
        return QStringLiteral("Contents Viewer Compare Swap");
    case ShortcutId::ContentsViewerAssignCompareA:
        return QStringLiteral("Contents Viewer Assign Compare A");
    case ShortcutId::ContentsViewerAssignCompareB:
        return QStringLiteral("Contents Viewer Assign Compare B");
    case ShortcutId::ContentsViewerViewer1:
        return QStringLiteral("Contents Viewer 1");
    case ShortcutId::ContentsViewerViewer2:
        return QStringLiteral("Contents Viewer 2");
    case ShortcutId::ContentsViewerViewer3:
        return QStringLiteral("Contents Viewer 3");
    case ShortcutId::ContentsViewerViewer4:
        return QStringLiteral("Contents Viewer 4");
    case ShortcutId::ProjectExpandAll:
        return QStringLiteral("Project Expand All");
    case ShortcutId::ProjectCollapseAll:
        return QStringLiteral("Project Collapse All");
    case ShortcutId::ProjectFocusSearch:
        return QStringLiteral("Project Focus Search");
    case ShortcutId::ProjectRenameSelected:
        return QStringLiteral("Project Rename Selected");
    case ShortcutId::ProjectDeleteSelected:
        return QStringLiteral("Project Delete Selected");
    case ShortcutId::AnimationAddKeyframe:
        return QStringLiteral("Animation Add Keyframe");
    case ShortcutId::AnimationRemoveKeyframe:
        return QStringLiteral("Animation Remove Keyframe");
    case ShortcutId::AnimationSelectAllKeyframes:
        return QStringLiteral("Animation Select All Keyframes");
    case ShortcutId::AnimationCopyKeyframes:
        return QStringLiteral("Animation Copy Keyframes");
    case ShortcutId::AnimationPasteKeyframes:
        return QStringLiteral("Animation Paste Keyframes");
    case ShortcutId::AnimationLinearInterpolation:
        return QStringLiteral("Animation Linear Interpolation");
    case ShortcutId::AnimationHoldInterpolation:
        return QStringLiteral("Animation Hold Interpolation");
    case ShortcutId::AnimationBezierInterpolation:
        return QStringLiteral("Animation Bezier Interpolation");
    case ShortcutId::AnimationEaseIn:
        return QStringLiteral("Animation Ease In");
    case ShortcutId::AnimationEaseOut:
        return QStringLiteral("Animation Ease Out");
    case ShortcutId::AnimationEaseInOut:
        return QStringLiteral("Animation Ease In/Out");
    case ShortcutId::AnimationShowGraphEditor:
        return QStringLiteral("Animation Show Graph Editor");
    case ShortcutId::AnimationToggleValueGraph:
        return QStringLiteral("Animation Toggle Value Graph");
    case ShortcutId::AnimationToggleVelocityGraph:
        return QStringLiteral("Animation Toggle Velocity Graph");
    case ShortcutId::AnimationGoToNextKeyframe:
        return QStringLiteral("Animation Go To Next Keyframe");
    case ShortcutId::AnimationGoToPreviousKeyframe:
        return QStringLiteral("Animation Go To Previous Keyframe");
    case ShortcutId::AnimationGoToFirstKeyframe:
        return QStringLiteral("Animation Go To First Keyframe");
    case ShortcutId::AnimationGoToLastKeyframe:
        return QStringLiteral("Animation Go To Last Keyframe");
    case ShortcutId::EffectShowInspector:
        return QStringLiteral("Effect Show Inspector");
    case ShortcutId::CompositionCreate:
        return QStringLiteral("Composition Create");
    case ShortcutId::CompositionColor:
        return QStringLiteral("Composition Color");
    case ShortcutId::ViewZoomIn:
        return QStringLiteral("View Zoom In");
    case ShortcutId::ViewZoomOut:
        return QStringLiteral("View Zoom Out");
    case ShortcutId::ViewDefaultZoom:
        return QStringLiteral("View Default Zoom");
    case ShortcutId::ViewFitToScreen:
        return QStringLiteral("View Fit To Screen");
    case ShortcutId::ViewShowGrid:
        return QStringLiteral("View Show Grid");
    case ShortcutId::ViewSnapToGrid:
        return QStringLiteral("View Snap To Grid");
    case ShortcutId::ViewShowGuides:
        return QStringLiteral("View Show Guides");
    case ShortcutId::ViewSnapToGuides:
        return QStringLiteral("View Snap To Guides");
    case ShortcutId::ViewShowRulers:
        return QStringLiteral("View Show Rulers");
    case ShortcutId::ViewSecondaryPreview:
        return QStringLiteral("View Secondary Preview");
    case ShortcutId::RenderAddCurrentToQueue:
        return QStringLiteral("Render Add Current To Queue");
    case ShortcutId::RenderShowQueue:
        return QStringLiteral("Render Show Queue");
    case ShortcutId::RenderShowManager:
        return QStringLiteral("Render Show Manager");
    case ShortcutId::RenderSettings:
        return QStringLiteral("Render Settings");
    case ShortcutId::RenderStart:
        return QStringLiteral("Render Start");
    case ShortcutId::LayerCreateSolid:
        return QStringLiteral("Layer Create Solid");
    case ShortcutId::LayerCreateNull:
        return QStringLiteral("Layer Create Null");
    case ShortcutId::LayerCreateAdjust:
        return QStringLiteral("Layer Create Adjust");
    case ShortcutId::LayerCreateText:
        return QStringLiteral("Layer Create Text");
    case ShortcutId::LayerCreateLayerCycleForward:
        return QStringLiteral("Layer Create Layer Cycle Forward");
    case ShortcutId::LayerCreateLayerCycleReverse:
        return QStringLiteral("Layer Create Layer Cycle Reverse");
    case ShortcutId::LayerCreateShapeCycleForward:
        return QStringLiteral("Layer Create Shape Cycle Forward");
    case ShortcutId::LayerCreateShapeCycleReverse:
        return QStringLiteral("Layer Create Shape Cycle Reverse");
    case ShortcutId::LayerDuplicate:
        return QStringLiteral("Layer Duplicate");
    case ShortcutId::LayerRename:
        return QStringLiteral("Layer Rename");
    case ShortcutId::LayerDelete:
        return QStringLiteral("Layer Delete");
    case ShortcutId::LayerBringToFront:
        return QStringLiteral("Layer Bring To Front");
    case ShortcutId::LayerBringForward:
        return QStringLiteral("Layer Bring Forward");
    case ShortcutId::LayerSendBackward:
        return QStringLiteral("Layer Send Backward");
    case ShortcutId::LayerSendToBack:
        return QStringLiteral("Layer Send To Back");
    case ShortcutId::LayerAlignLeft:
        return QStringLiteral("Layer Align Left");
    case ShortcutId::LayerAlignHCenter:
        return QStringLiteral("Layer Align HCenter");
    case ShortcutId::LayerAlignRight:
        return QStringLiteral("Layer Align Right");
    case ShortcutId::LayerAlignTop:
        return QStringLiteral("Layer Align Top");
    case ShortcutId::LayerAlignVCenter:
        return QStringLiteral("Layer Align VCenter");
    case ShortcutId::LayerAlignBottom:
        return QStringLiteral("Layer Align Bottom");
    case ShortcutId::LayerDistributeHCenter:
        return QStringLiteral("Layer Distribute HCenter");
    case ShortcutId::LayerDistributeVCenter:
        return QStringLiteral("Layer Distribute VCenter");
    case ShortcutId::LayerDistributeSpacing:
        return QStringLiteral("Layer Distribute Spacing");
    case ShortcutId::ImportPlacementNextSizeMode:
        return QStringLiteral("Import Placement Next Size Mode");
    case ShortcutId::ImportPlacementPreviousSizeMode:
        return QStringLiteral("Import Placement Previous Size Mode");
    case ShortcutId::ImportPlacementConfirm:
        return QStringLiteral("Import Placement Confirm");
    case ShortcutId::ImportPlacementCancel:
        return QStringLiteral("Import Placement Cancel");
    case ShortcutId::ImportPlacementReset:
        return QStringLiteral("Import Placement Reset");
    case ShortcutId::Count:
        break;
    }
    return QStringLiteral("Shortcut");
}

std::array<ShortcutId, static_cast<std::size_t>(ShortcutId::Count)> allShortcutIds()
{
    return {
        ShortcutId::Undo,
        ShortcutId::Redo,
        ShortcutId::SelectionTool,
        ShortcutId::HandTool,
        ShortcutId::ZoomTool,
        ShortcutId::RotateTool,
        ShortcutId::AnchorPointTool,
        ShortcutId::PlaybackToggle,
        ShortcutId::TimelineCopySelectedKeyframes,
        ShortcutId::TimelinePasteKeyframesAtPlayhead,
        ShortcutId::TimelineSelectAllKeyframes,
        ShortcutId::TimelineAddKeyframeAtPlayhead,
        ShortcutId::TimelineRemoveKeyframeAtPlayhead,
        ShortcutId::TimelineCleanKeyframes,
        ShortcutId::TimelineJumpToFirstKeyframe,
        ShortcutId::TimelineJumpToLastKeyframe,
        ShortcutId::TimelineJumpToNextKeyframe,
        ShortcutId::TimelineJumpToPreviousKeyframe,
        ShortcutId::LayerDeleteSelected,
        ShortcutId::TimelineSplitLayerAtPlayhead,
        ShortcutId::TimelineZoomIn,
        ShortcutId::TimelineZoomOut,
        ShortcutId::TimelineZoomReset,
        ShortcutId::TimelineEaseIn,
        ShortcutId::TimelineEaseOut,
        ShortcutId::TimelineEaseInOut,
        ShortcutId::TimelineToggleCurveEditor,
        ShortcutId::TimelineToggleGraphMode,
        ShortcutId::TimelineSelectionTool,
        ShortcutId::TimelineHandTool,
        ShortcutId::TimelineZoomTool,
        ShortcutId::TimelineRotateTool,
        ShortcutId::TimelineSlideTool,
        ShortcutId::TimelineAddMarker,
        ShortcutId::TimelineNextMarker,
        ShortcutId::TimelinePreviousMarker,
        ShortcutId::ContentsViewerFit,
        ShortcutId::ContentsViewerReset,
        ShortcutId::ContentsViewerCompareSwap,
        ShortcutId::ContentsViewerAssignCompareA,
        ShortcutId::ContentsViewerAssignCompareB,
        ShortcutId::ContentsViewerViewer1,
        ShortcutId::ContentsViewerViewer2,
        ShortcutId::ContentsViewerViewer3,
        ShortcutId::ContentsViewerViewer4,
        ShortcutId::ProjectExpandAll,
        ShortcutId::ProjectCollapseAll,
        ShortcutId::ProjectFocusSearch,
        ShortcutId::ProjectRenameSelected,
        ShortcutId::ProjectDeleteSelected,
        ShortcutId::AnimationAddKeyframe,
        ShortcutId::AnimationRemoveKeyframe,
        ShortcutId::AnimationSelectAllKeyframes,
        ShortcutId::AnimationCopyKeyframes,
        ShortcutId::AnimationPasteKeyframes,
        ShortcutId::AnimationLinearInterpolation,
        ShortcutId::AnimationHoldInterpolation,
        ShortcutId::AnimationBezierInterpolation,
        ShortcutId::AnimationEaseIn,
        ShortcutId::AnimationEaseOut,
        ShortcutId::AnimationEaseInOut,
        ShortcutId::AnimationShowGraphEditor,
        ShortcutId::AnimationToggleValueGraph,
        ShortcutId::AnimationToggleVelocityGraph,
        ShortcutId::AnimationGoToNextKeyframe,
        ShortcutId::AnimationGoToPreviousKeyframe,
        ShortcutId::AnimationGoToFirstKeyframe,
        ShortcutId::AnimationGoToLastKeyframe,
        ShortcutId::EffectShowInspector,
        ShortcutId::CompositionCreate,
        ShortcutId::CompositionColor,
        ShortcutId::ViewZoomIn,
        ShortcutId::ViewZoomOut,
        ShortcutId::ViewDefaultZoom,
        ShortcutId::ViewFitToScreen,
        ShortcutId::ViewShowGrid,
        ShortcutId::ViewSnapToGrid,
        ShortcutId::ViewShowGuides,
        ShortcutId::ViewSnapToGuides,
        ShortcutId::ViewShowRulers,
        ShortcutId::ViewSecondaryPreview,
        ShortcutId::RenderAddCurrentToQueue,
        ShortcutId::RenderShowQueue,
        ShortcutId::RenderShowManager,
        ShortcutId::RenderSettings,
        ShortcutId::RenderStart,
        ShortcutId::LayerCreateSolid,
        ShortcutId::LayerCreateNull,
        ShortcutId::LayerCreateAdjust,
        ShortcutId::LayerCreateText,
        ShortcutId::LayerCreateLayerCycleForward,
        ShortcutId::LayerCreateLayerCycleReverse,
        ShortcutId::LayerCreateShapeCycleForward,
        ShortcutId::LayerCreateShapeCycleReverse,
        ShortcutId::LayerDuplicate,
        ShortcutId::LayerRename,
        ShortcutId::LayerDelete,
        ShortcutId::LayerBringToFront,
        ShortcutId::LayerBringForward,
        ShortcutId::LayerSendBackward,
        ShortcutId::LayerSendToBack,
        ShortcutId::LayerAlignLeft,
        ShortcutId::LayerAlignHCenter,
        ShortcutId::LayerAlignRight,
        ShortcutId::LayerAlignTop,
        ShortcutId::LayerAlignVCenter,
        ShortcutId::LayerAlignBottom,
        ShortcutId::LayerDistributeHCenter,
        ShortcutId::LayerDistributeVCenter,
        ShortcutId::LayerDistributeSpacing,
        ShortcutId::ImportPlacementNextSizeMode,
        ShortcutId::ImportPlacementPreviousSizeMode,
        ShortcutId::ImportPlacementConfirm,
        ShortcutId::ImportPlacementCancel,
        ShortcutId::ImportPlacementReset,
    };
}

void ShortcutBindings::resetToDefaults()
{
    defaults_[index(ShortcutId::Undo)] = QKeySequence::Undo;
    defaults_[index(ShortcutId::Redo)] = QKeySequence::Redo;
    defaults_[index(ShortcutId::SelectionTool)] = QKeySequence(Qt::Key_V);
    defaults_[index(ShortcutId::HandTool)] = QKeySequence(Qt::Key_H);
    defaults_[index(ShortcutId::ZoomTool)] = QKeySequence(Qt::Key_Z);
    defaults_[index(ShortcutId::RotateTool)] = QKeySequence(Qt::Key_W);
    defaults_[index(ShortcutId::AnchorPointTool)] = QKeySequence(Qt::Key_Y);
    defaults_[index(ShortcutId::PlaybackToggle)] = QKeySequence(Qt::Key_Space);
    defaults_[index(ShortcutId::TimelineCopySelectedKeyframes)] = QKeySequence(Qt::CTRL | Qt::Key_C);
    defaults_[index(ShortcutId::TimelinePasteKeyframesAtPlayhead)] = QKeySequence(Qt::CTRL | Qt::Key_V);
    defaults_[index(ShortcutId::TimelineSelectAllKeyframes)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A);
    defaults_[index(ShortcutId::TimelineAddKeyframeAtPlayhead)] = QKeySequence(Qt::Key_Insert);
    defaults_[index(ShortcutId::TimelineRemoveKeyframeAtPlayhead)] = QKeySequence(Qt::Key_Backspace);
    defaults_[index(ShortcutId::TimelineCleanKeyframes)] = QKeySequence(Qt::CTRL | Qt::Key_Delete);
    defaults_[index(ShortcutId::TimelineJumpToFirstKeyframe)] = QKeySequence(Qt::CTRL | Qt::Key_Home);
    defaults_[index(ShortcutId::TimelineJumpToLastKeyframe)] = QKeySequence(Qt::CTRL | Qt::Key_End);
    defaults_[index(ShortcutId::TimelineJumpToNextKeyframe)] = QKeySequence(Qt::CTRL | Qt::Key_PageDown);
    defaults_[index(ShortcutId::TimelineJumpToPreviousKeyframe)] = QKeySequence(Qt::CTRL | Qt::Key_PageUp);
    defaults_[index(ShortcutId::LayerDeleteSelected)] = QKeySequence(QKeySequence::Delete);
    defaults_[index(ShortcutId::TimelineSplitLayerAtPlayhead)] = QKeySequence(Qt::ALT | Qt::Key_S);
    defaults_[index(ShortcutId::TimelineZoomIn)] = QKeySequence(Qt::CTRL | Qt::Key_Equal);
    defaults_[index(ShortcutId::TimelineZoomOut)] = QKeySequence(Qt::CTRL | Qt::Key_Minus);
    defaults_[index(ShortcutId::TimelineZoomReset)] = QKeySequence(Qt::CTRL | Qt::Key_0);
    defaults_[index(ShortcutId::TimelineEaseIn)] = QKeySequence(Qt::SHIFT | Qt::Key_F9);
    defaults_[index(ShortcutId::TimelineEaseOut)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9);
    defaults_[index(ShortcutId::TimelineEaseInOut)] = QKeySequence(Qt::Key_F9);
    defaults_[index(ShortcutId::TimelineToggleCurveEditor)] = QKeySequence(Qt::CTRL | Qt::Key_G);
    defaults_[index(ShortcutId::TimelineToggleGraphMode)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G);
    defaults_[index(ShortcutId::TimelineSelectionTool)] = QKeySequence(Qt::Key_V);
    defaults_[index(ShortcutId::TimelineHandTool)] = QKeySequence(Qt::Key_H);
    defaults_[index(ShortcutId::TimelineZoomTool)] = QKeySequence(Qt::Key_Z);
    defaults_[index(ShortcutId::TimelineRotateTool)] = QKeySequence(Qt::Key_R);
    defaults_[index(ShortcutId::TimelineSlideTool)] = QKeySequence(Qt::Key_S);
    defaults_[index(ShortcutId::TimelineAddMarker)] = QKeySequence(Qt::Key_M);
    defaults_[index(ShortcutId::TimelineNextMarker)] = QKeySequence(Qt::SHIFT | Qt::Key_M);
    defaults_[index(ShortcutId::TimelinePreviousMarker)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    defaults_[index(ShortcutId::ContentsViewerFit)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0);
    defaults_[index(ShortcutId::ContentsViewerReset)] = QKeySequence(Qt::CTRL | Qt::Key_0);
    defaults_[index(ShortcutId::ContentsViewerCompareSwap)] = QKeySequence(Qt::Key_Tab);
    defaults_[index(ShortcutId::ContentsViewerAssignCompareA)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    defaults_[index(ShortcutId::ContentsViewerAssignCompareB)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B);
    defaults_[index(ShortcutId::ContentsViewerViewer1)] = QKeySequence(Qt::CTRL | Qt::Key_1);
    defaults_[index(ShortcutId::ContentsViewerViewer2)] = QKeySequence(Qt::CTRL | Qt::Key_2);
    defaults_[index(ShortcutId::ContentsViewerViewer3)] = QKeySequence(Qt::CTRL | Qt::Key_3);
    defaults_[index(ShortcutId::ContentsViewerViewer4)] = QKeySequence(Qt::CTRL | Qt::Key_4);
    defaults_[index(ShortcutId::ProjectExpandAll)] = QKeySequence(Qt::Key_Asterisk);
    defaults_[index(ShortcutId::ProjectCollapseAll)] = QKeySequence(Qt::SHIFT | Qt::Key_Asterisk);
    defaults_[index(ShortcutId::ProjectFocusSearch)] = QKeySequence::Find;
    defaults_[index(ShortcutId::ProjectRenameSelected)] = QKeySequence(Qt::Key_F2);
    defaults_[index(ShortcutId::ProjectDeleteSelected)] = QKeySequence::Delete;
    defaults_[index(ShortcutId::AnimationAddKeyframe)] = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_K);
    defaults_[index(ShortcutId::AnimationRemoveKeyframe)] = QKeySequence(Qt::ALT | Qt::Key_K);
    defaults_[index(ShortcutId::AnimationSelectAllKeyframes)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A);
    defaults_[index(ShortcutId::AnimationCopyKeyframes)] = QKeySequence(Qt::CTRL | Qt::Key_C);
    defaults_[index(ShortcutId::AnimationPasteKeyframes)] = QKeySequence(Qt::CTRL | Qt::Key_V);
    defaults_[index(ShortcutId::AnimationLinearInterpolation)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_1);
    defaults_[index(ShortcutId::AnimationHoldInterpolation)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_5);
    defaults_[index(ShortcutId::AnimationBezierInterpolation)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_6);
    defaults_[index(ShortcutId::AnimationEaseIn)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2);
    defaults_[index(ShortcutId::AnimationEaseOut)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3);
    defaults_[index(ShortcutId::AnimationEaseInOut)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4);
    defaults_[index(ShortcutId::AnimationShowGraphEditor)] = QKeySequence(Qt::SHIFT | Qt::Key_F3);
    defaults_[index(ShortcutId::AnimationToggleValueGraph)] = QKeySequence(Qt::ALT | Qt::Key_G);
    defaults_[index(ShortcutId::AnimationToggleVelocityGraph)] = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_G);
    defaults_[index(ShortcutId::AnimationGoToNextKeyframe)] = QKeySequence(Qt::Key_K);
    defaults_[index(ShortcutId::AnimationGoToPreviousKeyframe)] = QKeySequence(Qt::Key_J);
    defaults_[index(ShortcutId::AnimationGoToFirstKeyframe)] = QKeySequence(Qt::SHIFT | Qt::Key_J);
    defaults_[index(ShortcutId::AnimationGoToLastKeyframe)] = QKeySequence(Qt::SHIFT | Qt::Key_K);
    defaults_[index(ShortcutId::EffectShowInspector)] = QKeySequence(Qt::Key_F3);
    defaults_[index(ShortcutId::CompositionCreate)] = QKeySequence(Qt::CTRL | Qt::Key_N);
    defaults_[index(ShortcutId::CompositionColor)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B);
    defaults_[index(ShortcutId::ViewZoomIn)] = QKeySequence(Qt::CTRL | Qt::Key_Equal);
    defaults_[index(ShortcutId::ViewZoomOut)] = QKeySequence(Qt::CTRL | Qt::Key_Minus);
    defaults_[index(ShortcutId::ViewDefaultZoom)] = QKeySequence(Qt::CTRL | Qt::Key_Slash);
    defaults_[index(ShortcutId::ViewFitToScreen)] = QKeySequence(Qt::SHIFT | Qt::Key_Slash);
    defaults_[index(ShortcutId::ViewShowGrid)] = QKeySequence(QStringLiteral("Ctrl+'"));
    defaults_[index(ShortcutId::ViewSnapToGrid)] = QKeySequence(QStringLiteral("Ctrl+Shift+'"));
    defaults_[index(ShortcutId::ViewShowGuides)] = QKeySequence(QStringLiteral("Ctrl+;"));
    defaults_[index(ShortcutId::ViewSnapToGuides)] = QKeySequence(QStringLiteral("Ctrl+Shift+;"));
    defaults_[index(ShortcutId::ViewShowRulers)] = QKeySequence(QStringLiteral("Ctrl+R"));
    defaults_[index(ShortcutId::ViewSecondaryPreview)] = QKeySequence(Qt::Key_F12);
    defaults_[index(ShortcutId::RenderAddCurrentToQueue)] = QKeySequence(Qt::CTRL | Qt::Key_M);
    defaults_[index(ShortcutId::RenderShowQueue)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R);
    defaults_[index(ShortcutId::RenderShowManager)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    defaults_[index(ShortcutId::RenderSettings)] = QKeySequence(Qt::CTRL | Qt::Key_K);
    defaults_[index(ShortcutId::RenderStart)] = QKeySequence(Qt::Key_F12);
    defaults_[index(ShortcutId::LayerCreateSolid)] = QKeySequence(Qt::CTRL | Qt::Key_Y);
    defaults_[index(ShortcutId::LayerCreateNull)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y);
    defaults_[index(ShortcutId::LayerCreateAdjust)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y);
    defaults_[index(ShortcutId::LayerCreateText)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T);
    defaults_[index(ShortcutId::LayerCreateLayerCycleForward)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N);
    defaults_[index(ShortcutId::LayerCreateLayerCycleReverse)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_N);
    defaults_[index(ShortcutId::LayerCreateShapeCycleForward)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S);
    defaults_[index(ShortcutId::LayerCreateShapeCycleReverse)] = QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S);
    defaults_[index(ShortcutId::LayerDuplicate)] = QKeySequence(Qt::CTRL | Qt::Key_D);
    defaults_[index(ShortcutId::LayerRename)] = QKeySequence(Qt::Key_F2);
    defaults_[index(ShortcutId::LayerDelete)] = QKeySequence(Qt::Key_Delete);
    defaults_[index(ShortcutId::LayerBringToFront)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight);
    defaults_[index(ShortcutId::LayerBringForward)] = QKeySequence(Qt::CTRL | Qt::Key_BracketRight);
    defaults_[index(ShortcutId::LayerSendBackward)] = QKeySequence(Qt::CTRL | Qt::Key_BracketLeft);
    defaults_[index(ShortcutId::LayerSendToBack)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft);
    defaults_[index(ShortcutId::LayerAlignLeft)] = QKeySequence(Qt::ALT | Qt::Key_1);
    defaults_[index(ShortcutId::LayerAlignHCenter)] = QKeySequence(Qt::ALT | Qt::Key_2);
    defaults_[index(ShortcutId::LayerAlignRight)] = QKeySequence(Qt::ALT | Qt::Key_3);
    defaults_[index(ShortcutId::LayerAlignTop)] = QKeySequence(Qt::ALT | Qt::Key_4);
    defaults_[index(ShortcutId::LayerAlignVCenter)] = QKeySequence(Qt::ALT | Qt::Key_5);
    defaults_[index(ShortcutId::LayerAlignBottom)] = QKeySequence(Qt::ALT | Qt::Key_6);
    defaults_[index(ShortcutId::LayerDistributeHCenter)] = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_2);
    defaults_[index(ShortcutId::LayerDistributeVCenter)] = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_5);
    defaults_[index(ShortcutId::LayerDistributeSpacing)] = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_S);
    defaults_[index(ShortcutId::ImportPlacementNextSizeMode)] = QKeySequence(Qt::Key_S);
    defaults_[index(ShortcutId::ImportPlacementPreviousSizeMode)] = QKeySequence(Qt::SHIFT | Qt::Key_S);
    defaults_[index(ShortcutId::ImportPlacementConfirm)] = QKeySequence(Qt::Key_Return);
    defaults_[index(ShortcutId::ImportPlacementCancel)] = QKeySequence(Qt::Key_Escape);
    defaults_[index(ShortcutId::ImportPlacementReset)] = QKeySequence(Qt::Key_R);

    overrides_.fill(QKeySequence());
}

QKeySequence ShortcutBindings::defaultShortcut(ShortcutId id) const
{
    return defaults_[index(id)];
}

QKeySequence ShortcutBindings::shortcut(ShortcutId id) const
{
    const auto idx = index(id);
    const QKeySequence& overrideSeq = overrides_[idx];
    if (hasOverride(overrideSeq)) {
        return overrideSeq;
    }
    return defaults_[idx];
}

void ShortcutBindings::setShortcut(ShortcutId id, const QKeySequence& sequence)
{
    overrides_[index(id)] = sequence;
}

QJsonObject ShortcutBindings::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("format"), QStringLiteral("Artifact.Shortcuts.v1"));

    QJsonObject shortcuts;
    for (const auto id : allShortcutIds()) {
        shortcuts.insert(shortcutIdKey(id), shortcut(id).toString(QKeySequence::PortableText));
    }
    json.insert(QStringLiteral("shortcuts"), shortcuts);
    return json;
}

bool ShortcutBindings::loadFromJson(const QJsonObject& json)
{
    const QJsonObject shortcuts = json.value(QStringLiteral("shortcuts")).toObject();
    bool hasRecognizedShortcut = false;
    for (const auto id : allShortcutIds()) {
        const QString key = shortcutIdKey(id);
        if (shortcuts.contains(key)) {
            hasRecognizedShortcut = true;
            break;
        }
    }

    if (!hasRecognizedShortcut) {
        return false;
    }

    resetToDefaults();

    for (const auto id : allShortcutIds()) {
        const QString key = shortcutIdKey(id);
        const auto value = shortcuts.value(key);
        if (!value.isString()) {
            continue;
        }
        const QKeySequence sequence(value.toString(), QKeySequence::PortableText);
        setShortcut(id, sequence);
    }

    return true;
}

bool ShortcutBindings::matches(const QKeyEvent* event, ShortcutId id) const
{
    if (!event || event->isAutoRepeat()) {
        return false;
    }

    const auto idx = index(id);
    const QKeySequence& overrideSeq = overrides_[idx];
    if (hasOverride(overrideSeq)) {
        return eventSequence(event) == overrideSeq;
    }

    switch (id) {
    case ShortcutId::Undo:
        return event->matches(QKeySequence::Undo);
    case ShortcutId::Redo:
        return event->matches(QKeySequence::Redo);
    default:
        return eventSequence(event) == defaults_[idx];
    }
}

QString ShortcutBindings::shortcutText(ShortcutId id) const
{
    return shortcut(id).toString(QKeySequence::NativeText);
}

} // namespace ArtifactCore
