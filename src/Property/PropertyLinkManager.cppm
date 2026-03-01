module;

#include <vector>
#include <QVariant>
#include <QColor>

module Property.LinkManager;

import std;

namespace ArtifactCore {

PropertyLinkManager& PropertyLinkManager::instance() {
    static PropertyLinkManager instance;
    return instance;
}

void PropertyLinkManager::addLink(AbstractProperty* source, AbstractProperty* target, LinkType type) {
    PropertyLink link;
    link.source = source;
    link.target = target;
    link.type = type;
    links_.push_back(link);
}

void PropertyLinkManager::addLink(const PropertyLink& link) {
    links_.push_back(link);
}

void PropertyLinkManager::updateTargets(AbstractProperty* source) {
    if (!source) return;

    QVariant sourceValue = source->getValue();

    for (auto& link : links_) {
        if (link.source == source && link.target) {
            QVariant targetValue;

            switch (link.type) {
            case LinkType::Direct:
                targetValue = sourceValue;
                break;
            case LinkType::Inverse: {
                if (sourceValue.userType() == QMetaType::Float || sourceValue.userType() == QMetaType::Double) {
                    targetValue = 1.0f - sourceValue.toFloat();
                } else if (sourceValue.userType() == QMetaType::Bool) {
                    targetValue = !sourceValue.toBool();
                }
                break;
            }
            case LinkType::Scale:
                if (sourceValue.canConvert<float>()) {
                    targetValue = sourceValue.toFloat() * link.multiplier;
                }
                break;
            case LinkType::Offset:
                if (sourceValue.canConvert<float>()) {
                    targetValue = sourceValue.toFloat() + link.offset;
                }
                break;
            case LinkType::Custom:
                if (link.customMapper) {
                    targetValue = link.customMapper(sourceValue);
                }
                break;
            }

            if (targetValue.isValid()) {
                link.target->setValue(targetValue);
            }
        }
    }
}

void PropertyLinkManager::clear() {
    links_.clear();
}

} // namespace ArtifactCore
