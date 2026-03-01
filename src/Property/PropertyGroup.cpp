module;

#include <algorithm>
#include <utility>
module Property.Group;

import std;
import Property.Abstract;

namespace ArtifactCore {

class PropertyGroup::Impl {
public:
  explicit Impl(PropertyGroup* owner)
      : owner_(owner) {}

  std::vector<AbstractPropertyPtr> properties_;
  PropertyGroup* owner_ = nullptr;
};

PropertyGroup::PropertyGroup(QString name)
    : impl_(new Impl(this)), name_(std::move(name)) {}

PropertyGroup::~PropertyGroup() {
  delete impl_;
}

QString PropertyGroup::name() const {
  return name_;
}

void PropertyGroup::setName(const QString& name) {
  name_ = name;
}

void PropertyGroup::addProperty(const AbstractPropertyPtr& property) {
  if (!property) {
    return;
  }
  if (findProperty(property->getName())) {
    return;
  }
  impl_->properties_.push_back(property);
}

bool PropertyGroup::removeProperty(const QString& propertyName) {
  const auto it = std::find_if(
      impl_->properties_.begin(),
      impl_->properties_.end(),
      [&propertyName](const AbstractPropertyPtr& candidate) {
        return candidate && candidate->getName() == propertyName;
      });
  if (it == impl_->properties_.end()) {
    return false;
  }
  impl_->properties_.erase(it);
  return true;
}

AbstractPropertyPtr PropertyGroup::findProperty(const QString& propertyName) const {
  const auto it = std::find_if(
      impl_->properties_.begin(),
      impl_->properties_.end(),
      [&propertyName](const AbstractPropertyPtr& candidate) {
        return candidate && candidate->getName() == propertyName;
      });
  return it == impl_->properties_.end() ? AbstractPropertyPtr() : *it;
}

size_t PropertyGroup::propertyCount() const {
  return impl_->properties_.size();
}

std::vector<AbstractPropertyPtr> PropertyGroup::allProperties() const {
  return impl_->properties_;
}

std::vector<AbstractPropertyPtr> PropertyGroup::sortedProperties() const {
  auto sorted = impl_->properties_;
  std::stable_sort(sorted.begin(), sorted.end(),
      [](const AbstractPropertyPtr& a, const AbstractPropertyPtr& b) {
        return a->displayPriority() < b->displayPriority();
      });
  return sorted;
}

void PropertyGroup::addPropertyWithPriority(const AbstractPropertyPtr& property, int priority) {
  if (!property) return;
  property->setDisplayPriority(priority);
  addProperty(property);
}

} // namespace ArtifactCore