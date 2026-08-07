module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>
#include <string>

module Export.Lottie.RigExporter;

import Export.Lottie.Exporter;

namespace ArtifactCore::Export::Lottie {
namespace {

template <typename ValueBuilder>
std::vector<LottieKeyframe> sampleKeyframes(
    const std::vector<std::vector<double>>& samples,
    int startFrame,
    const ValueBuilder& builder) {
    std::vector<LottieKeyframe> result;
    result.reserve(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        LottieKeyframe key;
        key.t = static_cast<double>(startFrame) + static_cast<double>(index);
        key.s = samples[index];
        key.e = index + 1 < samples.size() ? samples[index + 1] : samples[index];
        builder(key);
        result.push_back(std::move(key));
    }
    return result;
}

} // namespace

bool appendRigBoneAnimation(Rig2D& rig,
                            const QString& boneName,
                            int startFrame,
                            int endFrame,
                            int framesPerSecond,
                            LottieLayer& outputLayer) {
    if (boneName.trimmed().isEmpty() || startFrame < 0 || endFrame < startFrame ||
        endFrame - startFrame > 100000 || framesPerSecond <= 0 || framesPerSecond > 1000) {
        return false;
    }
    Bone2D* bone = rig.findBone(boneName);
    if (!bone) return false;
    if (!std::isfinite(bone->length()) || bone->length() < 0.0f) return false;

    std::vector<std::vector<double>> positions;
    std::vector<std::vector<double>> scales;
    std::vector<std::vector<double>> rotations;
    positions.reserve(static_cast<std::size_t>(endFrame - startFrame + 1));
    scales.reserve(positions.capacity());
    rotations.reserve(positions.capacity());

    for (int frame = startFrame; frame <= endFrame; ++frame) {
        rig.evaluate(RationalTime::fromFrameCount(frame, framesPerSecond));
        const BoneTransform& transform = bone->resolvedTransform();
        if (!std::isfinite(transform.position.x()) || !std::isfinite(transform.position.y()) ||
            !std::isfinite(transform.scale.x()) || !std::isfinite(transform.scale.y()) ||
            !std::isfinite(transform.rotation)) return false;
        positions.push_back({transform.position.x(), transform.position.y()});
        scales.push_back({transform.scale.x() * 100.0, transform.scale.y() * 100.0});
        rotations.push_back({static_cast<double>(transform.rotation)});
    }

    outputLayer.ty = 4;
    outputLayer.nm = boneName.toStdString();
    outputLayer.ip = startFrame;
    outputLayer.op = endFrame + 1;
    outputLayer.position.keyframes = sampleKeyframes(positions, startFrame, [](LottieKeyframe&) {});
    outputLayer.scale.keyframes = sampleKeyframes(scales, startFrame, [](LottieKeyframe&) {});
    outputLayer.rotation.keyframes = sampleKeyframes(rotations, startFrame, [](LottieKeyframe&) {});
    outputLayer.anchor.k = {0.0, 0.0};
    outputLayer.opacity.k = {100.0};

    LottieShapeRect rectangle;
    const double boneLength = static_cast<double>(std::max(1.0f, bone->length()));
    rectangle.p.k = {boneLength * 0.5, 0.0};
    rectangle.s.k = {boneLength, 4.0};
    rectangle.r.k = {0.0};
    LottieShapeFill fill;
    fill.c.k = {0.35, 0.65, 1.0, 1.0};
    fill.o.k = {100.0};
    outputLayer.shapes.emplace_back(rectangle);
    outputLayer.shapes.emplace_back(fill);
    return true;
}

bool appendRigAnimation(Rig2D& rig,
                        int startFrame,
                        int endFrame,
                        int framesPerSecond,
                        std::vector<LottieLayer>& outputLayers) {
    if (startFrame < 0 || endFrame < startFrame || endFrame - startFrame > 100000 ||
        framesPerSecond <= 0 || framesPerSecond > 1000 || rig.bones().isEmpty()) {
        return false;
    }
    std::vector<LottieLayer> converted;
    converted.reserve(static_cast<std::size_t>(rig.bones().size()));
    int index = 1;
    std::vector<Bone2D*> convertedBones;
    convertedBones.reserve(static_cast<std::size_t>(rig.bones().size()));
    for (Bone2D* bone : rig.bones()) {
        if (!bone) continue;
        LottieLayer layer;
        layer.ind = index++;
        if (!appendRigBoneAnimation(rig, bone->name(), startFrame, endFrame,
                                     framesPerSecond, layer)) {
            return false;
        }
        convertedBones.push_back(bone);
        converted.push_back(std::move(layer));
    }
    if (converted.empty()) return false;
    for (std::size_t i = 0; i < converted.size(); ++i) {
        const Bone2D* parent = convertedBones[i]->parent();
        if (!parent) continue;
        for (std::size_t parentIndex = 0; parentIndex < convertedBones.size(); ++parentIndex) {
            if (convertedBones[parentIndex] == parent) {
                converted[i].parent = converted[parentIndex].ind;
                break;
            }
        }
    }
    outputLayers.insert(outputLayers.end(),
                        std::make_move_iterator(converted.begin()),
                        std::make_move_iterator(converted.end()));
    return true;
}

bool appendRigToDocument(Rig2D& rig,
                         int width,
                         int height,
                         double frameRate,
                         int startFrame,
                         int endFrame,
                         const QString& name,
                         LottieDocument& document) {
    if (width <= 0 || height <= 0 || !std::isfinite(frameRate) || frameRate <= 0.0 ||
        frameRate > 1000.0 || startFrame < 0 || endFrame < startFrame ||
        endFrame - startFrame > 100000 || name.trimmed().isEmpty()) {
        return false;
    }
    const int roundedRate = std::clamp(static_cast<int>(std::lround(frameRate)), 1, 1000);
    std::vector<LottieLayer> layers;
    if (!appendRigAnimation(rig, startFrame, endFrame, roundedRate, layers)) return false;
    for (auto& layer : layers) {
        LottieExporter::compressKeyframes(layer.position.keyframes);
        LottieExporter::compressKeyframes(layer.scale.keyframes);
        LottieExporter::compressKeyframes(layer.rotation.keyframes);
    }
    if (const SkinMesh* mesh = rig.skinMesh()) {
        const auto& vertices = mesh->restPositions();
        const auto& triangles = mesh->triangles();
        if (!vertices.empty() && triangles.size() >= 3 && triangles.size() % 3 == 0) {
            LottieLayer skinLayer;
            skinLayer.ind = static_cast<int>(layers.size()) + 1;
            skinLayer.nm = name.toStdString() + std::string(" Skin");
            skinLayer.ty = 4;
            skinLayer.ip = startFrame;
            skinLayer.op = endFrame + 1;
            skinLayer.position.k = {0.0, 0.0};
            skinLayer.anchor.k = {0.0, 0.0};
            skinLayer.scale.k = {100.0, 100.0};
            skinLayer.rotation.k = {0.0};
            skinLayer.opacity.k = {100.0};
            for (std::size_t triangle = 0; triangle < triangles.size(); triangle += 3) {
                const uint32_t a = triangles[triangle];
                const uint32_t b = triangles[triangle + 1];
                const uint32_t c = triangles[triangle + 2];
                if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
                const QVector2D points[] = {vertices[a], vertices[b], vertices[c]};
                LottieShapePath path;
                path.closed = true;
                for (const auto& point : points) {
                    path.vertices.push_back(point.x());
                    path.vertices.push_back(point.y());
                    path.inTangents.push_back(0.0);
                    path.inTangents.push_back(0.0);
                    path.outTangents.push_back(0.0);
                    path.outTangents.push_back(0.0);
                }
                skinLayer.shapes.emplace_back(std::move(path));
            }
            if (!skinLayer.shapes.empty()) {
                LottieShapeFill fill;
                fill.c.k = {0.7, 0.7, 0.7, 1.0};
                fill.o.k = {100.0};
                skinLayer.shapes.emplace_back(std::move(fill));
                layers.push_back(std::move(skinLayer));
            }
        }
    }
    document.w = width;
    document.h = height;
    document.fr = frameRate;
    document.ip = startFrame;
    document.op = endFrame + 1;
    document.nm = name.toStdString();
    document.layers.insert(document.layers.end(),
                           std::make_move_iterator(layers.begin()),
                           std::make_move_iterator(layers.end()));
    return true;
}

} // namespace ArtifactCore::Export::Lottie
