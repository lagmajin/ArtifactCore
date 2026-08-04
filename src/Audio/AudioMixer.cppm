module;
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

module Audio.Mixer;

import Container.NamedVector;
import Audio.Bus;
import Audio.Segment;
import Core.ArtifactString;
import Utils.String.Like;
import Memory.TrackedPtr;
import Memory.SharedPtr;

namespace ArtifactCore {

namespace {

ZeroString toZeroString(const QString& text)
{
    return ZeroString(text.toUtf8().constData());
}

ZeroString toZeroString(const UniString& text)
{
    return ZeroString(static_cast<std::string>(text));
}

}

struct SideChainSend {
    SharedPtr<AudioBus> source;
    SharedPtr<AudioBus> target;
    float amount;
};

struct AudioMixer::Impl {
    std::vector<SharedPtr<AudioBus>> buses;
    std::map<const AudioBus*, const AudioBus*> routing;
    std::vector<SideChainSend> sends;

    SharedPtr<AudioBus> resolveBus(const AudioBus* bus) const {
        if (!bus) {
            return nullptr;
        }
        for (const auto& candidate : buses) {
            if (candidate && candidate.get() == bus) {
                return candidate;
            }
        }
        return nullptr;
    }

    std::vector<SharedPtr<AudioBus>> getSortedBuses() {
        NamedVector<SharedPtr<AudioBus>> result{makeNamedVector<SharedPtr<AudioBus>>(ContainerName{"AudioMixerSortedBuses"})};
        std::set<const AudioBus*> visited;
        std::set<const AudioBus*> visiting;

        std::function<void(const AudioBus*)> visit = [&](const AudioBus* bus) {
            if (!bus || visited.count(bus)) {
                return;
            }
            if (visiting.count(bus)) {
                return;
            }

            visiting.insert(bus);

            for (const auto& [src, target] : routing) {
                if (target == bus) {
                    visit(src);
                }
            }
            for (const auto& send : sends) {
                if (send.target && send.target.get() == bus) {
                    visit(send.source.get());
                }
            }

            visiting.erase(bus);
            visited.insert(bus);
            if (auto resolved = resolveBus(bus)) {
                result.add(resolved);
            }
        };

        for (const auto& bus : buses) {
            visit(bus.get());
        }

        return result.toStdVector();
    }
};

AudioMixer::AudioMixer() : impl_(std::make_unique<Impl>()) {
    masterBus_ = makeShared<AudioBus>();
    masterBus_->setName(ZeroString("Master"));
    impl_->buses.push_back(masterBus_);
}

AudioMixer::~AudioMixer() = default;

int AudioMixer::busCount() const
{
    return static_cast<int>(impl_->buses.size());
}

std::vector<String> AudioMixer::busNames() const
{
    const auto namesZero = busNamesZero();
    NamedVector<String> result{makeNamedVector<String>(ContainerName{"AudioMixerBusNames"})};
    result.reserve(namesZero.size());
    for (const auto& name : namesZero) {
        result.add(String(name.data(), name.length()));
    }
    return result.toStdVector();
}

std::vector<ZeroString> AudioMixer::busNamesZero() const
{
    std::vector<ZeroString> result;
    result.reserve(impl_->buses.size());
    for (const auto& bus : impl_->buses) {
        if (!bus) {
            continue;
        }
        result.push_back(bus->getName());
    }
    return result;
}

SharedPtr<AudioBus> AudioMixer::findBusByName(const String& name) const
{
    const ZeroString lookupName(ArtifactCore::toStdString(name));
    for (const auto& bus : impl_->buses) {
        if (!bus) {
            continue;
        }
        if (bus->getName() == lookupName) {
            return bus;
        }
    }
    return nullptr;
}

SharedPtr<AudioBus> AudioMixer::findBusByName(const QString& name) const
{
    return findBusByName(toZeroString(name));
}

SharedPtr<AudioBus> AudioMixer::findBusByName(const UniString& name) const
{
    return findBusByName(toZeroString(name));
}

SharedPtr<AudioBus> AudioMixer::findBusById(const Id& id) const
{
    if (id.isNil()) return nullptr;
    const auto it = std::find_if(
        impl_->buses.begin(), impl_->buses.end(),
        [&id](const auto& bus) { return bus && bus->id() == id; });
    return it == impl_->buses.end() ? nullptr : *it;
}

std::vector<SharedPtr<AudioBus>> AudioMixer::getAllBuses() const
{
    NamedVector<SharedPtr<AudioBus>> result{makeNamedVector<SharedPtr<AudioBus>>(ContainerName{"AudioMixerAllBuses"})};
    result.reserve(impl_->buses.size());
    for (const auto& bus : impl_->buses) {
        if (bus) result.add(bus);
    }
    return result.toStdVector();
}

SharedPtr<AudioBus> AudioMixer::getRoutingTarget(SharedPtr<AudioBus> bus) const
{
    auto it = impl_->routing.find(bus.get());
    if (it != impl_->routing.end()) {
        return impl_->resolveBus(it->second);
    }
    return masterBus_;
}

std::vector<std::pair<SharedPtr<AudioBus>, float>> AudioMixer::getSideChainSends(SharedPtr<AudioBus> bus) const
{
    NamedVector<std::pair<SharedPtr<AudioBus>, float>> result{makeNamedVector<std::pair<SharedPtr<AudioBus>, float>>(ContainerName{"AudioMixerSideChainSends"})};
    for (const auto& send : impl_->sends) {
        if (send.source == bus) {
            result.add({send.target, send.amount});
        }
    }
    return result.toStdVector();
}

QJsonObject AudioMixer::serialize() const {
    QJsonObject obj;
    QJsonArray busesArr;

    for (const auto& bus : impl_->buses) {
        if (!bus || bus == masterBus_) {
            continue;
        }

        QJsonObject busObj;
        busObj["id"] = bus->id().toQString();
        busObj["name"] = toQString(bus->getName());
        busObj["volume"] = bus->getVolume();
        busObj["pan"] = bus->getPan();
        busObj["layout"] = static_cast<int>(bus->getLayout());
        busObj["mute"] = bus->isMute();
        busObj["solo"] = bus->isSolo();

        const auto target = getRoutingTarget(bus);
        if (target) {
            busObj["targetId"] = target->id().toQString();
            busObj["target"] = toQString(target->getName());
        }

        QJsonArray sendsArr;
        for (const auto& send : impl_->sends) {
            if (send.source == bus) {
                QJsonObject sendObj;
                sendObj["targetId"] = send.target->id().toQString();
                sendObj["target"] = toQString(send.target->getName());
                sendObj["amount"] = send.amount;
                sendsArr.push_back(sendObj);
            }
        }
        busObj["sends"] = sendsArr;

        busesArr.push_back(busObj);
    }

    obj["buses"] = busesArr;
    return obj;
}

bool AudioMixer::deserialize(const QJsonObject& data) {
    if (!data.value(QStringLiteral("buses")).isArray()) {
        return false;
    }
    impl_->routing.clear();
    impl_->sends.clear();
    // Deserialization represents the complete mixer state. Remove buses that
    // are not present in the incoming document instead of merging stale buses
    // from the previously loaded composition.
    impl_->buses.erase(
        std::remove_if(impl_->buses.begin(), impl_->buses.end(),
            [this](const auto& bus) { return bus && bus != masterBus_; }),
        impl_->buses.end());

    const auto busesArr = data["buses"].toArray();
    std::set<QString> serializedBusIds;
    // Create all buses first. Routing targets are allowed to appear later in
    // the serialized array, so resolving them during the first pass would
    // silently drop valid connections.
    for (const auto& val : busesArr) {
        const auto busObj = val.toObject();
        const QString name = busObj["name"].toString();

        if (name.trimmed().isEmpty()) {
            continue;
        }

        const QString serializedId = busObj["id"].toString().trimmed();
        if (!serializedId.isEmpty() &&
            !serializedBusIds.insert(serializedId).second) {
            continue;
        }

        SharedPtr<AudioBus> bus;
        if (!serializedId.isEmpty()) {
            const Id id(serializedId);
            const auto idIt = std::find_if(
                impl_->buses.begin(), impl_->buses.end(),
                [&id](const auto& candidate) {
                    return candidate && candidate->id() == id;
                });
            if (idIt != impl_->buses.end()) bus = *idIt;
        }
        if (!bus) bus = findBusByName(name);
        if (!bus) {
            bus = createBus(name);
        }
        if (!bus) continue;
        if (!serializedId.isEmpty()) bus->restoreId(Id(serializedId));
        bus->setName(toZeroString(name));

        bus->setVolume(static_cast<float>(busObj["volume"].toDouble()));
        bus->setPan(static_cast<float>(busObj["pan"].toDouble()));
        if (busObj.contains("layout")) {
            bus->setLayout(static_cast<AudioChannelLayout>(busObj["layout"].toInt(
                static_cast<int>(AudioChannelLayout::Stereo))));
        }
        bus->setMute(busObj["mute"].toBool());
        bus->setSolo(busObj["solo"].toBool());
    }

    // Resolve graph edges only after every bus exists.
    const auto findBusBySerializedId = [this](const QString& value) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty()) return SharedPtr<AudioBus>{};
        return findBusById(Id(trimmed));
    };
    for (const auto& val : busesArr) {
        const auto busObj = val.toObject();
        const auto bus = findBusByName(busObj["name"].toString());
        if (!bus) {
            continue;
        }

        const QString targetName = busObj["target"].toString();
        auto target = findBusBySerializedId(busObj["targetId"].toString());
        if (!target) target = findBusByName(targetName);
        if (target && target != bus) {
            connect(bus, target);
        }

        const auto sendsArr = busObj["sends"].toArray();
        for (const auto& sVal : sendsArr) {
            const auto sendObj = sVal.toObject();
            const QString sTarget = sendObj["target"].toString();
            const float sAmount = static_cast<float>(sendObj["amount"].toDouble(1.0));
            auto sBus = findBusBySerializedId(sendObj["targetId"].toString());
            if (!sBus) sBus = findBusByName(sTarget);
            if (sBus) addSideChainSend(bus, sBus, sAmount);
        }
    }

    return true;
}

SharedPtr<AudioBus> AudioMixer::createBus(const String& name) {
    if (name.length() == 0 || findBusByName(name)) {
        return nullptr;
    }
    auto bus = makeShared<AudioBus>();
    bus->setName(name);
    impl_->buses.push_back(bus);
    connect(bus, masterBus_);
    return bus;
}

SharedPtr<AudioBus> AudioMixer::createBus(const QString& name) {
    return createBus(toZeroString(name));
}

SharedPtr<AudioBus> AudioMixer::createBus(const UniString& name) {
    return createBus(toZeroString(name));
}

void AudioMixer::removeBus(SharedPtr<AudioBus> bus) {
    if (!bus || bus == masterBus_ || !impl_->resolveBus(bus.get())) {
        return;
    }

    impl_->routing.erase(bus.get());
    for (auto& pair : impl_->routing) {
        if (pair.second == bus.get()) {
            pair.second = masterBus_.get();
        }
    }

    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == bus || send.target == bus;
        }),
        impl_->sends.end());

    impl_->buses.erase(std::remove(impl_->buses.begin(), impl_->buses.end(), bus), impl_->buses.end());
}

void AudioMixer::connect(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) {
    if (!source || !target || source == target ||
        !impl_->resolveBus(source.get()) || !impl_->resolveBus(target.get())) {
        return;
    }
    // Reject a route that would make the primary bus graph cyclic.
    const AudioBus* cursor = target.get();
    std::set<const AudioBus*> visited;
    while (cursor && visited.insert(cursor).second) {
        if (cursor == source.get()) return;
        const auto it = impl_->routing.find(cursor);
        cursor = it == impl_->routing.end() ? nullptr : it->second;
    }
    impl_->routing[source.get()] = target.get();
}

void AudioMixer::disconnect(SharedPtr<AudioBus> source) {
    impl_->routing.erase(source.get());
}

void AudioMixer::addSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target, float amount) {
    if (!source || !target || source == target || !std::isfinite(amount) ||
        !impl_->resolveBus(source.get()) || !impl_->resolveBus(target.get())) {
        return;
    }
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto existing = std::find_if(
        impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == source && send.target == target;
        });
    if (existing != impl_->sends.end()) {
        existing->amount = amount;
        return;
    }
    impl_->sends.push_back({source, target, amount});
}

void AudioMixer::removeSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) {
    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == source && send.target == target;
        }),
        impl_->sends.end());
}

void AudioMixer::process(AudioSegment& finalOutput) {
    const int frames = finalOutput.frameCount();
    const int sampleRate = finalOutput.sampleRate;

    if (frames <= 0 || sampleRate <= 0) {
        return;
    }

    // AudioBus accumulates inputs, therefore every bus must start each block
    // empty or the previous block will be mixed into the current one.
    for (const auto& bus : impl_->buses) {
        if (bus) {
            bus->clearInput(frames, sampleRate);
        }
    }

    const auto sorted = impl_->getSortedBuses();

    for (const auto& bus : sorted) {
        bus->process(bus->getOutputBuffer());

        auto it = impl_->routing.find(bus.get());
        if (it != impl_->routing.end() && it->second) {
            if (auto target = impl_->resolveBus(it->second)) {
                target->addInput(bus->getOutputBuffer());
            }
        }

        for (const auto& send : impl_->sends) {
            if (send.source == bus) {
                send.target->addSideChain(bus->getOutputBuffer(), send.amount);
            }
        }
    }

    finalOutput = masterBus_->getOutputBuffer();
}

} // namespace ArtifactCore
