module;
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <QString>
#include <QDebug>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

module MeshImporter;

import Mesh;
import Utils.String.UniString;

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
        
        // n-gon をそのまま保持するため、aiProcess_Triangulate を外す（またはオプションにする）
        // DDCグレードでは元のトポロジーが重要。
        const aiScene* scene = importer.ReadFile(qpath.toStdString(), aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);
        
        if (!scene || !scene->HasMeshes()) {
            qWarning() << "Assimp failed to load mesh:" << qpath;
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        
        // 全メッシュの合計頂点数を計算
        int totalVertices = 0;
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            totalVertices += scene->mMeshes[m]->mNumVertices;
        }
        
        mesh->setVertexCount(totalVertices);
        
        // アトリビュートの取得（無ければ作成される）
        auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
        auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
        auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");

        unsigned int vertexOffset = 0;
        
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* srcMesh = scene->mMeshes[m];
            
            // 頂点データのコピー
            for (unsigned int i = 0; i < srcMesh->mNumVertices; ++i) {
                int vIdx = vertexOffset + i;
                
                (*posAttr)[vIdx] = QVector3D(
                    srcMesh->mVertices[i].x,
                    srcMesh->mVertices[i].y,
                    srcMesh->mVertices[i].z
                );
                
                if (srcMesh->HasNormals()) {
                    (*normAttr)[vIdx] = QVector3D(
                        srcMesh->mNormals[i].x,
                        srcMesh->mNormals[i].y,
                        srcMesh->mNormals[i].z
                    );
                }
                
                if (srcMesh->HasTextureCoords(0)) {
                    (*uvAttr)[vIdx] = QVector2D(
                        srcMesh->mTextureCoords[0][i].x,
                        srcMesh->mTextureCoords[0][i].y
                    );
                }
            }
            
            // 面（ポリゴン）データのコピー (N-gon対応)
            for (unsigned int i = 0; i < srcMesh->mNumFaces; ++i) {
                const aiFace& face = srcMesh->mFaces[i];
                QVector<int> polyIndices;
                polyIndices.reserve(face.mNumIndices);
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    polyIndices.push_back(face.mIndices[j] + vertexOffset);
                }
                mesh->addPolygon(polyIndices);
            }
            
            vertexOffset += srcMesh->mNumVertices;
        }
        
        mesh->updateBounds();
        return mesh;
    }

};
