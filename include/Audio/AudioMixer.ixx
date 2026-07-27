module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <QJsonObject>
#include <QString>
#include <memory>
#include <string>
#include <vector>

export module Audio.Mixer;

import Audio.Bus;
import Audio.Segment;
import Core.ArtifactString;
import Utils.String.UniString;
import Memory.SharedPtr;

export namespace ArtifactCore {

class LIBRARY_DLL_API AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    // バス管理
    SharedPtr<AudioBus> createBus(const ZeroString& name);
    SharedPtr<AudioBus> createBus(const String& name);
    SharedPtr<AudioBus> createBus(const QString& name);
    SharedPtr<AudioBus> createBus(const UniString& name);
    void removeBus(SharedPtr<AudioBus> bus);
    
    // ルーティング設定
    void connect(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target);
    void disconnect(SharedPtr<AudioBus> source);
    
    // サイドチェーン設定 (パラレルな信号送信)
    void addSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target, float amount = 1.0f);
    void removeSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target);

    // 全体の実行
    // グラフをトポロジカルソートして順次処理します
    void process(AudioSegment& finalOutput);

    SharedPtr<AudioBus> getMasterBus() const { return masterBus_; }
    int busCount() const;
    std::vector<ZeroString> busNamesZero() const;
    std::vector<String> busNames() const;
    SharedPtr<AudioBus> findBusByName(const ZeroString& name) const;
    SharedPtr<AudioBus> findBusByName(const String& name) const;
    SharedPtr<AudioBus> findBusByName(const QString& name) const;
    SharedPtr<AudioBus> findBusByName(const UniString& name) const;

    std::vector<SharedPtr<AudioBus>> getAllBuses() const;
    SharedPtr<AudioBus> getRoutingTarget(SharedPtr<AudioBus> bus) const;
    std::vector<std::pair<SharedPtr<AudioBus>, float>> getSideChainSends(SharedPtr<AudioBus> bus) const;

    QJsonObject serialize() const;
    bool deserialize(const QJsonObject& data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    SharedPtr<AudioBus> masterBus_;
};

} // namespace ArtifactCore
