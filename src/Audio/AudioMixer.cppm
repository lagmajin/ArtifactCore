module;
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
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

AudioBusKind legacyBusKind(const QString& name)
{
    return name.startsWith(QStringLiteral("layer_"))
        ? AudioBusKind::Layer : AudioBusKind::Group;
}

AudioBusKind readBusKind(const QJsonObject& busObj, const QString& name)
{
    if (!busObj.contains(QStringLiteral("kind"))) {
        return legacyBusKind(name);
    }
    const int value = busObj.value(QStringLiteral("kind")).toInt(
        static_cast<int>(legacyBusKind(name)));
    if (value < static_cast<int>(AudioBusKind::Layer) ||
        value > static_cast<int>(AudioBusKind::Vca)) {
        return legacyBusKind(name);
    }
    return static_cast<AudioBusKind>(value);
}

}

struct SideChainSend {
    SharedPtr<AudioBus> source;
    SharedPtr<AudioBus> target;
    float amount;
    bool preFader = false;
};

struct AudioMixer::Impl {
    std::vector<SharedPtr<AudioBus>> buses;
    std::map<const AudioBus*, const AudioBus*> routing;
    std::vector<SideChainSend> sends;
    std::map<const AudioBus*, AudioBusKind> busKinds;
    std::map<const AudioBus*, std::vector<const AudioBus*>> vcaMembers;

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
    impl_->busKinds[masterBus_.get()] = AudioBusKind::Master;
}

AudioMixer::~AudioMixer() = default;

String AudioMixer::layerBusName(const Id& layerId) {
    return String("layer_" + layerId.toString().toStdString());
}

QString AudioMixer::routingResultDescription(const AudioRoutingResult result) {
    switch (result) {
    case AudioRoutingResult::Applied:
        return QStringLiteral("Routing updated.");
    case AudioRoutingResult::NoRoute:
        return QStringLiteral("The bus has no editable output route.");
    case AudioRoutingResult::InvalidSource:
        return QStringLiteral("The source bus is unavailable.");
    case AudioRoutingResult::InvalidTarget:
        return QStringLiteral("The destination bus is unavailable.");
    case AudioRoutingResult::MasterSource:
        return QStringLiteral("Master is the final output and cannot be routed as a source.");
    case AudioRoutingResult::SelfRoute:
        return QStringLiteral("A bus cannot route to itself.");
    case AudioRoutingResult::CycleDetected:
        return QStringLiteral("This route would create an audio routing cycle.");
    case AudioRoutingResult::InvalidAmount:
        return QStringLiteral("The send amount must be a finite value.");
    case AudioRoutingResult::NoSend:
        return QStringLiteral("The sidechain send no longer exists.");
    }
    return QStringLiteral("The routing operation could not be completed.");
}

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

AudioBusKind AudioMixer::busKind(SharedPtr<AudioBus> bus) const
{
    if (!bus || !impl_->resolveBus(bus.get())) {
        return AudioBusKind::Group;
    }
    const auto it = impl_->busKinds.find(bus.get());
    return it == impl_->busKinds.end() ? AudioBusKind::Group : it->second;
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

qint64 AudioMixer::graphLatencySamples() const
{
    qint64 maximum = 0;
    for (const auto& bus : impl_->buses) {
        if (!bus) continue;
        qint64 path = 0;
        std::set<const AudioBus*> visited;
        const AudioBus* cursor = bus.get();
        while (cursor && visited.insert(cursor).second) {
            auto resolved = impl_->resolveBus(cursor);
            if (!resolved) break;
            const qint64 busLatency = resolved->latencySamples();
            if (busLatency > 0 && path > std::numeric_limits<qint64>::max() - busLatency) {
                path = std::numeric_limits<qint64>::max();
                break;
            }
            path += std::max<qint64>(0, busLatency);
            const auto it = impl_->routing.find(cursor);
            cursor = it == impl_->routing.end() ? nullptr : it->second;
        }
        maximum = std::max(maximum, path);
    }
    return maximum;
}

qint64 AudioMixer::graphTailSamples() const
{
    qint64 maximum = 0;
    for (const auto& bus : impl_->buses) {
        if (!bus) continue;
        qint64 path = 0;
        std::set<const AudioBus*> visited;
        const AudioBus* cursor = bus.get();
        while (cursor && visited.insert(cursor).second) {
            auto resolved = impl_->resolveBus(cursor);
            if (!resolved) break;
            const qint64 busTail = std::max<qint64>(0, resolved->tailSamples());
            if (busTail > 0 && path > std::numeric_limits<qint64>::max() - busTail) {
                path = std::numeric_limits<qint64>::max();
                break;
            }
            path += busTail;
            const auto it = impl_->routing.find(cursor);
            cursor = it == impl_->routing.end() ? nullptr : it->second;
        }
        maximum = std::max(maximum, path);
    }
    return maximum;
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

bool AudioMixer::isSideChainSendPreFader(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) const
{
    const auto it = std::find_if(
        impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) { return send.source == source && send.target == target; });
    return it != impl_->sends.end() && it->preFader;
}

QJsonObject AudioMixer::serialize() const {
    QJsonObject obj;
    QJsonArray busesArr;

    if (masterBus_) {
        QJsonObject masterObj;
        masterObj[QStringLiteral("volume")] = masterBus_->getVolume();
        masterObj[QStringLiteral("mute")] = masterBus_->isMute();
        obj[QStringLiteral("master")] = masterObj;
    }

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
        busObj["kind"] = static_cast<int>(busKind(bus));

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
                sendObj["preFader"] = send.preFader;
                sendsArr.push_back(sendObj);
            }
        }
        busObj["sends"] = sendsArr;

        if (busKind(bus) == AudioBusKind::Vca) {
            QJsonArray membersArr;
            const auto membersIt = impl_->vcaMembers.find(bus.get());
            if (membersIt != impl_->vcaMembers.end()) {
                for (const auto* member : membersIt->second) {
                    if (member) membersArr.push_back(member->id().toQString());
                }
            }
            busObj["vcaMembers"] = membersArr;
        }

        busesArr.push_back(busObj);
    }

    obj["buses"] = busesArr;
    return obj;
}

bool AudioMixer::deserialize(const QJsonObject& data) {
    if (!data.value(QStringLiteral("buses")).isArray()) {
        return false;
    }
    if (masterBus_ && data.value(QStringLiteral("master")).isObject()) {
        const auto masterObj = data.value(QStringLiteral("master")).toObject();
        if (masterObj.contains(QStringLiteral("volume"))) {
            masterBus_->setVolume(static_cast<float>(masterObj.value(
                QStringLiteral("volume")).toDouble(
                    static_cast<double>(masterBus_->getVolume()))));
        }
        if (masterObj.contains(QStringLiteral("mute"))) {
            masterBus_->setMute(masterObj.value(QStringLiteral("mute")).toBool(
                masterBus_->isMute()));
        }
    }
    impl_->routing.clear();
    impl_->sends.clear();
    impl_->vcaMembers.clear();
    impl_->busKinds.clear();
    impl_->busKinds[masterBus_.get()] = AudioBusKind::Master;
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
        const AudioBusKind kind = readBusKind(busObj, name);
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
            bus = createBus(toZeroString(name), kind);
        }
        if (!bus) continue;
        impl_->busKinds[bus.get()] = kind;
        if (!serializedId.isEmpty()) bus->restoreId(Id(serializedId));
        bus->setName(toZeroString(name));

        // Older project files may omit fields added after the first mixer
        // serializer. Keep AudioBus defaults for absent values instead of
        // turning a missing field into a destructive zero.
        if (busObj.contains(QStringLiteral("volume"))) {
            bus->setVolume(static_cast<float>(busObj["volume"].toDouble(
                static_cast<double>(bus->getVolume()))));
        }
        if (busObj.contains(QStringLiteral("pan"))) {
            bus->setPan(static_cast<float>(busObj["pan"].toDouble(
                static_cast<double>(bus->getPan()))));
        }
        if (busObj.contains("layout")) {
            bus->setLayout(static_cast<AudioChannelLayout>(busObj["layout"].toInt(
                static_cast<int>(AudioChannelLayout::Stereo))));
        }
        if (busObj.contains(QStringLiteral("mute"))) {
            bus->setMute(busObj["mute"].toBool(bus->isMute()));
        }
        if (busObj.contains(QStringLiteral("solo"))) {
            bus->setSolo(busObj["solo"].toBool(bus->isSolo()));
        }
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
            const bool sPreFader = sendObj["preFader"].toBool(false);
            auto sBus = findBusBySerializedId(sendObj["targetId"].toString());
            if (!sBus) sBus = findBusByName(sTarget);
            if (sBus) addSideChainSend(bus, sBus, sAmount, sPreFader);
        }

        if (busKind(bus) == AudioBusKind::Vca) {
            for (const auto& memberValue : busObj["vcaMembers"].toArray()) {
                const auto member = findBusBySerializedId(memberValue.toString());
                if (member) assignVcaMember(bus, member);
            }
        }
    }

    return true;
}

SharedPtr<AudioBus> AudioMixer::createBus(const String& name) {
    return createBus(name, AudioBusKind::Group);
}

SharedPtr<AudioBus> AudioMixer::createBus(const String& name, AudioBusKind kind) {
    if (name.length() == 0 || findBusByName(name)) {
        return nullptr;
    }
    if (kind == AudioBusKind::Master || kind == AudioBusKind::Layer) {
        kind = AudioBusKind::Group;
    }
    auto bus = makeShared<AudioBus>();
    bus->setName(name);
    impl_->buses.push_back(bus);
    impl_->busKinds[bus.get()] = kind;
    connect(bus, masterBus_);
    return bus;
}

SharedPtr<AudioBus> AudioMixer::createBus(const QString& name) {
    return createBus(toZeroString(name));
}

SharedPtr<AudioBus> AudioMixer::createBus(const UniString& name) {
    return createBus(toZeroString(name));
}

SharedPtr<AudioBus> AudioMixer::ensureLayerBus(const Id& layerId) {
    const auto name = layerBusName(layerId);
    auto bus = findBusByName(name);
    if (!bus) {
        bus = createBus(name);
    }
    if (bus && bus != masterBus_) {
        impl_->busKinds[bus.get()] = AudioBusKind::Layer;
    }
    return bus;
}

void AudioMixer::removeBus(SharedPtr<AudioBus> bus) {
    if (!bus || bus == masterBus_ || !impl_->resolveBus(bus.get())) {
        return;
    }

    impl_->routing.erase(bus.get());
    impl_->busKinds.erase(bus.get());
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

    impl_->vcaMembers.erase(bus.get());
    for (auto it = impl_->vcaMembers.begin(); it != impl_->vcaMembers.end();) {
        auto& members = it->second;
        members.erase(std::remove(members.begin(), members.end(), bus.get()), members.end());
        if (members.empty()) it = impl_->vcaMembers.erase(it);
        else ++it;
    }

    impl_->buses.erase(std::remove(impl_->buses.begin(), impl_->buses.end(), bus), impl_->buses.end());
}

AudioRoutingResult AudioMixer::connect(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) {
    // Master is the terminal sink for this mixer. Allowing it to become a
    // source silently makes finalOutput diverge from the graph's audible end.
    if (!source || !impl_->resolveBus(source.get())) return AudioRoutingResult::InvalidSource;
    if (!target || !impl_->resolveBus(target.get())) return AudioRoutingResult::InvalidTarget;
    if (source == masterBus_) return AudioRoutingResult::MasterSource;
    if (source == target) return AudioRoutingResult::SelfRoute;
    // Reject a route that would make the primary bus graph cyclic.
    const AudioBus* cursor = target.get();
    std::set<const AudioBus*> visited;
    while (cursor && visited.insert(cursor).second) {
        if (cursor == source.get()) return AudioRoutingResult::CycleDetected;
        const auto it = impl_->routing.find(cursor);
        cursor = it == impl_->routing.end() ? nullptr : it->second;
    }
    impl_->routing[source.get()] = target.get();
    return AudioRoutingResult::Applied;
}

AudioRoutingResult AudioMixer::disconnect(SharedPtr<AudioBus> source) {
    if (!source || !impl_->resolveBus(source.get())) return AudioRoutingResult::InvalidSource;
    if (source == masterBus_) return AudioRoutingResult::MasterSource;
    return impl_->routing.erase(source.get()) > 0
        ? AudioRoutingResult::Applied : AudioRoutingResult::NoRoute;
}

AudioRoutingResult AudioMixer::addSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target, float amount, bool preFader) {
    if (!source || !impl_->resolveBus(source.get())) return AudioRoutingResult::InvalidSource;
    if (!target || !impl_->resolveBus(target.get())) return AudioRoutingResult::InvalidTarget;
    if (source == target) return AudioRoutingResult::SelfRoute;
    if (!std::isfinite(amount)) return AudioRoutingResult::InvalidAmount;
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto existing = std::find_if(
        impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == source && send.target == target;
        });
    if (existing != impl_->sends.end()) {
        existing->amount = amount;
        existing->preFader = preFader;
        return AudioRoutingResult::Applied;
    }
    impl_->sends.push_back({source, target, amount, preFader});
    return AudioRoutingResult::Applied;
}

AudioRoutingResult AudioMixer::removeSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) {
    if (!source || !impl_->resolveBus(source.get())) return AudioRoutingResult::InvalidSource;
    if (!target || !impl_->resolveBus(target.get())) return AudioRoutingResult::InvalidTarget;
    const auto oldSize = impl_->sends.size();
    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == source && send.target == target;
        }),
        impl_->sends.end());
    return impl_->sends.size() != oldSize
        ? AudioRoutingResult::Applied : AudioRoutingResult::NoSend;
}

AudioRoutingResult AudioMixer::assignVcaMember(SharedPtr<AudioBus> vca, SharedPtr<AudioBus> member)
{
    if (!vca || !impl_->resolveBus(vca.get())) return AudioRoutingResult::InvalidSource;
    if (busKind(vca) != AudioBusKind::Vca) return AudioRoutingResult::InvalidSource;
    if (!member || !impl_->resolveBus(member.get())) return AudioRoutingResult::InvalidTarget;
    if (member == masterBus_ || member == vca || busKind(member) == AudioBusKind::Vca) {
        return AudioRoutingResult::InvalidTarget;
    }
    auto& members = impl_->vcaMembers[vca.get()];
    if (std::find(members.begin(), members.end(), member.get()) == members.end()) {
        members.push_back(member.get());
    }
    return AudioRoutingResult::Applied;
}

AudioRoutingResult AudioMixer::removeVcaMember(SharedPtr<AudioBus> vca, SharedPtr<AudioBus> member)
{
    if (!vca || !impl_->resolveBus(vca.get())) return AudioRoutingResult::InvalidSource;
    if (!member || !impl_->resolveBus(member.get())) return AudioRoutingResult::InvalidTarget;
    const auto it = impl_->vcaMembers.find(vca.get());
    if (it == impl_->vcaMembers.end()) return AudioRoutingResult::NoRoute;
    auto& members = it->second;
    const auto oldSize = members.size();
    members.erase(std::remove(members.begin(), members.end(), member.get()), members.end());
    const bool removed = members.size() != oldSize;
    if (members.empty()) impl_->vcaMembers.erase(it);
    return removed ? AudioRoutingResult::Applied : AudioRoutingResult::NoRoute;
}

std::vector<SharedPtr<AudioBus>> AudioMixer::getVcaMembers(SharedPtr<AudioBus> vca) const
{
    NamedVector<SharedPtr<AudioBus>> result{makeNamedVector<SharedPtr<AudioBus>>(ContainerName{"AudioMixerVcaMembers"})};
    if (!vca) return result.toStdVector();
    const auto it = impl_->vcaMembers.find(vca.get());
    if (it == impl_->vcaMembers.end()) return result.toStdVector();
    for (const auto* member : it->second) {
        if (auto resolved = impl_->resolveBus(member)) result.add(resolved);
    }
    return result.toStdVector();
}

void AudioMixer::process(AudioSegment& finalOutput) {
    const int frames = finalOutput.frameCount();
    const int sampleRate = finalOutput.sampleRate;

    if (frames <= 0 || sampleRate <= 0) {
        finalOutput.zero();
        return;
    }

    // Input buses are staged by the composition before process() is called.
    // Clearing here would erase those samples before routing can consume them.
    // Callers own block preparation; derived buses are cleared by the
    // composition before each evaluation block.
    for (const auto& bus : impl_->buses) {
        if (!bus) continue;
        const bool isDerived = bus == masterBus_ || std::any_of(
            impl_->routing.begin(), impl_->routing.end(),
            [&bus](const auto& route) { return route.second == bus.get(); });
        if (isDerived) bus->clearInput(frames, sampleRate);
    }

    const auto sorted = impl_->getSortedBuses();

    const auto primaryPathLatency = [this](const SharedPtr<AudioBus>& source) {
        qint64 total = 0;
        std::set<const AudioBus*> visited;
        const AudioBus* cursor = source.get();
        while (cursor && visited.insert(cursor).second) {
            const auto resolved = impl_->resolveBus(cursor);
            if (!resolved) break;
            const qint64 value = std::max<qint64>(0, resolved->latencySamples());
            if (value > 0 && total > std::numeric_limits<qint64>::max() - value) {
                return std::numeric_limits<qint64>::max();
            }
            total += value;
            const auto route = impl_->routing.find(cursor);
            cursor = route == impl_->routing.end() ? nullptr : route->second;
        }
        return total;
    };
    qint64 maximumPathLatency = 0;
    for (const auto& bus : sorted) {
        if (bus) maximumPathLatency = std::max(
            maximumPathLatency, primaryPathLatency(bus));
    }

    // Solo is a graph-level decision. A bus remains audible when it is
    // soloed, carries a soloed child, or feeds an explicitly soloed group.
    // The last case keeps a group solo useful; it deliberately follows only
    // the downstream primary route so a sibling of a soloed child stays muted.
    // The Master bus must always process the surviving graph. Sidechain sends
    // remain control inputs and do not make a primary route audible alone.
    const bool hasSolo = std::any_of(
        impl_->buses.begin(), impl_->buses.end(),
        [this](const SharedPtr<AudioBus>& bus) {
            return bus && bus != masterBus_ && bus->isSolo();
        });
    const auto hasSoloUpstream = [this](const SharedPtr<AudioBus>& target) {
        if (!target || target == masterBus_) {
            return false;
        }
        std::set<const AudioBus*> visited;
        std::function<bool(const AudioBus*)> visit = [&](const AudioBus* bus) {
            if (!bus || !visited.insert(bus).second) {
                return false;
            }
            if (bus->isSolo()) {
                return true;
            }
            for (const auto& [source, destination] : impl_->routing) {
                if (destination == bus && visit(source)) {
                    return true;
                }
            }
            return false;
        };
        return visit(target.get());
    };
    const auto feedsSoloGroup = [this](const SharedPtr<AudioBus>& source) {
        if (!source || source == masterBus_) {
            return false;
        }
        std::set<const AudioBus*> visited;
        const AudioBus* cursor = source.get();
        while (cursor && visited.insert(cursor).second) {
            const auto it = impl_->routing.find(cursor);
            if (it == impl_->routing.end() || !it->second ||
                it->second == masterBus_.get()) {
                return false;
            }
            cursor = it->second;
            if (cursor->isSolo()) {
                return true;
            }
        }
        return false;
    };

    for (const auto& bus : sorted) {
        if (busKind(bus) == AudioBusKind::Vca) {
            bus->getOutputBuffer().zero();
            continue;
        }
        if (hasSolo && bus != masterBus_ && !hasSoloUpstream(bus) &&
            !feedsSoloGroup(bus)) {
            // Preserve the explicit mute state; solo is a temporary mix
            // decision and must not be persisted as a mute mutation.
            bus->getOutputBuffer().zero();
        }
        float vcaGain = 1.0f;
        for (const auto& [vca, members] : impl_->vcaMembers) {
            if (std::find(members.begin(), members.end(), bus.get()) == members.end()) {
                continue;
            }
            if (const auto vcaBus = impl_->resolveBus(vca)) {
                const float db = vcaBus->getVolume();
                if (std::isfinite(db)) {
                    vcaGain *= std::pow(10.0f, db / 20.0f);
                }
            }
        }
        bus->process(bus->getOutputBuffer(), vcaGain);

        // Compensate only primary source buses. A group/master already
        // contains aligned upstream material; delaying it again would double
        // compensate the path. Sidechain sends remain control paths.
        if (bus != masterBus_) {
            const bool hasPrimaryInput = std::any_of(
                impl_->routing.begin(), impl_->routing.end(),
                [&bus](const auto& route) {
                    return route.second == bus.get();
                });
            if (!hasPrimaryInput) {
                const qint64 pathLatency = primaryPathLatency(bus);
                const qint64 compensation = pathLatency >= maximumPathLatency
                    ? 0 : maximumPathLatency - pathLatency;
                bus->applyLatencyCompensation(compensation);
            } else {
                // A bus can change from source to group after a routing edit;
                // discard any old source delay history at that boundary.
                bus->applyLatencyCompensation(0);
            }
        }

        auto it = impl_->routing.find(bus.get());
        if (it != impl_->routing.end() && it->second) {
            if (auto target = impl_->resolveBus(it->second)) {
                target->addInput(bus->getOutputBuffer());
            }
        }

        for (const auto& send : impl_->sends) {
            if (send.source == bus) {
                const auto& sendBuffer = send.preFader
                    ? bus->getPreFaderBuffer()
                    : bus->getOutputBuffer();
                send.target->addSideChain(sendBuffer, send.amount);
            }
        }
    }

    finalOutput = masterBus_->getOutputBuffer();
}

} // namespace ArtifactCore
