module;
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iomanip>

module Color.Swatch;

import Color.Float;

namespace ArtifactCore {

class ColorSwatch::Impl {
public:
    std::string name;
    std::vector<SwatchEntry> entries;

    Impl(const std::string& n = "New Palette") : name(n) {}
};

ColorSwatch::ColorSwatch() : impl_(std::make_unique<Impl>()) {}
ColorSwatch::ColorSwatch(const std::string& name) : impl_(std::make_unique<Impl>(name)) {}

ColorSwatch::~ColorSwatch() = default;

ColorSwatch::ColorSwatch(const ColorSwatch& other) : impl_(std::make_unique<Impl>(other.impl_->name)) {
    impl_->entries = other.impl_->entries;
}

ColorSwatch::ColorSwatch(ColorSwatch&& other) noexcept = default;

ColorSwatch& ColorSwatch::operator=(const ColorSwatch& other) {
    if (this != &other) {
        impl_->name = other.impl_->name;
        impl_->entries = other.impl_->entries;
    }
    return *this;
}

ColorSwatch& ColorSwatch::operator=(ColorSwatch&& other) noexcept = default;

const std::string& ColorSwatch::getName() const { return impl_->name; }
void ColorSwatch::setName(const std::string& name) { impl_->name = name; }

size_t ColorSwatch::count() const { return impl_->entries.size(); }
const SwatchEntry& ColorSwatch::at(size_t index) const {
    static SwatchEntry empty;
    if (index >= impl_->entries.size()) return empty;
    return impl_->entries[index];
}

void ColorSwatch::addEntry(const SwatchEntry& entry) {
    impl_->entries.push_back(entry);
}

void ColorSwatch::addColor(const FloatColor& color, const std::string& name) {
    impl_->entries.emplace_back(color, name);
}

void ColorSwatch::removeAt(size_t index) {
    if (index < impl_->entries.size()) {
        impl_->entries.erase(impl_->entries.begin() + index);
    }
}

void ColorSwatch::clear() {
    impl_->entries.clear();
}

int ColorSwatch::findColor(const FloatColor& color, float tolerance) const {
    for (size_t i = 0; i < impl_->entries.size(); ++i) {
        const auto& c = impl_->entries[i].color;
        float dr = std::abs(c.r() - color.r());
        float dg = std::abs(c.g() - color.g());
        float db = std::abs(c.b() - color.b());
        float da = std::abs(c.a() - color.a());
        if (dr < tolerance && dg < tolerance && db < tolerance && da < tolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ColorSwatch::sort() {
    // 輝度によるソート例（将来的にオプションを増やす）
    std::sort(impl_->entries.begin(), impl_->entries.end(), [](const SwatchEntry& a, const SwatchEntry& b) {
        float la = a.color.averageRGB();
        float lb = b.color.averageRGB();
        return la < lb;
    });
}

bool ColorSwatch::load(const std::filesystem::path& path) {
    // 拡張子で判定（とりあえずGPLをメインにする）
    if (path.extension() == ".gpl") {
        return importGPL(path);
    }
    // バイナリまたはJSONの実装が必要な場合はここに追加
    return false;
}

bool ColorSwatch::save(const std::filesystem::path& path) const {
    if (path.extension() == ".gpl") {
        return exportGPL(path);
    }
    return false;
}

bool ColorSwatch::importGPL(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    if (!std::getline(file, line) || line.find("GIMP Palette") == std::string::npos) {
        return false;
    }

    clear();
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("Name:") == 0) {
            setName(line.substr(5));
            continue;
        }
        if (line.find("Columns:") == 0) continue;

        std::istringstream iss(line);
        int r, g, b;
        if (iss >> r >> g >> b) {
            std::string entryName;
            std::getline(iss, entryName);
            // 余分な空白を除去
            entryName.erase(0, entryName.find_first_not_of(" \t"));
            addColor(FloatColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f), entryName);
        }
    }
    return true;
}

bool ColorSwatch::exportGPL(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "GIMP Palette" << std::endl;
    file << "Name: " << getName() << std::endl;
    file << "Columns: 0" << std::endl;
    file << "#" << std::endl;

    for (const auto& entry : impl_->entries) {
        int r = static_cast<int>(std::clamp(entry.color.r() * 255.0f, 0.0f, 255.0f));
        int g = static_cast<int>(std::clamp(entry.color.g() * 255.0f, 0.0f, 255.0f));
        int b = static_cast<int>(std::clamp(entry.color.b() * 255.0f, 0.0f, 255.0f));
        
        file << std::setw(3) << r << " " 
             << std::setw(3) << g << " " 
             << std::setw(3) << b << " " 
             << entry.name << std::endl;
    }

    return true;
}

} // namespace ArtifactCore
