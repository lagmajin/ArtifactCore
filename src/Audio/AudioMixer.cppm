module;
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

module Audio.Mixer;

import Container.NamedVector;
import Audio.Bus;
import Audio.Segment;
import Memory.TrackedPtr;

namespace ArtifactCore {

struct SideChainSend {
    std::shared_ptr<AudioBus> source;
    std::shared_ptr<AudioBus> target;
    float amount;
};

struct AudioMixer::Impl {
    std::vector<std::shared_ptr<AudioBus>> buses;
    std::map<std::shared_ptr<AudioBus>, std::shared_ptr<AudioBus>> routing;
    std::vector<SideChainSend> sends;

    std::vector<std::shared_ptr<AudioBus>> getSortedBuses() {
        NamedVector<std::shared_ptr<AudioBus>> result{makeNamedVector<std::shared_ptr<AudioBus>>(ContainerName{"AudioMixerSortedBuses"})};
        std::set<std::shared_ptr<AudioBus>> visited;
        std::set<std::shared_ptr<AudioBus>> visiting;

        std::function<void(std::shared_ptr<AudioBus>)> visit = [&](const std::shared_ptr<AudioBus>& bus) {
            if (visited.count(bus)) {
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
                if (send.target == bus) {
                    visit(send.source);
                }
            }

            visiting.erase(bus);
            visited.insert(bus);
            result.add(bus);
        };

        for (const auto& bus : buses) {
            visit(bus);
        }

        return result.toStdVector();
    }
};

AudioMixer::AudioMixer() : impl_(std::make_unique<Impl>()) {
    masterBus_ = std::make_shared<AudioBus>();
    masterBus_->setName("Master");
    impl_->buses.push_back(masterBus_);
}

AudioMixer::~AudioMixer() = default;

int AudioMixer::busCount() const
{
    return static_cast<int>(impl_->buses.size());
}

std::vector<std::string> AudioMixer::busNames() const
{
    NamedVector<std::string> result{makeNamedVector<std::string>(ContainerName{"AudioMixerBusNames"})};
    result.reserve(impl_->buses.size());
    for (const auto& bus : impl_->buses) {
        if (!bus) {
            continue;
        }
        result.add(static_cast<std::string>(bus->getName()));
    }
    return result.toStdVector();
}

std::shared_ptr<AudioBus> AudioMixer::findBusByName(const std::string& name) const
{
    for (const auto& bus : impl_->buses) {
        if (!bus) {
            continue;
        }
        if (static_cast<std::string>(bus->getName()) == name) {
            return bus;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<AudioBus>> AudioMixer::getAllBuses() const
{
    NamedVector<std::shared_ptr<AudioBus>> result{makeNamedVector<std::shared_ptr<AudioBus>>(ContainerName{"AudioMixerAllBuses"})};
    result.reserve(impl_->buses.size());
    for (const auto& bus : impl_->buses) {
        if (bus) result.add(bus);
    }
    return result.toStdVector();
}

std::shared_ptr<AudioBus> AudioMixer::getRoutingTarget(std::shared_ptr<AudioBus> bus) const
{
    auto it = impl_->routing.find(bus);
    if (it != impl_->routing.end()) {
        return it->second;
    }
    return masterBus_;
}

std::vector<std::pair<std::shared_ptr<AudioBus>, float>> AudioMixer::getSideChainSends(std::shared_ptr<AudioBus> bus) const
{
    NamedVector<std::pair<std::shared_ptr<AudioBus>, float>> result{makeNamedVector<std::pair<std::shared_ptr<AudioBus>, float>>(ContainerName{"AudioMixerSideChainSends"})};
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
        busObj["name"] = QString::fromStdString(bus->getName());
        busObj["volume"] = bus->getVolume();
        busObj["pan"] = bus->getPan();
        busObj["mute"] = bus->isMute();
        busObj["solo"] = bus->isSolo();

        const auto target = getRoutingTarget(bus);
        busObj["target"] = QString::fromStdString(target->getName());

        QJsonArray sendsArr;
        for (const auto& send : impl_->sends) {
            if (send.source == bus) {
                QJsonObject sendObj;
                sendObj["target"] = QString::fromStdString(send.target->getName());
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
    impl_->routing.clear();
    impl_->sends.clear();

    const auto busesArr = data["buses"].toArray();
    for (const auto& val : busesArr) {
        const auto busObj = val.toObject();
        const std::string name = busObj["name"].toString().toStdString();

        auto bus = findBusByName(name);
        if (!bus) {
            bus = createBus(name);
        }

        bus->setVolume(static_cast<float>(busObj["volume"].toDouble()));
        bus->setPan(static_cast<float>(busObj["pan"].toDouble()));
        bus->setMute(busObj["mute"].toBool());
        bus->setSolo(busObj["solo"].toBool());

        const std::string targetName = busObj["target"].toString().toStdString();
        const auto target = findBusByName(targetName);
        if (target) {
            impl_->routing[bus] = target;
        }

        const auto sendsArr = busObj["sends"].toArray();
        for (const auto& sVal : sendsArr) {
            const auto sendObj = sVal.toObject();
            const std::string sTarget = sendObj["target"].toString().toStdString();
            const float sAmount = static_cast<float>(sendObj["amount"].toDouble());
            const auto sBus = findBusByName(sTarget);
            if (sBus) {
                impl_->sends.push_back({bus, sBus, sAmount});
            }
        }
    }

    return true;
}

std::shared_ptr<AudioBus> AudioMixer::createBus(const std::string& name) {
    auto bus = std::make_shared<AudioBus>();
    bus->setName(name);
    impl_->buses.push_back(bus);
    connect(bus, masterBus_);
    return bus;
}

void AudioMixer::removeBus(std::shared_ptr<AudioBus> bus) {
    if (bus == masterBus_) {
        return;
    }

    impl_->routing.erase(bus);
    for (auto& pair : impl_->routing) {
        if (pair.second == bus) {
            pair.second = masterBus_;
        }
    }

    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == bus || send.target == bus;
        }),
        impl_->sends.end());

    impl_->buses.erase(std::remove(impl_->buses.begin(), impl_->buses.end(), bus), impl_->buses.end());
}

void AudioMixer::connect(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target) {
    if (source == target) {
        return;
    }
    impl_->routing[source] = target;
}

void AudioMixer::disconnect(std::shared_ptr<AudioBus> source) {
    impl_->routing.erase(source);
}

void AudioMixer::addSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target, float amount) {
    if (source == target) {
        return;
    }
    impl_->sends.push_back({source, target, amount});
}

void AudioMixer::removeSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target) {
    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& send) {
            return send.source == source && send.target == target;
        }),
        impl_->sends.end());
}

void AudioMixer::process(AudioSegment& finalOutput) {
    const int frames = finalOutput.frameCount();
    const int sampleRate = finalOutput.sampleRate;

    const auto sorted = impl_->getSortedBuses();

    for (const auto& bus : sorted) {
        bus->clearInput(frames, sampleRate);
    }

    for (const auto& bus : sorted) {
        bus->process(bus->getOutputBuffer());

        auto it = impl_->routing.find(bus);
        if (it != impl_->routing.end() && it->second) {
            it->second->addInput(bus->getOutputBuffer());
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
