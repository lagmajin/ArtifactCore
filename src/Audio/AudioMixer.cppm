module;
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

module Audio.Mixer;

import Audio.Bus;
import Audio.Segment;

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
        std::vector<std::shared_ptr<AudioBus>> result;
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
            result.push_back(bus);
        };

        for (const auto& bus : buses) {
            visit(bus);
        }

        return result;
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
    std::vector<std::string> result;
    result.reserve(impl_->buses.size());
    for (const auto& bus : impl_->buses) {
        if (!bus) {
            continue;
        }
        result.push_back(static_cast<std::string>(bus->getName()));
    }
    return result;
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
