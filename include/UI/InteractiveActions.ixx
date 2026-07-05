module;
#include <utility>
#include <QVariant>
#include <QString>
#include <QJsonObject>
#include <QPointF>
#include <memory>
#include <wobjectdefs.h>

export module UI.InteractiveActions;

import Input.Operator;
import InputEvent;
import Property.Abstract;
import Command.SessionManager;
import Command.Common;
import Utils.Id;

export namespace ArtifactCore {

/**
 * @brief Concrete InteractiveAction for dragging properties.
 * Commits a PropertyChangeCommand on release.
 */
class PropertyDragAction : public InteractiveAction {
    W_OBJECT(PropertyDragAction)
private:
    AbstractProperty* target_ = nullptr;
    Id targetId_;
    QVariant initialValue_;
    float startMouseX_ = 0.0f;
    float sensitivity_ = 1.0f;

public:
    PropertyDragAction(AbstractProperty* prop, const Id& id) : target_(prop), targetId_(id) {}

    void begin(const InputEvent& event) override {
        if (!target_) return;
        initialValue_ = target_->getValue();
        startMouseX_ = event.position.x();
    }

    void update(const InputEvent& event) override {
        if (!target_) return;
        float delta = (event.position.x() - startMouseX_) * sensitivity_;
        
        // Handle Float/Int increment (simplified)
        if (target_->getType() == PropertyType::Float) {
            float newValue = initialValue_.toFloat() + delta;
            target_->setValue(newValue);
        } else if (target_->getType() == PropertyType::Integer) {
            int newValue = initialValue_.toInt() + static_cast<int>(delta);
            target_->setValue(newValue);
        }
    }

    void end(const InputEvent& event) override {
        if (!target_) return;
        
        QVariant finalValue = target_->getValue();
        if (finalValue != initialValue_) {
            // Push to session
            if (EditSessionManager::instance().hasActiveSession()) {
                auto cmd = std::make_unique<PropertyChangeCommand>(
                    targetId_, 
                    target_->getName(), 
                    QJsonValue::fromVariant(finalValue), 
                    QJsonValue::fromVariant(initialValue_)
                );
                // Note: SerializableCommand ownership logic in pushCommand
                // Using cast if needed, but here we assume it'sSerializableCommand base.
                EditSessionManager::instance().activeSession()->pushCommand(std::move(cmd));
            }
        }
    }

    void cancel() override {
        if (target_) {
            target_->setValue(initialValue_);
        }
    }
};

/**
 * @brief Interactive action for dragging an asset onto a composition.
 * Shows a preview during drag and creates a layer on drop.
 */
class AssetToLayerAction : public InteractiveAction {
    W_OBJECT(AssetToLayerAction)
private:
    Id assetId_;
    QPointF previewPos_;
    bool isTimelineDrop_ = false;

public:
    AssetToLayerAction(const Id& assetId) : assetId_(assetId) {}

    void begin(const InputEvent& event) override {
        previewPos_ = QPointF(event.position.x(), event.position.y());
    }

    void update(const InputEvent& event) override {
        previewPos_ = QPointF(event.position.x(), event.position.y());
        // UI can query this previewPos_ to draw a ghost image
    }

    void end(const InputEvent& event) override {
        // Trigger the actual layer creation via ActionManager
        QVariantMap params;
        params["assetId"] = assetId_.toString();
        params["dropPos"] = QPointF(event.position.x(), event.position.y());
        
        ActionManager::instance()->executeAction("artifact.layer.create_from_asset", params);
    }

    void cancel() override {
        // Nothing to revert if not committed
    }
    
    QPointF previewPos() const { return previewPos_; }
};

}
