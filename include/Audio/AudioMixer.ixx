module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <QJsonObject>
#include <QString>
#include <memory>
#include <string>
#include <vector>

export module Audio.Mixer;

import Audio.Segment;
import Audio.Bus;
import Core.ArtifactString;
import Utils.Id;
import Utils.String.UniString;
import Memory.SharedPtr;

export namespace ArtifactCore {

enum class AudioRoutingResult {
    Applied,
    NoRoute,
    InvalidSource,
    InvalidTarget,
    MasterSource,
    SelfRoute,
    CycleDetected,
    InvalidAmount,
    NoSend,
};

enum class AudioBusKind {
    Master,
    Layer,
    Group,
    Return,
    Vca,
};

class LIBRARY_DLL_API AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    // Stable internal identity for a source-owned bus. Presentation labels
    // remain user-editable and must not be used for routing identity.
    static String layerBusName(const Id& layerId);

    // バス管理
    SharedPtr<AudioBus> createBus(const String& name);
    SharedPtr<AudioBus> createBus(const String& name, AudioBusKind kind);
    SharedPtr<AudioBus> createBus(const QString& name);
    SharedPtr<AudioBus> createBus(const UniString& name);
    SharedPtr<AudioBus> ensureLayerBus(const Id& layerId);
    void removeBus(SharedPtr<AudioBus> bus);
    
    // ルーティング設定
    AudioRoutingResult connect(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target);
    AudioRoutingResult disconnect(SharedPtr<AudioBus> source);
    
    // サイドチェーン設定 (パラレルな信号送信)
    AudioRoutingResult addSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target,
                                       float amount = 1.0f, bool preFader = false);
    AudioRoutingResult removeSideChainSend(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target);
    AudioRoutingResult assignVcaMember(SharedPtr<AudioBus> vca, SharedPtr<AudioBus> member);
    AudioRoutingResult removeVcaMember(SharedPtr<AudioBus> vca, SharedPtr<AudioBus> member);
    std::vector<SharedPtr<AudioBus>> getVcaMembers(SharedPtr<AudioBus> vca) const;
    static QString routingResultDescription(AudioRoutingResult result);

    // 全体の実行。呼び出し側がsource busへ入力を積んだ後に呼ぶ。
    // グラフをトポロジカルソートして順次処理し、派生busを内部で更新する。
    void process(ArtifactCore::AudioSegment& finalOutput);

    SharedPtr<AudioBus> getMasterBus() const { return masterBus_; }
    int busCount() const;
    std::vector<ZeroString> busNamesZero() const;
    std::vector<String> busNames() const;
    SharedPtr<AudioBus> findBusByName(const String& name) const;
    SharedPtr<AudioBus> findBusByName(const QString& name) const;
    SharedPtr<AudioBus> findBusByName(const UniString& name) const;
    SharedPtr<AudioBus> findBusById(const Id& id) const;
    AudioBusKind busKind(SharedPtr<AudioBus> bus) const;

    std::vector<SharedPtr<AudioBus>> getAllBuses() const;
    qint64 graphLatencySamples() const;
    qint64 graphTailSamples() const;
    SharedPtr<AudioBus> getRoutingTarget(SharedPtr<AudioBus> bus) const;
    std::vector<std::pair<SharedPtr<AudioBus>, float>> getSideChainSends(SharedPtr<AudioBus> bus) const;
    bool isSideChainSendPreFader(SharedPtr<AudioBus> source, SharedPtr<AudioBus> target) const;

    QJsonObject serialize() const;
    bool deserialize(const QJsonObject& data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    SharedPtr<AudioBus> masterBus_;
};

} // namespace ArtifactCore
