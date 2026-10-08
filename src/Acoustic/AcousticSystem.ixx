module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

export module Artifact.Acoustic.System;

import Artifact.Acoustic;
import Artifact.Acoustic.ModalResonator;
import Artifact.Acoustic.RainModel;
import Artifact.Acoustic.WindModel;
import Artifact.Acoustic.WaveModel;
import Artifact.Acoustic.FrictionModel;
import Artifact.Acoustic.Spatial;
import Artifact.Acoustic.AudioTaskSynthesizer;
import Memory.TrackedPtr;
import Memory.SharedPtr;
import Container.NamedVector;

export namespace Artifact::Acoustic {

    export enum class RainImpactSurface : std::uint8_t {
        Solid,
        Water
    };

    using ArtifactCore::SharedPtr;
    using ArtifactCore::makeShared;

    export class AcousticSystem {
    public:
        AcousticSystem() {
            m_materialLibrary["Steel"] = { 7.8f, 0.8f, 0.5f };
            m_materialLibrary["Wood"] = { 0.6f, 0.4f, 2.0f };
            m_materialLibrary["Glass"] = { 2.5f, 1.0f, 0.2f };
            m_materialLibrary["Flesh"] = { 1.05f, 0.15f, 3.0f };
            m_materialLibrary["Foam"] = { 0.08f, 0.05f, 4.0f };
            m_materialLibrary["Rubber"] = { 1.1f, 0.45f, 2.5f };

            m_rain = makeShared<RainModel>();
            m_wind = makeShared<WindModel>();
            m_waves = makeShared<WaveModel>();
            m_spatial = std::make_unique<SpatialCalculator>();
        }

        // --- 外部入力 ---

        void UpdateLayerSpatial(std::uint32_t layerId, const Vector3& pos, const Vector3& vel) {
            m_layerSpatial[layerId] = { pos, vel };
        }

        void SetListener(const Vector3& pos, const Vector3& vel) {
            m_spatial->SetListener({ pos, vel });
        }

        void OnCollision(std::uint32_t layerId, const std::string& material, float impulse, float pos) {
            if (!m_layerModels.contains(layerId)) {
                auto profile = m_materialLibrary.contains(material) ? m_materialLibrary[material] : m_materialLibrary["Steel"];
                m_layerModels[layerId] = std::make_unique<ModalResonator>(profile);
            }
            m_layerModels[layerId]->Trigger(impulse, pos);
        }

        void OnFriction(std::uint32_t layerId, float velocity, float pressure) {
            if (!m_frictionModels.contains(layerId)) {
                m_frictionModels[layerId] = std::make_unique<FrictionModel>();
            }
            m_frictionModels[layerId]->SetRelativeVelocity(velocity);
            m_frictionModels[layerId]->SetPressure(pressure);
        }

        void SetRainIntensity(float dropsPerSec) { m_rain->SetIntensity(dropsPerSec); }
        void SetRainImpactSurface(RainImpactSurface surface) {
            m_rain->SetWaterImpact(surface == RainImpactSurface::Water);
        }
        void SetWindVelocity(float velocity_ms) { m_wind->SetVelocity(velocity_ms); }
        void SetWaveHeight(float normalizedHeight) { m_waves->SetWaveHeight(normalizedHeight); }
        void SetWavePeriod(float seconds) { m_waves->SetPeriod(seconds); }
        void SetWaveBreaking(float normalizedIntensity) { m_waves->SetBreaking(normalizedIntensity); }
        void SetWaveBubbleSamples(std::span<const AcousticBubbleSample> samples) {
            m_waves->SetBubbleSamples(samples);
        }

        // --- フレーム更新 ---

        void Update(float dt) {
            for (auto& [id, model] : m_layerModels) model->Update(dt);
            for (auto& [id, model] : m_frictionModels) model->Update(dt);
            m_rain->Update(dt);
            m_wind->Update(dt);
            m_waves->Update(dt);
        }

        // --- タスク発行と最適化 ---

        struct InternalTask {
            std::uint32_t layerId;
            AudioTask task;
        };

        std::vector<AudioTask> FetchTasks() {
            const AudioTaskBlock block = FetchAudioTasks();
            ArtifactCore::NamedVector<AudioTask> tasks{
                ArtifactCore::makeNamedVector<AudioTask>(ArtifactCore::ContainerName{"AcousticSystemAudioTasks"})};
            for (const AudioTask& task : block.view()) tasks.append(task);
            return tasks.toStdVector();
        }

        // Bounded audio handoff for block rendering; this path performs no heap allocations.
        AudioTaskBlock FetchAudioTasks() {
            AudioTaskBlock block;
            auto addTask = [&](std::uint32_t layerId, AudioTask task) {
                if (m_layerSpatial.contains(layerId)) {
                    m_spatial->Calculate(m_layerSpatial[layerId], task);
                }

                if (block.count < AudioTaskBlock::Capacity) {
                    block.tasks[block.count++] = task;
                    return;
                }

                std::size_t lowestIndex = 0;
                float lowestScore = taskPriority(block.tasks[0]);
                for (std::size_t i = 1; i < block.count; ++i) {
                    const float score = taskPriority(block.tasks[i]);
                    if (score < lowestScore) {
                        lowestIndex = i;
                        lowestScore = score;
                    }
                }
                if (taskPriority(task) > lowestScore) {
                    block.tasks[lowestIndex] = task;
                }
            };

            auto collect = [&](auto& models) {
                for (auto& [id, model] : models) {
                    const AudioTaskBatch batch = model->GenerateTasks();
                    for (const AudioTask& task : batch) addTask(id, task);
                }
            };

            collect(m_layerModels);
            collect(m_frictionModels);
            for (const AudioTask& task : m_rain->GenerateTasks()) addTask(0, task);
            for (const AudioTask& task : m_wind->GenerateTasks()) addTask(0, task);
            for (const AudioTask& task : m_waves->GenerateTasks()) addTask(0, task);

            std::sort(block.tasks.begin(), block.tasks.begin() + block.count,
                [](const AudioTask& a, const AudioTask& b) {
                    return AcousticSystem::taskPriority(a) > AcousticSystem::taskPriority(b);
                });
            return block;
        }

        // Writes caller-owned interleaved stereo PCM without allocating.
        // The caller must serialize this with Update and physical-event input.
        bool RenderAudioBlock(std::span<float> interleavedOutput, int sampleRate) {
            const AudioTaskBlock tasks = FetchAudioTasks();
            return m_audioSynthesizer.RenderStereo(tasks, interleavedOutput, sampleRate);
        }

        bool RenderAudioBlock(std::span<float> leftOutput,
                              std::span<float> rightOutput,
                              int sampleRate) {
            const AudioTaskBlock tasks = FetchAudioTasks();
            return m_audioSynthesizer.RenderPlanarAdd(
                tasks, leftOutput, rightOutput, sampleRate);
        }

        void ResetTransientState() {
            m_layerModels.clear();
            m_frictionModels.clear();
            m_layerSpatial.clear();
            m_audioSynthesizer.Reset();
        }

        AcousticSnapshot FetchDebugSnapshot() {
            AcousticSnapshot snapshot;
            snapshot.frameNumber = m_frameCount++;
            
            ArtifactCore::NamedVector<InternalTask> allInternalTasks{
                ArtifactCore::makeNamedVector<InternalTask>(ArtifactCore::ContainerName{"AcousticSystemInternalTasks"})};

            auto collect = [&](auto& models) {
                for (auto& [id, model] : models) {
                    const AudioTaskBatch tasks = model->GenerateTasks();
                    for (const AudioTask& sourceTask : tasks) {
                        AudioTask t = sourceTask;
                        if (m_layerSpatial.contains(id)) m_spatial->Calculate(m_layerSpatial[id], t);
                        allInternalTasks.append({ id, t });
                    }
                }
            };

            collect(m_layerModels);
            collect(m_frictionModels);

            // 環境音
            for (auto& t : m_rain->GenerateTasks()) allInternalTasks.append({ 0, t });
            for (auto& t : m_wind->GenerateTasks()) allInternalTasks.append({ 0, t });
            for (auto& t : m_waves->GenerateTasks()) allInternalTasks.append({ 0, t });

            // ソート
            std::sort(allInternalTasks.begin(), allInternalTasks.end(), [](const InternalTask& a, const InternalTask& b) {
                return AcousticSystem::taskPriority(a.task) >
                       AcousticSystem::taskPriority(b.task);
            });

            // LOD振り分け
            for (std::size_t i = 0; i < allInternalTasks.size(); ++i) {
                const auto& it = allInternalTasks[i];
                AudioTaskDebug debug = { it.layerId, it.task.type, it.task.frequency,
                    it.task.amplitude, it.task.qFactor, it.task.duration, it.task.pan,
                    it.task.doppler, it.task.attenuation, it.task.seed };
                
                if (i < 32) {
                    snapshot.activeTasks.push_back(debug);
                } else {
                    snapshot.culledTasks.push_back(debug);
                }
            }

            return snapshot;
        }

    private:
        static float taskPriority(const AudioTask& task) {
            const float amplitude = std::isfinite(task.amplitude)
                ? std::max(0.0f, task.amplitude) : 0.0f;
            const float attenuation = std::isfinite(task.attenuation)
                ? std::clamp(task.attenuation, 0.0f, 1.0f) : 0.0f;
            return amplitude * attenuation;
        }

        std::uint64_t m_frameCount = 0;
        std::map<std::string, MaterialProfile> m_materialLibrary;
        std::map<std::uint32_t, std::unique_ptr<IAcousticModel>> m_layerModels;
        std::map<std::uint32_t, std::unique_ptr<FrictionModel>> m_frictionModels;
        std::map<std::uint32_t, SpatialState> m_layerSpatial;
        
        SharedPtr<RainModel> m_rain;
        SharedPtr<WindModel> m_wind;
        SharedPtr<WaveModel> m_waves;
        std::unique_ptr<SpatialCalculator> m_spatial;
        AudioTaskSynthesizer m_audioSynthesizer;
    };
}
