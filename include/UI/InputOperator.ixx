module;
#include <QObject>
#include <QString>
#include <QKeySequence>
#include <functional>
#include <map>
#include <vector>
#include <wobjectdefs.h>

export module Input.Operator;

import std;
import InputEvent;

W_REGISTER_ARGTYPE(ArtifactCore::InputEvent)

export namespace ArtifactCore
{

class InputOperator;
class InputBinding;
class ActionManager;

/**
 * @brief Represents a key binding (like Blender's keymap)
 * 
 * Supports:
 * - Single key (e.g., Space)
 * - Modifier combinations (e.g., Ctrl+S, Shift+Ctrl+Z)
 * - Multi-step key combos (e.g., G then R for grab-rotate)
 * - Context-sensitive bindings
 */
class InputBinding : public QObject {
    W_OBJECT(InputBinding)
private:
    QString id_;
    QString name_;
    QString description_;
    
    // Key sequence (supports modifiers)
    int keyCode_ = 0;
    InputEvent::Modifiers modifiers_;
    InputEvent::Modifiers requiredModifiers_;  // Must be held
    InputEvent::Modifiers forbiddenModifiers_; // Must NOT be held
    
    // Multi-step support
    bool isChord_ = false;
    int firstKey_ = 0;
    std::vector<int> chordKeys_;
    
    // Context
    QString context_;  // e.g., "Timeline", "NodeGraph", "3DView"
    int priority_ = 0;
    
    // The action to execute
    std::function<void()> callback_;
    QString actionId_;
    
public:
    explicit InputBinding(const QString& id, QObject* parent = nullptr);
    ~InputBinding();
    
    // Properties
    QString id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }
    
    QString description() const { return description_; }
    void setDescription(const QString& desc) { description_ = desc; }
    
    // Key configuration
    int keyCode() const { return keyCode_; }
    void setKeyCode(int key) { keyCode_ = key; }
    
    InputEvent::Modifiers modifiers() const { return modifiers_; }
    void setModifiers(InputEvent::Modifiers mods) { modifiers_ = mods; }
    
    InputEvent::Modifiers requiredModifiers() const { return requiredModifiers_; }
    void setRequiredModifiers(InputEvent::Modifiers mods) { requiredModifiers_ = mods; }
    
    InputEvent::Modifiers forbiddenModifiers() const { return forbiddenModifiers_; }
    void setForbiddenModifiers(InputEvent::Modifiers mods) { forbiddenModifiers_ = mods; }
    
    // Chord support
    bool isChord() const { return isChord_; }
    void setIsChord(bool chord) { isChord_ = chord; }
    
    int firstKey() const { return firstKey_; }
    void setFirstKey(int key) { firstKey_ = key; }
    
    // Context
    QString context() const { return context_; }
    void setContext(const QString& ctx) { context_ = ctx; }
    
    int priority() const { return priority_; }
    void setPriority(int p) { priority_ = p; }
    
    // Callback
    std::function<void()> callback() const { return callback_; }
    void setCallback(const std::function<void()>& cb) { callback_ = cb; }
    
    QString actionId() const { return actionId_; }
    void setActionId(const QString& id) { actionId_ = id; }
    
    // Matching
    bool matches(const InputEvent& event) const;
    bool matchesChord(const std::vector<int>& keys) const;
    
    // Convert to human-readable string
    QString toString() const;
    
signals:
    void activated() W_SIGNAL(activated);
    void bindingChanged() W_SIGNAL(bindingChanged);
};

W_REGISTER_ARGTYPE(InputBinding*)

/**
 * @brief Action that can be triggered by shortcuts
 * 
 * Similar to Blender's operators - can have:
 * - Properties/parameters
 * - Undo support
 * - Confirmation dialogs
 */
class Action : public QObject {
    W_OBJECT(Action)
private:
    QString id_;
    QString name_;
    QString description_;
    QString category_;
    
    // Properties that can be passed to the action
    std::map<QString, QVariant> properties_;
    
    // Callback
    std::function<void(const QVariantMap&)> executeCallback_;
    std::function<bool(const QVariantMap&)> confirmCallback_;
    
public:
    explicit Action(const QString& id, QObject* parent = nullptr);
    ~Action();
    
    // Properties
    QString id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }
    
    QString description() const { return description_; }
    void setDescription(const QString& desc) { description_ = desc; }
    
    QString category() const { return category_; }
    void setCategory(const QString& cat) { category_ = cat; }
    
    // Properties map
    QVariant property(const QString& key) const;
    void setProperty(const QString& key, const QVariant& value);
    QVariantMap allProperties() const;
    void setProperties(const QVariantMap& props);
    
    // Execute
    void execute(const QVariantMap& params = QVariantMap());
    
    // Callbacks
    std::function<void(const QVariantMap&)>& executeCallback() { return executeCallback_; }
    void setExecuteCallback(const std::function<void(const QVariantMap&)>& cb) { executeCallback_ = cb; }
    
    std::function<bool(const QVariantMap&)>& confirmCallback() { return confirmCallback_; }
    void setConfirmCallback(const std::function<bool(const QVariantMap&)>& cb) { confirmCallback_ = cb; }
    
signals:
    void triggered(const QVariantMap& params) W_SIGNAL(triggered, params);
    void actionChanged() W_SIGNAL(actionChanged);
};

W_REGISTER_ARGTYPE(Action*)

/**
 * @brief Manager for all actions - similar to Blender's operator system
 */
class ActionManager : public QObject {
    W_OBJECT(ActionManager)
private:
    class Impl;
    Impl* impl_;
    
public:
    W_SINGLETON(ActionManager)
    
    explicit ActionManager(QObject* parent = nullptr);
    ~ActionManager();
    
    // Action registration
    Action* registerAction(const QString& id, 
                          const QString& name,
                          const QString& description = QString(),
                          const QString& category = QString());
    
    void unregisterAction(const QString& id);
    Action* getAction(const QString& id) const;
    std::vector<Action*> getActionsByCategory(const QString& category) const;
    std::vector<Action*> allActions() const;
    
    // Action execution
    void executeAction(const QString& id, const QVariantMap& params = QVariantMap());
    bool confirmAction(const QString& id, const QVariantMap& params = QVariantMap());
    
    // Create action with callback (convenience)
    Action* createAction(const QString& id,
                        const QString& name,
                        const QString& description,
                        std::function<void()> callback);
    
signals:
    void actionRegistered(Action* action) W_SIGNAL(actionRegistered, action);
    void actionUnregistered(const QString& id) W_SIGNAL(actionUnregistered, id);
    void actionExecuted(const QString& id, const QVariantMap& params) W_SIGNAL(actionExecuted, id, params);
};

/**
 * @brief Key map - similar to Blender's keymaps
 * 
 * Manages a collection of bindings for a specific context
 */
class KeyMap : public QObject {
    W_OBJECT(KeyMap)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit KeyMap(const QString& name, QObject* parent = nullptr);
    ~KeyMap();
    
    // Name
    QString name() const;
    void setName(const QString& name);
    
    // Context
    QString context() const;
    void setContext(const QString& ctx);
    
    // Add binding
    InputBinding* addBinding(int key, 
                            InputEvent::Modifiers modifiers,
                            Action* action,
                            const QString& description = QString());
    
    // Convenience methods
    InputBinding* addBinding(const QString& keySequence,
                            Action* action,
                            const QString& description = QString());
    
    // Remove binding
    void removeBinding(InputBinding* binding);
    void removeBinding(const QString& actionId);
    
    // Get bindings
    std::vector<InputBinding*> allBindings() const;
    InputBinding* findBinding(int key, InputEvent::Modifiers mods) const;
    std::vector<InputBinding*> findBindingsForAction(const QString& actionId) const;
    
    // Import/Export (for user customization)
    QString toJSON() const;
    bool fromJSON(const QString& json);
    
    // Clear
    void clear();
    
    // Check if key is mapped
    bool isKeyMapped(int key, InputEvent::Modifiers mods) const;
    
signals:
    void bindingAdded(InputBinding* binding) W_SIGNAL(bindingAdded, binding);
    void bindingRemoved(InputBinding* binding) W_SIGNAL(bindingRemoved, binding);
    void keyMapChanged() W_SIGNAL(keyMapChanged);
};

W_REGISTER_ARGTYPE(KeyMap*)

/**
 * @brief Input operator - processes input events and triggers actions
 * 
 * This is the main input system that:
 * - Receives input events
 * - Matches against active keymaps
 * - Executes corresponding actions
 */
class InputOperator : public QObject {
    W_OBJECT(InputOperator)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit InputOperator(QObject* parent = nullptr);
    ~InputOperator();
    
    // Singleton
    static InputOperator* instance();
    
    // KeyMap management
    KeyMap* addKeyMap(const QString& name, const QString& context = QString());
    void removeKeyMap(const QString& name);
    KeyMap* getKeyMap(const QString& name) const;
    std::vector<KeyMap*> allKeyMaps() const;
    
    // Active context
    QString activeContext() const;
    void setActiveContext(const QString& ctx);
    
    // Process input events
    bool processKeyEvent(const InputEvent& event);
    bool processKeyPress(int key, InputEvent::Modifiers modifiers);
    bool processKeyRelease(int key, InputEvent::Modifiers modifiers);
    
    // Chord handling
    bool isInChord() const;
    void cancelChord();
    
    // Action execution
    void executeAction(const QString& actionId, const QVariantMap& params = QVariantMap());
    
    // Enable/disable input processing
    bool isEnabled() const;
    void setEnabled(bool enabled);
    
    // Debug
    QString dumpKeyMaps() const;
    
signals:
    void keyPressed(int key, InputEvent::Modifiers mods) W_SIGNAL(keyPressed, key, mods);
    void keyReleased(int key, InputEvent::Modifiers mods) W_SIGNAL(keyReleased, key, mods);
    void actionExecuted(const QString& actionId) W_SIGNAL(actionExecuted, actionId);
    void contextChanged(const QString& context) W_SIGNAL(contextChanged, context);
    void chordStarted(int firstKey) W_SIGNAL(chordStarted, firstKey);
    void chordCancelled() W_SIGNAL(chordCancelled);
};

} // namespace ArtifactCore
