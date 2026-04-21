module;

#include <vector>
#include <mutex>
#include <memory>
#include <optional>

export module Artifact.Diagnostic.Registry;

import Artifact.Acoustic;
import Artifact.ShaderNode.Core;

export namespace Artifact::Diagnostic {

    export class DiagnosticRegistry {
    public:
        static DiagnosticRegistry& Instance() {
            static DiagnosticRegistry instance;
            return instance;
        }

        // --- Acoustic Telemetry ---

        void SetAcousticSnapshot(const Acoustic::AcousticSnapshot& snapshot) {
            std::lock_guard<std::mutex> lock(m_acousticMutex);
            m_latestAcoustic = snapshot;
        }

        std::optional<Acoustic::AcousticSnapshot> GetLatestAcoustic() {
            std::lock_guard<std::mutex> lock(m_acousticMutex);
            return m_latestAcoustic;
        }

        // --- Render Graph Telemetry ---

        void UpdateGraphSnapshot(const std::vector<ShaderNode::NodeDebugInfo>& nodes) {
            std::lock_guard<std::mutex> lock(m_graphMutex);
            m_latestGraph = nodes;
        }

        std::vector<ShaderNode::NodeDebugInfo> GetLatestGraph() {
            std::lock_guard<std::mutex> lock(m_graphMutex);
            return m_latestGraph;
        }

    private:
        DiagnosticRegistry() = default;

        std::mutex m_acousticMutex;
        std::optional<Acoustic::AcousticSnapshot> m_latestAcoustic;

        std::mutex m_graphMutex;
        std::vector<ShaderNode::NodeDebugInfo> m_latestGraph;
    };
}
