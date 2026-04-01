module;
#define TINYOBJLOADER_IMPLEMENTATION
#include "ufbx.h"
#include <tinyobjloader/tiny_obj_loader.h>
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
        MeshImporter::Backend lastBackend_ = MeshImporter::Backend::None;
        QString lastError_;

        std::shared_ptr<Mesh> loadWithUfbx(const QString& path) {
            lastBackend_ = MeshImporter::Backend::Ufbx;
            lastError_.clear();

            // Detect glTF/glb from extension
            const QString lowerPath = path.toLower();
            const bool isGltf = lowerPath.endsWith(".gltf") || lowerPath.endsWith(".glb");
            if (isGltf) {
                lastBackend_ = MeshImporter::Backend::UfbxGltf;
            }

            ufbx_load_opts opts = {};
            opts.generate_missing_normals = true;

            ufbx_error error;
            ufbx_scene* scene = ufbx_load_file(path.toStdString().c_str(), &opts, &error);

            if (!scene) {
                lastError_ = QStringLiteral("ufbx: %1").arg(QString::fromUtf8(error.description.data));
                qWarning() << "ufbx failed to load:" << path << "-" << error.description.data;
                return nullptr;
            }

            auto mesh = std::make_shared<Mesh>();
            int totalVertices = 0;
            for (size_t i = 0; i < scene->meshes.count; ++i) {
                totalVertices += (int)scene->meshes[i]->num_indices;
            }
            
            if (totalVertices == 0) {
                ufbx_free_scene(scene);
                lastError_ = QStringLiteral("ufbx: no mesh data");
                qWarning() << "ufbx loaded empty mesh:" << path;
                return nullptr;
            }
            
            mesh->setVertexCount(totalVertices);
            auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
            auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
            auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");
            auto colorAttr = mesh->vertexAttributes().add<QVector4D>("color");

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

                        // Vertex colors (glTF often has these)
                        if (srcMesh->vertex_color.exists) {
                            ufbx_vec4 col = ufbx_get_vertex_vec4(&srcMesh->vertex_color, idx);
                            (*colorAttr)[outIdx] = QVector4D(col.x, col.y, col.z, col.w);
                        } else {
                            (*colorAttr)[outIdx] = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
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

        std::shared_ptr<Mesh> loadWithTinyObj(const QString& path) {
            lastBackend_ = MeshImporter::Backend::TinyObj;
            lastError_.clear();
            tinyobj::ObjReaderConfig config;
            config.triangulate = false;

            tinyobj::ObjReader reader;
            if (!reader.ParseFromFile(path.toStdString(), config)) {
                const auto& err = reader.Error();
                const auto& warn = reader.Warning();
                if (!warn.empty()) {
                    qWarning() << "tinyobj warning:" << QString::fromStdString(warn);
                }
                if (!err.empty()) {
                    lastError_ = QStringLiteral("tinyobj: %1").arg(QString::fromStdString(err));
                    qWarning() << "tinyobj failed to load:" << path << "-" << QString::fromStdString(err);
                } else {
                    lastError_ = QStringLiteral("tinyobj: unknown error");
                    qWarning() << "tinyobj failed to load:" << path;
                }
                return nullptr;
            }

            const auto& attrib = reader.GetAttrib();
            const auto& shapes = reader.GetShapes();
            if (shapes.empty()) {
                lastError_ = QStringLiteral("tinyobj: no shapes");
                qWarning() << "tinyobj loaded no shapes:" << path;
                return nullptr;
            }

            size_t totalVertices = 0;
            size_t totalPolygons = 0;
            for (const auto& shape : shapes) {
                totalVertices += shape.mesh.indices.size();
                totalPolygons += shape.mesh.num_face_vertices.size();
            }
            if (totalVertices == 0 || totalPolygons == 0) {
                lastError_ = QStringLiteral("tinyobj: empty geometry");
                qWarning() << "tinyobj loaded empty geometry:" << path;
                return nullptr;
            }

            auto mesh = std::make_shared<Mesh>();
            mesh->setVertexCount(static_cast<int>(totalVertices));
            auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
            auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
            auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");

            size_t vertexOffset = 0;
            for (const auto& shape : shapes) {
                size_t indexOffset = 0;
                for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
                    const int fv = shape.mesh.num_face_vertices[face];
                    QVector<int> polyIndices;
                    polyIndices.reserve(fv);

                    for (int v = 0; v < fv; ++v) {
                        const tinyobj::index_t idx = shape.mesh.indices[indexOffset + static_cast<size_t>(v)];
                        const int outIdx = static_cast<int>(vertexOffset++);

                        QVector3D position(0.0f, 0.0f, 0.0f);
                        if (idx.vertex_index >= 0) {
                            const size_t vi = static_cast<size_t>(idx.vertex_index) * 3;
                            if (vi + 2 < attrib.vertices.size()) {
                                position = QVector3D(
                                    attrib.vertices[vi + 0],
                                    attrib.vertices[vi + 1],
                                    attrib.vertices[vi + 2]);
                            }
                        }
                        (*posAttr)[outIdx] = position;

                        QVector3D normal(0.0f, 0.0f, 1.0f);
                        if (idx.normal_index >= 0) {
                            const size_t ni = static_cast<size_t>(idx.normal_index) * 3;
                            if (ni + 2 < attrib.normals.size()) {
                                normal = QVector3D(
                                    attrib.normals[ni + 0],
                                    attrib.normals[ni + 1],
                                    attrib.normals[ni + 2]);
                            }
                        }
                        (*normAttr)[outIdx] = normal;

                        QVector2D uv(0.0f, 0.0f);
                        if (idx.texcoord_index >= 0) {
                            const size_t ti = static_cast<size_t>(idx.texcoord_index) * 2;
                            if (ti + 1 < attrib.texcoords.size()) {
                                uv = QVector2D(
                                    attrib.texcoords[ti + 0],
                                    attrib.texcoords[ti + 1]);
                            }
                        }
                        (*uvAttr)[outIdx] = uv;

                        polyIndices.push_back(outIdx);
                    }

                    mesh->addPolygon(polyIndices);
                    indexOffset += static_cast<size_t>(fv);
                }
            }

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
        impl_->lastBackend_ = MeshImporter::Backend::None;
        impl_->lastError_.clear();

        if (ext == "fbx") {
            return impl_->loadWithUfbx(qpath);
        }

        if (ext == "obj") {
            if (auto mesh = impl_->loadWithUfbx(qpath)) {
                return mesh;
            }
            qWarning() << "ufbx fallback to tinyobjloader for OBJ:" << qpath;
            return impl_->loadWithTinyObj(qpath);
        }

        if (ext == "gltf" || ext == "glb") {
            return impl_->loadWithUfbx(qpath);
        }

        qWarning() << "Unsupported mesh format:" << ext;
        impl_->lastError_ = QStringLiteral("unsupported format: %1").arg(ext);
        return nullptr;
    }

    MeshImporter::Backend MeshImporter::lastBackend() const {
        return impl_ ? impl_->lastBackend_ : MeshImporter::Backend::None;
    }

    QString MeshImporter::lastError() const {
        return impl_ ? impl_->lastError_ : QString();
    }

};
