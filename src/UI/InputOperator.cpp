module;

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeySequence>
#include <QDebug>
#include <wobjectimpl.h>

module Input.Operator;
import InputEvent;

namespace ArtifactCore {

W_OBJECT_IMPL(InputBinding)
W_OBJECT_IMPL(Action)
W_OBJECT_IMPL(ActionManager)
W_OBJECT_IMPL(KeyMap)
W_OBJECT_IMPL(InputOperator)

// ==================== InputBinding Implementation ====================

InputBinding::InputBinding(const QString& id, QObject* parent)
    : QObject(parent)
    , id_(id)
{
}

InputBinding::~InputBinding() = default;

bool InputBinding::matches(const InputEvent& event) const {
    if (event.type != InputEvent::Type::KeyDown) {
        return false;
    }
    
    // Check key code
    if (event.keyCode != keyCode_) {
        return false;
    }
    
    // Check required modifiers (all must be present)
    if ((event.modifiers & requiredModifiers_) != requiredModifiers_) {
        return false;
    }
    
    // Check forbidden modifiers (none should be present)
    if ((event.modifiers & forbiddenModifiers_) != InputEvent::Modifiers()) {
        return false;
    }
    
    // Check if we should ignore certain modifiers
    // (e.g., NumLock, CapsLock)
    
    return true;
}

bool InputBinding::matchesChord(const std::vector<int>& keys) const {
    if (!isChord_ || keys.empty()) {
        return false;
    }
    
    if (keys[0] != firstKey_) {
        return false;
    }
    
    if (keys.size() != chordKeys_.size() + 1) {
        return false;
    }
    
    for (size_t i = 1; i < keys.size(); ++i) {
        if (keys[i] != chordKeys_[i - 1]) {
            return false;
        }
    }
    
    return true;
}

QString InputBinding::toString() const {
    QStringList parts;
    
    // Modifiers
    if (modifiers_ & InputEvent::ModifierKey::LCtrl || modifiers_ & InputEvent::ModifierKey::RCtrl) {
        parts << "Ctrl";
    }
    if (modifiers_ & InputEvent::ModifierKey::LShift || modifiers_ & InputEvent::ModifierKey::RShift) {
        parts << "Shift";
    }
    if (modifiers_ & InputEvent::ModifierKey::LAlt || modifiers_ & InputEvent::ModifierKey::RAlt) {
        parts << "Alt";
    }
    if (modifiers_ & InputEvent::ModifierKey::LMeta || modifiers_ & InputEvent::ModifierKey::RMeta) {
        parts << "Win";
    }
    
    // Key
    if (keyCode_ > 0) {
        parts << QKeySequence(keyCode_).toString();
    }
    
    return parts.join("+");
}

// ==================== Action Implementation ====================

Action::Action(const QString& id, QObject* parent)
    : QObject(parent)
    , id_(id)
{
}

Action::~Action() = default;

QVariant Action::property(const QString& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return QVariant();
}

void Action::setProperty(const QString& key, const QVariant& value) {
    properties_[key] = value;
    emit actionChanged();
}

QVariantMap Action::allProperties() const {
    QVariantMap map;
    for (auto& [key, value] : properties_) {
        map[key] = value;
    }
    return map;
}

void Action::setProperties(const QVariantMap& props) {
    properties_.clear();
    for (auto it = props.begin(); it != props.end(); ++it) {
        properties_[it.key()] = it.value();
    }
    emit actionChanged();
}

void Action::execute(const QVariantMap& params) {
    // Merge action properties with execution params
    QVariantMap merged = allProperties();
    for (auto it = params.begin(); it != params.end(); ++it) {
        merged[it.key()] = it.value();
    }
    
    // Check confirmation
    if (confirmCallback_ && !confirmCallback_(merged)) {
        return; // User cancelled
    }
    
    // Execute
    if (executeCallback_) {
        executeCallback_(merged);
    }
    
    emit triggered(merged);
}

// ==================== ActionManager::Impl ====================

class ActionManager::Impl {
public:
    std::map<QString, Action*> actions_;
    std::map<QString, QString> categories_;
};

//W_OBJECT_IMPL(ActionManager)

ActionManager* ActionManager::instance() {
    static ActionManager manager;
    return &manager;
}

ActionManager::ActionManager(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

ActionManager::~ActionManager() {
    delete impl_;
}

Action* ActionManager::registerAction(const QString& id,
                                     const QString& name,
                                     const QString& description,
                                     const QString& category) {
    if (impl_->actions_.count(id)) {
        qWarning() << "Action" << id << "already registered";
        return impl_->actions_[id];
    }
    
    auto* action = new Action(id, this);
    action->setName(name);
    action->setDescription(description);
    action->setCategory(category);
    
    impl_->actions_[id] = action;
    
    if (!category.isEmpty()) {
        impl_->categories_[id] = category;
    }
    
    emit actionRegistered(action);
    return action;
}

void ActionManager::unregisterAction(const QString& id) {
    auto it = impl_->actions_.find(id);
    if (it != impl_->actions_.end()) {
        emit actionUnregistered(id);
        delete it->second;
        impl_->actions_.erase(it);
        impl_->categories_.erase(id);
    }
}

Action* ActionManager::getAction(const QString& id) const {
    auto it = impl_->actions_.find(id);
    if (it != impl_->actions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<Action*> ActionManager::getActionsByCategory(const QString& category) const {
    std::vector<Action*> result;
    for (auto& [id, cat] : impl_->categories_) {
        if (cat == category) {
            auto it = impl_->actions_.find(id);
            if (it != impl_->actions_.end()) {
                result.push_back(it->second);
            }
        }
    }
    return result;
}

std::vector<Action*> ActionManager::allActions() const {
    std::vector<Action*> result;
    for (auto& [id, action] : impl_->actions_) {
        result.push_back(action);
    }
    return result;
}

void ActionManager::executeAction(const QString& id, const QVariantMap& params) {
    auto* action = getAction(id);
    if (action) {
        action->execute(params);
        emit actionExecuted(id, params);
    } else {
        qWarning() << "Action not found:" << id;
    }
}

bool ActionManager::confirmAction(const QString& id, const QVariantMap& params) {
    auto* action = getAction(id);
    if (action) {
        auto& cb = action->confirmCallback();
        if (cb) return cb(params);
    }
    return true;
}

Action* ActionManager::createAction(const QString& id,
                                   const QString& name,
                                   const QString& description,
                                   std::function<void()> callback) {
    auto* action = registerAction(id, name, description);
    if (callback) {
        action->executeCallback() = [callback](const QVariantMap&) {
            callback();
        };
    }
    return action;
}

// ==================== KeyMap::Impl ====================

class KeyMap::Impl {
public:
    QString name_;
    QString context_;
    std::map<QString, InputBinding*> bindings_;  // actionId -> binding
    std::map<std::pair<int, int>, InputBinding*> keyBindings_;  // (key, mods) -> binding
};

//W_OBJECT_IMPL(KeyMap)

KeyMap::KeyMap(const QString& name, QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->name_ = name;
}

KeyMap::~KeyMap() {
    // Clean up bindings
    for (auto& [id, binding] : impl_->bindings_) {
        delete binding;
    }
    delete impl_;
}

QString KeyMap::name() const {
    return impl_->name_;
}

void KeyMap::setName(const QString& name) {
    impl_->name_ = name;
    emit keyMapChanged();
}

QString KeyMap::context() const {
    return impl_->context_;
}

void KeyMap::setContext(const QString& ctx) {
    impl_->context_ = ctx;
    emit keyMapChanged();
}

InputBinding* KeyMap::addBinding(int key,
                                 InputEvent::Modifiers modifiers,
                                 Action* action,
                                 const QString& description) {
    // Create binding
    auto* binding = new InputBinding(action->id(), this);
    binding->setKeyCode(key);
    binding->setModifiers(modifiers);
    binding->setName(action->name());
    binding->setDescription(description.isEmpty() ? action->description() : description);
    binding->setActionId(action->id());
    
    // Set callback
    binding->setCallback([this, action]() {
        action->execute();
    });
    
    // Store
    impl_->bindings_[action->id()] = binding;
    impl_->keyBindings_[{key, static_cast<int>(modifiers)}] = binding;
    
    emit bindingAdded(binding);
    return binding;
}

InputBinding* KeyMap::addBinding(const QString& keySequence,
                                Action* action,
                                const QString& description) {
    QKeySequence seq(keySequence);
    return addBinding(seq[0], InputEvent::Modifiers(), action, description);
}

void KeyMap::removeBinding(InputBinding* binding) {
    if (!binding) return;
    
    impl_->bindings_.erase(binding->actionId());
    impl_->keyBindings_.erase({binding->keyCode(), static_cast<int>(binding->modifiers())});
    
    emit bindingRemoved(binding);
    delete binding;
}

void KeyMap::removeBinding(const QString& actionId) {
    auto it = impl_->bindings_.find(actionId);
    if (it != impl_->bindings_.end()) {
        removeBinding(it->second);
    }
}

std::vector<InputBinding*> KeyMap::allBindings() const {
    std::vector<InputBinding*> result;
    for (auto& [id, binding] : impl_->bindings_) {
        result.push_back(binding);
    }
    return result;
}

InputBinding* KeyMap::findBinding(int key, InputEvent::Modifiers mods) const {
    auto it = impl_->keyBindings_.find({key, static_cast<int>(mods)});
    if (it != impl_->keyBindings_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<InputBinding*> KeyMap::findBindingsForAction(const QString& actionId) const {
    std::vector<InputBinding*> result;
    auto it = impl_->bindings_.find(actionId);
    if (it != impl_->bindings_.end()) {
        result.push_back(it->second);
    }
    return result;
}

QString KeyMap::toJSON() const {
    QJsonArray bindings;
    for (auto& [id, binding] : impl_->bindings_) {
        QJsonObject obj;
        obj["id"] = binding->id();
        obj["name"] = binding->name();
        obj["key"] = binding->keyCode();
        obj["modifiers"] = static_cast<int>(binding->modifiers());
        obj["actionId"] = binding->actionId();
        obj["description"] = binding->description();
        bindings.append(obj);
    }
    
    QJsonObject root;
    root["name"] = name();
    root["context"] = context();
    root["bindings"] = bindings;
    
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool KeyMap::fromJSON(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) {
        return false;
    }
    
    QJsonObject root = doc.object();
    setName(root["name"].toString());
    setContext(root["context"].toString());
    
    clear();
    
    auto* actionManager = ActionManager::instance();
    QJsonArray bindings = root["bindings"].toArray();
    for (const QJsonValue& val : bindings) {
        QJsonObject obj = val.toObject();
        QString actionId = obj["actionId"].toString();
        auto* action = actionManager->getAction(actionId);
        
        if (action) {
            addBinding(obj["key"].toInt(),
                      static_cast<InputEvent::Modifiers>(obj["modifiers"].toInt()),
                      action,
                      obj["description"].toString());
        }
    }
    
    return true;
}

void KeyMap::clear() {
    std::vector<InputBinding*> bindings = allBindings();
    for (auto* binding : bindings) {
        removeBinding(binding);
    }
}

bool KeyMap::isKeyMapped(int key, InputEvent::Modifiers mods) const {
    return findBinding(key, mods) != nullptr;
}

// ==================== InputOperator::Impl ====================

class InputOperator::Impl {
public:
    std::map<QString, KeyMap*> keyMaps_;
    QString activeContext_ = "Global";
    bool enabled_ = true;
    bool inChord_ = false;
    std::vector<int> chordKeys_;
    int chordTimeout_ = 1000;  // ms
    
    Impl() {}
};

//W_OBJECT_IMPL(InputOperator)

InputOperator::InputOperator(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

InputOperator::~InputOperator() {
    for (auto& [name, keymap] : impl_->keyMaps_) {
        delete keymap;
    }
    delete impl_;
}

InputOperator* InputOperator::instance() {
    static InputOperator op;
    return &op;
}

KeyMap* InputOperator::addKeyMap(const QString& name, const QString& context) {
    if (impl_->keyMaps_.count(name)) {
        return impl_->keyMaps_[name];
    }
    
    auto* keyMap = new KeyMap(name, this);
    keyMap->setContext(context.isEmpty() ? "Global" : context);
    impl_->keyMaps_[name] = keyMap;
    return keyMap;
}

void InputOperator::removeKeyMap(const QString& name) {
    auto it = impl_->keyMaps_.find(name);
    if (it != impl_->keyMaps_.end()) {
        delete it->second;
        impl_->keyMaps_.erase(it);
    }
}

KeyMap* InputOperator::getKeyMap(const QString& name) const {
    auto it = impl_->keyMaps_.find(name);
    if (it != impl_->keyMaps_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<KeyMap*> InputOperator::allKeyMaps() const {
    std::vector<KeyMap*> result;
    for (auto& [name, keyMap] : impl_->keyMaps_) {
        result.push_back(keyMap);
    }
    return result;
}

QString InputOperator::activeContext() const {
    return impl_->activeContext_;
}

void InputOperator::setActiveContext(const QString& ctx) {
    impl_->activeContext_ = ctx;
    emit contextChanged(ctx);
}

bool InputOperator::processKeyEvent(const InputEvent& event) {
    if (!impl_->enabled_) {
        return false;
    }
    
    if (event.type == InputEvent::Type::KeyDown) {
        return processKeyPress(event.keyCode, event.modifiers);
    } else if (event.type == InputEvent::Type::KeyUp) {
        return processKeyRelease(event.keyCode, event.modifiers);
    }
    
    return false;
}

bool InputOperator::processKeyPress(int key, InputEvent::Modifiers modifiers) {
    if (!impl_->enabled_) {
        return false;
    }
    
    emit keyPressed(key, modifiers);
    
    // Try to find a matching binding
    for (auto& [name, keyMap] : impl_->keyMaps_) {
        // Check context
        if (!keyMap->context().isEmpty() && 
            keyMap->context() != impl_->activeContext_ &&
            keyMap->context() != "Global") {
            continue;
        }
        
        auto* binding = keyMap->findBinding(key, modifiers);
        if (binding) {
            // Execute the action
            if (binding->callback()) {
                binding->callback();
                emit actionExecuted(binding->actionId());
                emit binding->activated();
            }
            return true;
        }
    }
    
    // No binding found
    return false;
}

bool InputOperator::processKeyRelease(int key, InputEvent::Modifiers modifiers) {
    if (!impl_->enabled_) {
        return false;
    }
    
    emit keyReleased(key, modifiers);
    return false;
}

bool InputOperator::isInChord() const {
    return impl_->inChord_;
}

void InputOperator::cancelChord() {
    impl_->inChord_ = false;
    impl_->chordKeys_.clear();
    emit chordCancelled();
}

void InputOperator::executeAction(const QString& actionId, const QVariantMap& params) {
    ActionManager::instance()->executeAction(actionId, params);
}

bool InputOperator::isEnabled() const {
    return impl_->enabled_;
}

void InputOperator::setEnabled(bool enabled) {
    impl_->enabled_ = enabled;
}

QString InputOperator::dumpKeyMaps() const {
    QString result;
    QTextStream stream(&result);
    
    for (auto& [name, keyMap] : impl_->keyMaps_) {
        stream << "KeyMap: " << name << " (Context: " << keyMap->context() << ")\n";
        for (auto* binding : keyMap->allBindings()) {
            stream << "  " << binding->toString() << " -> " << binding->actionId() 
                   << " (" << binding->name() << ")\n";
        }
    }
    
    return result;
}

} // namespace ArtifactCore
