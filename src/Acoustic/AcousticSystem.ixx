export module Artifact.Acoustic.System;

import Artifact.Acoustic;
import Artifact.Acoustic.FrictionModel;
import Artifact.Acoustic.Spatial;

import <vector>;
import <memory>;
import <map>;
import <string>;
import <algorithm>;

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

        void UpdateLayerSpatial(uint32_t layerId, const Vector3& pos, const Vector3& vel) {
            m_layerSpatial[layerId] = { pos, vel };
        }

        void SetListener(const Vector3& pos, const Vector3& vel) {
            m_spatial->SetListener({ pos, vel });
        }

        void OnCollision(uint32_t layerId, const std::string& material, float impulse, float pos) {
            if (!m_layerModels.contains(layerId)) {
                auto profile = m_materialLibrary.contains(material) ? m_materialLibrary[material] : m_materialLibrary["Steel"];
                m_layerModels[layerId] = std::make_unique<ModalResonator>(profile);
            }
            m_layerModels[layerId]->Trigger(impulse, pos);
        }

        void OnFriction(uint32_t layerId, float velocity, float pressure) {
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

        std::vector<AudioTask> FetchTasks() {
            std::vector<AudioTask> rawTasks;
            
            // 全モデルからタスクを収集
            auto collect = [&](auto& models) {
                for (auto& [id, model] : models) {
                    auto tasks = model->GenerateTasks();
                    // 空間情報を反映
                    if (m_layerSpatial.contains(id)) {
                        for (auto& t : tasks) m_spatial->Calculate(m_layerSpatial[id], t);
                    }
                    rawTasks.insert(rawTasks.end(), tasks.begin(), tasks.end());
                }
            };

            collect(m_layerModels);
            collect(m_frictionModels);

            // 環境音は空間演算なし（または固定）
            auto rainTasks = m_rain->GenerateTasks();
            rawTasks.insert(rawTasks.end(), rainTasks.begin(), rainTasks.end());

            auto windTasks = m_wind->GenerateTasks();
            rawTasks.insert(rawTasks.end(), windTasks.begin(), windTasks.end());

            // --- LOD / カリング (最適化) ---
            // 振幅(amplitude)と減衰(attenuation)の積でソートし、上位32個に制限
            std::sort(rawTasks.begin(), rawTasks.end(), [](const AudioTask& a, const AudioTask& b) {
                return (a.amplitude * a.attenuation) > (b.amplitude * b.attenuation);
            });

            if (rawTasks.size() > 32) {
                rawTasks.resize(32);
            }

            return rawTasks;
        }

    private:
        std::map<std::string, MaterialProfile> m_materialLibrary;
        std::map<uint32_t, std::unique_ptr<IAcousticModel>> m_layerModels;
        std::map<uint32_t, std::unique_ptr<FrictionModel>> m_frictionModels;
        std::map<uint32_t, SpatialState> m_layerSpatial;
        
        std::shared_ptr<RainModel> m_rain;
        std::shared_ptr<WindModel> m_wind;
        std::unique_ptr<SpatialCalculator> m_spatial;
    };
}
