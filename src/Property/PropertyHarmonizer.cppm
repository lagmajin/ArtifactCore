module;

#include <vector>
#include <algorithm>
#include <numeric>
#include <QVariant>
#include <iostream>
#include <cmath>

module Property.Harmonizer;

import Property.Abstract;
import Property.Types;

namespace ArtifactCore {

double PropertyHarmonizer::calculateBaseline(const std::vector<double>& values, const HarmonizeOptions& options) {
    if (values.empty()) return 0.0;

    switch (options.baseline) {
        case HarmonizeBaseline::Mean: {
            double sum = std::accumulate(values.begin(), values.end(), 0.0);
            return sum / values.size();
        }
        case HarmonizeBaseline::Median: {
            std::vector<double> sorted = values;
            std::sort(sorted.begin(), sorted.end());
            size_t n = sorted.size();
            if (n % 2 == 0) {
                return (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
            } else {
                return sorted[n/2];
            }
        }
        case HarmonizeBaseline::Selected: {
            if (options.selectedIndex >= 0 && options.selectedIndex < static_cast<int>(values.size())) {
                return values[options.selectedIndex];
            }
            return values.front(); // fallback
        }
        default:
            return 0.0;
    }
}

void PropertyHarmonizer::applyHarmonize(std::vector<AbstractProperty*>& properties, const HarmonizeOptions& options) {
    if (properties.empty()) return;

    // Collect values (assuming Float for now)
    std::vector<double> values;
    std::vector<AbstractProperty*> validProps;
    
    for (auto* prop : properties) {
        if (!prop || prop->getType() != PropertyType::Float) continue;
        
        bool ok;
        double val = prop->getValue().toDouble(&ok);
        if (ok) {
            values.push_back(val);
            validProps.push_back(prop);
        }
    }

    if (values.empty()) return;

    double baseline = calculateBaseline(values, options);

    if (options.mode == HarmonizeMode::Blend) {
        for (size_t i = 0; i < validProps.size(); ++i) {
            double current = values[i];
            double nextVal = current + (baseline - current) * std::clamp(options.intensity, 0.0, 1.0);
            validProps[i]->setValue(QVariant(nextVal));
        }
    } 
    else if (options.mode == HarmonizeMode::Normalize) {
        double compressionFactor = 1.0 - std::clamp(options.intensity, 0.0, 1.0);
        
        for (size_t i = 0; i < validProps.size(); ++i) {
            double current = values[i];
            double deviation = current - baseline;
            double nextVal = baseline + (deviation * compressionFactor);
            validProps[i]->setValue(QVariant(nextVal));
        }
    }
    else if (options.mode == HarmonizeMode::Distribute) {
        std::vector<size_t> indices(values.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&values](size_t a, size_t b) {
            return values[a] < values[b];
        });

        if (values.size() > 1) {
            double minV = values[indices.front()];
            double maxV = values[indices.back()];
            double range = maxV - minV;
            
            double step = range / (values.size() - 1);
            double startVal = baseline - step * (values.size() - 1) / 2.0;

            for (size_t i = 0; i < indices.size(); ++i) {
                double targetVal = startVal + step * i;
                double current = values[indices[i]];
                
                double nextVal = current + (targetVal - current) * std::clamp(options.intensity, 0.0, 1.0);
                validProps[indices[i]]->setValue(QVariant(nextVal));
            }
        }
    }
}

} // namespace ArtifactCore
