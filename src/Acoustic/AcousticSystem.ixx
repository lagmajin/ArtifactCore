module;

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

export module Artifact.Acoustic.System;

import Artifact.Acoustic;
import Artifact.Acoustic.ModalResonator;
import Artifact.Acoustic.RainModel;
import Artifact.Acoustic.WindModel;
import Artifact.Acoustic.FrictionModel;
import Artifact.Acoustic.Spatial;

export namespace Artifact::Acoustic {

    export class AcousticSystem {
    public:
        AcousticSystem() {
            m_materialLibrary["Steel"] = { 7.8f, 0.8f, 0.5f };
            m_materialLibrary["Wood"] = { 0.6f, 0.4f, 2.0f };
            m_materialLibrary["Glass"] = { 2.5f, 1.0f, 0.2f };

            m_rain = std::make_shared<RainModel>();
            m_wind = std::make_shared<WindModel>();
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
        void SetWindVelocity(float velocity_ms) { m_wind->SetVelocity(velocity_ms); }

        // --- フレーム更新 ---

        void Update(float dt) {
            for (auto& [id, model] : m_layerModels) model->Update(dt);
            for (auto& [id, model] : m_frictionModels) model->Update(dt);
            m_rain->Update(dt);
            m_wind->Update(dt);
        }

        // --- タスク発行と最適化 ---

        struct InternalTask {
            std::uint32_t layerId;
            AudioTask task;
        };

        std::vector<AudioTask> FetchTasks() {
            auto snapshot = FetchDebugSnapshot();
            std::vector<AudioTask> tasks;
            for (const auto& t : snapshot.activeTasks) {
                tasks.push_back({ t.type, t.amp, t.freq, 10.0f, t.duration, 0.0f, 1.0f, t.attenuation, 0 });
            }
            return tasks;
        }

        AcousticSnapshot FetchDebugSnapshot() {
            AcousticSnapshot snapshot;
            snapshot.frameNumber = m_frameCount++;
            
            std::vector<InternalTask> allInternalTasks;

            auto collect = [&](auto& models) {
                for (auto& [id, model] : models) {
                    auto tasks = model->GenerateTasks();
                    for (auto& t : tasks) {
                        if (m_layerSpatial.contains(id)) m_spatial->Calculate(m_layerSpatial[id], t);
                        allInternalTasks.push_back({ id, t });
                    }
                }
            };

            collect(m_layerModels);
            collect(m_frictionModels);

            // 環境音
            for (auto& t : m_rain->GenerateTasks()) allInternalTasks.push_back({ 0, t });
            for (auto& t : m_wind->GenerateTasks()) allInternalTasks.push_back({ 0, t });

            // ソート
            std::sort(allInternalTasks.begin(), allInternalTasks.end(), [](const InternalTask& a, const InternalTask& b) {
                return (a.task.amplitude * a.task.attenuation) > (b.task.amplitude * b.task.attenuation);
            });

            // LOD振り分け
            for (std::size_t i = 0; i < allInternalTasks.size(); ++i) {
                const auto& it = allInternalTasks[i];
                AudioTaskDebug debug = { it.layerId, it.task.type, it.task.frequency, it.task.amplitude, it.task.duration, it.task.attenuation };
                
                if (i < 32) {
                    snapshot.activeTasks.push_back(debug);
                } else {
                    snapshot.culledTasks.push_back(debug);
                }
            }

            return snapshot;
        }

    private:
        std::uint64_t m_frameCount = 0;
        std::map<std::string, MaterialProfile> m_materialLibrary;
        std::map<std::uint32_t, std::unique_ptr<IAcousticModel>> m_layerModels;
        std::map<std::uint32_t, std::unique_ptr<FrictionModel>> m_frictionModels;
        std::map<std::uint32_t, SpatialState> m_layerSpatial;
        
        std::shared_ptr<RainModel> m_rain;
        std::shared_ptr<WindModel> m_wind;
        std::unique_ptr<SpatialCalculator> m_spatial;
    };
}
