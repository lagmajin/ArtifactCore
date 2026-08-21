module;
#include <utility>
#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>
#include <memory>
export module Asset.Manager;

import AssetType;
import Memory.SharedPtr;

export namespace ArtifactCore {

 enum class SourceResolutionCandidateKind {
  None = 0,
  SavedAssetId,
  ProjectRelativePath,
  RegistryRelativePath,
  AbsolutePathFallback,
 };

 enum class SourceCandidateOutcome {
  KeptOriginalEmptyCandidate = 0,
  AdoptedExistingCandidate,
  AdoptedCandidateForEmptyOriginal,
  KeptOriginalMissingCandidate,
 };

 struct SourceCandidateResolution {
  SourceResolutionCandidateKind kind = SourceResolutionCandidateKind::None;
  bool adopted = false;
  QString originalPath;
  QString candidatePath;
  QString resolvedPath;
  SourceCandidateOutcome outcome = SourceCandidateOutcome::KeptOriginalEmptyCandidate;
 };

 QString projectRelativeSourceCandidate(
     const QString& projectDirectory, const QString& path);

 SourceCandidateResolution resolveProjectRelativeSource(
     const QString& projectDirectory,
     SourceResolutionCandidateKind kind,
     const QString& originalPath,
     const QString& relativeCandidate,
     bool adoptWhenOriginalEmpty);

 class AssetManager {
  private:
   class Impl;
   Impl* impl_;
  public:
   static AssetManager& instance();
   AssetManager();
   ~AssetManager();
   AssetManager(const AssetManager&) = delete;
   AssetManager& operator=(const AssetManager&) = delete;

   QUuid acquireSource(const QString& path, AssetType type);
   bool releaseSource(const QUuid& assetId);
   QUuid localizeSource(const QUuid& assetId);
   bool acquireExistingSource(const QUuid& assetId);
   void resetSourceRegistry();
   bool isLocalizedSource(const QUuid& assetId) const;
   QUuid sourceId(const QString& path) const;
   int useCount(const QUuid& assetId) const;
   std::uint64_t sourceVersion(const QUuid& assetId) const;
   std::uint64_t invalidateSource(const QUuid& assetId);
   SharedPtr<void> decodedPayload(
       const QUuid& assetId, std::uint64_t version,
       const QString& representation) const;
   SharedPtr<void> publishDecodedPayload(
       const QUuid& assetId, std::uint64_t version,
       const QString& representation, SharedPtr<void> payload);
   QJsonObject sourceRegistrySnapshot() const;
   bool restoreSourceRegistrySnapshot(const QJsonObject& snapshot);
   QJsonArray sourceHealthSnapshot() const;
 };

};
