module;
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include <QString>
#include <QDebug>

module MeshImporter;

import Mesh;
// import Utils.String.UniString; // •K—v‚É‰ž‚¶‚Ä—LŒø‰»

namespace ArtifactCore {

 class MeshImporter::Impl {
 public:
  Impl() {}
  ~Impl() {}
 };

 MeshImporter::MeshImporter() : impl_(new Impl()) {}
 MeshImporter::~MeshImporter() { delete impl_; }

 std::shared_ptr<Mesh> MeshImporter::importMeshFromFile(const UniString& path) {
  Assimp::Importer importer;
  QString qpath = path.toQString();
  const aiScene* scene = importer.ReadFile(qpath.toStdString(), aiProcess_Triangulate | aiProcess_GenNormals);
  if (!scene || !scene->HasMeshes()) {
   qWarning() << "Assimp failed to load mesh:" << qpath;
   return nullptr;
  }
  QVector<Vertex> vertices;
  QVector<unsigned int> indices;
  QVector<SubMesh> subMeshes;
  unsigned int vertexOffset = 0;
  for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
   const aiMesh* srcMesh = scene->mMeshes[m];
   unsigned int subMeshStart = indices.size();
   for (unsigned int i = 0; i < srcMesh->mNumVertices; ++i) {
    Vertex v;
    v.position = QVector3D(
     srcMesh->mVertices[i].x,
     srcMesh->mVertices[i].y,
     srcMesh->mVertices[i].z);
    v.normal = srcMesh->HasNormals() ? QVector3D(
     srcMesh->mNormals[i].x,
     srcMesh->mNormals[i].y,
     srcMesh->mNormals[i].z) : QVector3D();
    v.uv = (srcMesh->HasTextureCoords(0)) ? QVector2D(
     srcMesh->mTextureCoords[0][i].x,
     srcMesh->mTextureCoords[0][i].y) : QVector2D();
    // tangent, bitangent, color, boneWeights, boneIndices‚Í–¢‘Î‰ž
    vertices.push_back(v);
   }
   unsigned int indexStart = indices.size();
   for (unsigned int i = 0; i < srcMesh->mNumFaces; ++i) {
    const aiFace& face = srcMesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; ++j) {
     indices.push_back(face.mIndices[j] + vertexOffset);
    }
   }
   SubMesh subMesh;
   subMesh.name = QString::fromUtf8(srcMesh->mName.C_Str());
   subMesh.materialIndex = srcMesh->mMaterialIndex;
   subMesh.startIndex = indexStart;
   subMesh.indexCount = indices.size() - indexStart;
   subMeshes.push_back(subMesh);
   vertexOffset += srcMesh->mNumVertices;
  }
  auto mesh = std::make_shared<Mesh>();
  mesh->setVertices(vertices);
  mesh->setIndices(indices);
  for (const auto& sm : subMeshes) {
   mesh->addSubMesh(sm);
  }
  return mesh;
 }
};
