module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Mixer;

import Audio.Bus;
import Audio.Segment;
import std;

export namespace ArtifactCore {

class LIBRARY_DLL_API AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    // バス管理
    std::shared_ptr<AudioBus> createBus(const std::string& name);
    void removeBus(std::shared_ptr<AudioBus> bus);
    
    // ルーティング設定
    void connect(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target);
    void disconnect(std::shared_ptr<AudioBus> source);
    
    // サイドチェーン設定 (パラレルな信号送信)
    void addSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target, float amount = 1.0f);
    void removeSideChainSend(std::shared_ptr<AudioBus> source, std::shared_ptr<AudioBus> target);

    // 全体の実行
    // グラフをトポロジカルソートして順次処理します
    void process(AudioSegment& finalOutput);

    std::shared_ptr<AudioBus> getMasterBus() const { return masterBus_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::shared_ptr<AudioBus> masterBus_;
};

} // namespace ArtifactCore
