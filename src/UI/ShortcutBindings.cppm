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
    case ShortcutId::TimelineZoomIn:
        return QStringLiteral("TimelineZoomIn");
    case ShortcutId::TimelineZoomOut:
        return QStringLiteral("TimelineZoomOut");
    case ShortcutId::TimelineZoomReset:
        return QStringLiteral("TimelineZoomReset");
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
    case ShortcutId::RepeatLastAction:
        return QStringLiteral("RepeatLastAction");
    case ShortcutId::TimelineJumpToInPoint:
        return QStringLiteral("TimelineJumpToInPoint");
    case ShortcutId::TimelineJumpToOutPoint:
        return QStringLiteral("TimelineJumpToOutPoint");
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
    case ShortcutId::TimelineZoomIn:
        return QStringLiteral("Timeline Zoom In");
    case ShortcutId::TimelineZoomOut:
        return QStringLiteral("Timeline Zoom Out");
    case ShortcutId::TimelineZoomReset:
        return QStringLiteral("Timeline Zoom Reset");
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
    case ShortcutId::RepeatLastAction:
        return QStringLiteral("Repeat Last Action");
    case ShortcutId::TimelineJumpToInPoint:
        return QStringLiteral("Timeline Jump to In Point");
    case ShortcutId::TimelineJumpToOutPoint:
        return QStringLiteral("Timeline Jump to Out Point");
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
        ShortcutId::TimelineZoomIn,
        ShortcutId::TimelineZoomOut,
        ShortcutId::TimelineZoomReset,
        ShortcutId::ImportPlacementNextSizeMode,
        ShortcutId::ImportPlacementPreviousSizeMode,
        ShortcutId::ImportPlacementConfirm,
        ShortcutId::ImportPlacementCancel,
        ShortcutId::ImportPlacementReset,
        ShortcutId::RepeatLastAction,
        ShortcutId::TimelineJumpToInPoint,
        ShortcutId::TimelineJumpToOutPoint,
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
    defaults_[index(ShortcutId::TimelineZoomIn)] = QKeySequence(Qt::CTRL | Qt::Key_Equal);
    defaults_[index(ShortcutId::TimelineZoomOut)] = QKeySequence(Qt::CTRL | Qt::Key_Minus);
    defaults_[index(ShortcutId::TimelineZoomReset)] = QKeySequence(Qt::CTRL | Qt::Key_0);
    defaults_[index(ShortcutId::ImportPlacementNextSizeMode)] = QKeySequence(Qt::Key_S);
    defaults_[index(ShortcutId::ImportPlacementPreviousSizeMode)] = QKeySequence(Qt::SHIFT | Qt::Key_S);
    defaults_[index(ShortcutId::ImportPlacementConfirm)] = QKeySequence(Qt::Key_Return);
    defaults_[index(ShortcutId::ImportPlacementCancel)] = QKeySequence(Qt::Key_Escape);
    defaults_[index(ShortcutId::ImportPlacementReset)] = QKeySequence(Qt::Key_R);
    defaults_[index(ShortcutId::RepeatLastAction)] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    defaults_[index(ShortcutId::TimelineJumpToInPoint)] = QKeySequence(Qt::Key_Home);
    defaults_[index(ShortcutId::TimelineJumpToOutPoint)] = QKeySequence(Qt::Key_End);

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
