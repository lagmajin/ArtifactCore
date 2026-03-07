module;
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <string>
#include <set>
#include <functional>

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
    std::map<std::shared_ptr<AudioBus>, std::shared_ptr<AudioBus>> routing; // source -> target
    std::vector<SideChainSend> sends;

    // トポロジカルソート用 (依存関係の順序でバスを並べる)
    std::vector<std::shared_ptr<AudioBus>> getSortedBuses() {
        std::vector<std::shared_ptr<AudioBus>> result;
        std::set<std::shared_ptr<AudioBus>> visited;
        std::set<std::shared_ptr<AudioBus>> visiting;

        std::function<void(std::shared_ptr<AudioBus>)> visit = [&](std::shared_ptr<AudioBus> bus) {
            if (visited.count(bus)) return;
            if (visiting.count(bus)) {
                // サイクル検出時はストップ (簡易的な保護)
                return;
            }

            visiting.insert(bus);

            // このバスに音声を送っているソースを先に処理する
            for (auto const& [src, target] : routing) {
                if (target == bus) {
                    visit(src);
                }
            }
            // サイドチェーン送信元も先に処理
            for (const auto& send : sends) {
                if (send.target == bus) {
                    visit(send.source);
                }
            }

            visiting.erase(bus);
            visited.insert(bus);
            result.push_back(bus);
        };

        for (auto& bus : buses) {
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

std::shared_ptr<AudioBus> AudioMixer::createBus(const std::string& name) {
    auto bus = std::make_shared<AudioBus>();
    bus->setName(name);
    impl_->buses.push_back(bus);
    // デフォルトでMasterへルーティング
    connect(bus, masterBus_);
    return bus;
}

void AudioMixer::removeBus(std::shared_ptr<AudioBus> bus) {
    if (bus == masterBus_) return;
    
    // 関連するルーティングを削除
    impl_->routing.erase(bus);
    for (auto& pair : impl_->routing) {
        if (pair.second == bus) pair.second = masterBus_;
    }

    // 関連するサイドチェーンを削除
    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(),
        [&](const auto& s) { return s.source == bus || s.target == bus; }), impl_->sends.end());

    impl_->buses.erase(std::remove(impl_->buses.begin(), impl_->buses.end(), bus), impl_->buses.end());
}

void AudioMixer::connect(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target) {
    if (source == target) return;
    impl_->routing[source] = target;
}

void AudioMixer::disconnect(std::shared_ptr<AudioBus> source) {
    impl_->routing.erase(source);
}

void AudioMixer::addSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target, float amount) {
    if (source == target) return;
    impl_->sends.push_back({source, target, amount});
}

void AudioMixer::removeSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target) {
    impl_->sends.erase(std::remove_if(impl_->sends.begin(), impl_->sends.end(), 
        [&](const auto& s) { return s.source == source && s.target == target; }), impl_->sends.end());
}

void AudioMixer::process(AudioSegment& finalOutput) {
    int frames = finalOutput.frameCount();
    int sampleRate = finalOutput.sampleRate;

    auto sorted = impl_->getSortedBuses();

    // 1. 各バスの入力バッファをクリア
    for (auto& bus : sorted) {
        bus->clearInput(frames, sampleRate);
    }

    // 2. 音源 (Timeline等) から各バスへの流し込みは外部で行われる想定
    // ...

    // 3. ルーティングに沿って順次処理
    for (auto& bus : sorted) {
        // 自バスの処理 (Gain, Pan, VST)
        // ※ bus->getOutputBuffer() には既に前段からの入力が溜まっている
        bus->process(bus->getOutputBuffer());

        // 次のターゲット（Master等）へ音声を送る (Push)
        auto it = impl_->routing.find(bus);
        if (it != impl_->routing.end() && it->second) {
            it->second->addInput(bus->getOutputBuffer());
        }

        // サイドチェーンを送る
        for (const auto& send : impl_->sends) {
            if (send.source == bus) {
                send.target->addSideChain(bus->getOutputBuffer(), send.amount);
            }
        }
    }

    // 4. 最終出力を Master からコピー
    finalOutput = masterBus_->getOutputBuffer();
}

} // namespace ArtifactCore