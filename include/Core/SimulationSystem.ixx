module;
#include <memory>
#include <string>

export module ArtifactCore.Core.Simulation;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




export namespace ArtifactCore {

    /**
     * @brief Base interface for all time-based simulation systems
     */
    class ISimulationSystem {
    public:
        virtual ~ISimulationSystem() = default;
        
        virtual void update(double deltaTime) = 0;
        virtual void reset() = 0;
        
        virtual const std::string& getName() const = 0;
        virtual void setPaused(bool paused) = 0;
        virtual bool isPaused() const = 0;
    };

    /**
     * @brief Manager for multiple simulation systems
     */
    class SimulationManager {
    public:
        static SimulationManager& instance() {
            static SimulationManager inst;
            return inst;
        }

        void addSystem(std::shared_ptr<ISimulationSystem> system) {
            systems_.push_back(system);
        }

        void update(double deltaTime) {
            for (auto& system : systems_) {
                if (!system->isPaused()) {
                    system->update(deltaTime);
                }
            }
        }

        void resetAll() {
            for (auto& system : systems_) {
                system->reset();
            }
        }

    private:
        std::vector<std::shared_ptr<ISimulationSystem>> systems_;
    };

}
