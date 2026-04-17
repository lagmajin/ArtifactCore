module;
#include <QKeySequence>
#include <QKeyEvent>
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

    return QKeySequence(event->modifiers() | event->key());
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

void ShortcutBindings::resetToDefaults()
{
    defaults_[index(ShortcutId::Undo)] = QKeySequence::Undo;
    defaults_[index(ShortcutId::Redo)] = QKeySequence::Redo;
    defaults_[index(ShortcutId::SelectionTool)] = QKeySequence(Qt::Key_V);
    defaults_[index(ShortcutId::HandTool)] = QKeySequence(Qt::Key_H);
    defaults_[index(ShortcutId::ZoomTool)] = QKeySequence(Qt::Key_Z);
    defaults_[index(ShortcutId::RotateTool)] = QKeySequence(Qt::Key_W);

    overrides_.fill(QKeySequence());
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
