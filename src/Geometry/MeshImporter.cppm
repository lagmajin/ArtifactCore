module;
#include "ufbx.h"
#include <QString>
#include <QDebug>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <filesystem>

module MeshImporter;

import Mesh;
import Utils.String.UniString;

namespace ArtifactCore {

    class MeshImporter::Impl {
    public:
        Impl() {}
        ~Impl() {}

        std::shared_ptr<Mesh> loadWithUfbx(const QString& path) {
            ufbx_load_opts opts = {};
            opts.generate_missing_normals = true;

            ufbx_error error;
            ufbx_scene* scene = ufbx_load_file(path.toStdString().c_str(), &opts, &error);

            if (!scene) {
                qWarning() << "ufbx failed to load:" << path << "-" << error.description.data;
                return nullptr;
            }

            auto mesh = std::make_shared<Mesh>();
            int totalVertices = 0;
            for (size_t i = 0; i < scene->meshes.count; ++i) {
                totalVertices += (int)scene->meshes[i]->num_indices;
            }
            
            mesh->setVertexCount(totalVertices);
            auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
            auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
            auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");

            int vertexOffset = 0;
            for (size_t i = 0; i < scene->meshes.count; ++i) {
                ufbx_mesh* srcMesh = scene->meshes[i];
                for (size_t f = 0; f < srcMesh->num_faces; ++f) {
                    ufbx_face face = srcMesh->faces[f];
                    QVector<int> polyIndices;
                    polyIndices.reserve((int)face.num_indices);

                    for (size_t vi = 0; vi < face.num_indices; ++vi) {
                        size_t idx = face.index_begin + vi;
                        int outIdx = vertexOffset + (int)idx;
                        
                        ufbx_vec3 pos = ufbx_get_vertex_vec3(&srcMesh->vertex_position, idx);
                        (*posAttr)[outIdx] = QVector3D(pos.x, pos.y, pos.z);

                        ufbx_vec3 norm = ufbx_get_vertex_vec3(&srcMesh->vertex_normal, idx);
                        (*normAttr)[outIdx] = QVector3D(norm.x, norm.y, norm.z);

                        if (srcMesh->vertex_uv.exists) {
                            ufbx_vec2 uv = ufbx_get_vertex_vec2(&srcMesh->vertex_uv, idx);
                            (*uvAttr)[outIdx] = QVector2D(uv.x, uv.y);
                        }
                        polyIndices.push_back(outIdx);
                    }
                    mesh->addPolygon(polyIndices);
                }
                vertexOffset += (int)srcMesh->num_indices;
            }

            ufbx_free_scene(scene);
            mesh->updateBounds();
            return mesh;
        }
    };

    MeshImporter::MeshImporter() : impl_(new Impl()) {}
    MeshImporter::~MeshImporter() { delete impl_; }

    std::shared_ptr<Mesh> MeshImporter::importMeshFromFile(const UniString& path) {
        QString qpath = path.toQString();
        std::filesystem::path fsPath(qpath.toStdString());
        QString ext = QString::fromStdString(fsPath.extension().string().substr(1)).toLower();

        if (ext == "fbx" || ext == "obj") { // ufbx は obj もサポートしています
            return impl_->loadWithUfbx(qpath);
        }

        // TODO: glTF (fastgltf) の実装をここに追加予定
        qWarning() << "Unsupported mesh format:" << ext;
        return nullptr;
    }

};
