module;
#include <QString>
#include <vector>

export module Export.Lottie.RigExporter;

import ArtifactCore.Rig2D;
import Export.Lottie.Types;

export namespace ArtifactCore::Export::Lottie {

// Samples one Rig2D bone into a Lottie shape layer transform animation.
bool appendRigBoneAnimation(Rig2D& rig,
                            const QString& boneName,
                            int startFrame,
                            int endFrame,
                            int framesPerSecond,
                            LottieLayer& outputLayer);

bool appendRigAnimation(Rig2D& rig,
                        int startFrame,
                        int endFrame,
                        int framesPerSecond,
                        std::vector<LottieLayer>& outputLayers);

bool appendRigToDocument(Rig2D& rig,
                         int width,
                         int height,
                         double frameRate,
                         int startFrame,
                         int endFrame,
                         const QString& name,
                         LottieDocument& document);

} // namespace ArtifactCore::Export::Lottie
