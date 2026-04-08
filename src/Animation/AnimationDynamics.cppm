module;
#include <utility>
#include <string>

module Animation.Dynamics;

namespace ArtifactCore {

DynamicsPreset presetByName(const std::string& name)
{
    if (name == "Smooth")  return DynamicsPreset::Smooth();
    if (name == "Bouncy")  return DynamicsPreset::Bouncy();
    if (name == "Jelly")   return DynamicsPreset::Jelly();
    if (name == "Heavy")   return DynamicsPreset::Heavy();
    if (name == "Floaty")  return DynamicsPreset::Floaty();
    if (name == "Rigid")   return DynamicsPreset::Rigid();
    return DynamicsPreset::Smooth(); // fallback
}

const char* presetName(const DynamicsPreset& p) noexcept
{
    if (p.stiffness == 80.0f  && p.damping == 16.0f) return "Smooth";
    if (p.stiffness == 200.0f && p.damping ==  8.0f) return "Bouncy";
    if (p.stiffness == 120.0f && p.damping ==  6.0f) return "Jelly";
    if (p.stiffness ==  40.0f && p.damping == 20.0f) return "Heavy";
    if (p.stiffness ==  20.0f && p.damping ==  4.0f) return "Floaty";
    if (p.stiffness == 500.0f && p.damping == 50.0f) return "Rigid";
    return "Custom";
}

} // namespace ArtifactCore
