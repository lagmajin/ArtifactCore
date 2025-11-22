module;
export module Asset.Manager;

export namespace ArtifactCore {

 class AssetManager {
  private:
   class Impl;
   Impl* impl_;
  public:
   AssetManager();
   ~AssetManager();
 };

};