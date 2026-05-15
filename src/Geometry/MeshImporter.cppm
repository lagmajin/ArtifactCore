module;
class tst_QList;
#include <memory>
#include <utility>

#define TINYOBJLOADER_IMPLEMENTATION
#include "ufbx.h"
#include <QDebug>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QStringView>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <tinyobjloader/tiny_obj_loader.h>

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
  QString lastBaseColorTexture_;
  QString lastMetallicRoughnessTexture_;
  QString lastNormalTexture_;
  QString lastEmissionTexture_;
  QString lastOcclusionTexture_;
  QString lastOpacityTexture_;

  static QString ufbxStringToQString(const ufbx_string& s)
  {
    if (!s.data || s.length == 0) {
      return {};
    }
    return QString::fromUtf8(s.data, static_cast<int>(s.length));
  }

  static QString resolveTexturePath(const QString& modelPath,
                                   const QString& texturePath)
  {
    if (texturePath.isEmpty()) {
      return {};
    }
    const QFileInfo texInfo(texturePath);
    if (texInfo.isAbsolute()) {
      return QDir::cleanPath(texInfo.absoluteFilePath());
    }
    const QFileInfo modelInfo(modelPath);
    return QDir(modelInfo.absolutePath()).absoluteFilePath(texturePath);
  }

  void setLastBaseColorTexture(const QString& path)
  {
    lastBaseColorTexture_ = QDir::cleanPath(path.trimmed());
  }

  void setLastMetallicRoughnessTexture(const QString& path)
  {
    lastMetallicRoughnessTexture_ = QDir::cleanPath(path.trimmed());
  }

  void setLastNormalTexture(const QString& path)
  {
    lastNormalTexture_ = QDir::cleanPath(path.trimmed());
  }

  void setLastEmissionTexture(const QString& path)
  {
    lastEmissionTexture_ = QDir::cleanPath(path.trimmed());
  }

  void setLastOcclusionTexture(const QString& path)
  {
    lastOcclusionTexture_ = QDir::cleanPath(path.trimmed());
  }

  void setLastOpacityTexture(const QString& path)
  {
    lastOpacityTexture_ = QDir::cleanPath(path.trimmed());
  }

  void detectTexturesFromUfbx(const QString& modelPath, ufbx_scene* scene)
  {
    if (!scene) {
      return;
    }

    for (size_t i = 0; i < scene->materials.count; ++i) {
      const ufbx_material* material = scene->materials[i];
      if (!material) {
        continue;
      }

      const auto assignTexture = [&](const ufbx_texture* texture, auto&& setter,
                                     const char* label) {
        if (!texture || texture->type != UFBX_TEXTURE_FILE) {
          return;
        }
        QString candidate = ufbxStringToQString(texture->absolute_filename);
        if (candidate.isEmpty()) {
          candidate = ufbxStringToQString(texture->relative_filename);
        }
        if (candidate.isEmpty()) {
          candidate = ufbxStringToQString(texture->filename);
        }
        if (candidate.isEmpty()) {
          return;
        }
        const QString resolved = resolveTexturePath(modelPath, candidate);
        if (resolved.isEmpty()) {
          return;
        }
        setter(resolved);
        qDebug() << "[MeshImporter] ufbx" << label << ":" << resolved;
      };

      assignTexture(material->pbr.base_color.texture, [&](const QString& p) { setLastBaseColorTexture(p); }, "base color texture");
      if (lastMetallicRoughnessTexture_.isEmpty()) {
        assignTexture(material->pbr.base_factor.texture, [&](const QString& p) { setLastMetallicRoughnessTexture(p); }, "metallic-roughness texture");
      }
      if (lastNormalTexture_.isEmpty()) {
        assignTexture(material->pbr.normal_map.texture, [&](const QString& p) { setLastNormalTexture(p); }, "normal texture");
      }
      if (lastEmissionTexture_.isEmpty()) {
        assignTexture(material->pbr.emission_color.texture, [&](const QString& p) { setLastEmissionTexture(p); }, "emission texture");
      }
      if (lastOpacityTexture_.isEmpty()) {
        assignTexture(material->pbr.opacity.texture, [&](const QString& p) { setLastOpacityTexture(p); }, "opacity texture");
      }
      if (lastOcclusionTexture_.isEmpty()) {
        assignTexture(material->pbr.ambient_occlusion.texture, [&](const QString& p) { setLastOcclusionTexture(p); }, "occlusion texture");
      }

      if (!lastBaseColorTexture_.isEmpty() &&
          !lastNormalTexture_.isEmpty() &&
          !lastEmissionTexture_.isEmpty() &&
          !lastOpacityTexture_.isEmpty() &&
          !lastOcclusionTexture_.isEmpty()) {
        return;
      }
    }
  }

  void detectTexturesFromTinyObj(const QString& modelPath,
                                 const tinyobj::ObjReader& reader)
  {
    const auto& materials = reader.GetMaterials();
    if (materials.empty()) {
      return;
    }

    for (const auto& material : materials) {
      const auto assignTexture = [&](const std::string& path, auto&& setter,
                                     const char* label) {
        const QString candidate = QString::fromStdString(path);
        if (candidate.isEmpty()) {
          return;
        }
        const QString resolved = resolveTexturePath(modelPath, candidate);
        if (resolved.isEmpty()) {
          return;
        }
        setter(resolved);
        qDebug() << "[MeshImporter] tinyobj" << label << ":" << resolved;
      };

      if (lastBaseColorTexture_.isEmpty()) {
        assignTexture(material.diffuse_texname,
                      [&](const QString& p) { setLastBaseColorTexture(p); },
                      "base color texture");
        if (lastBaseColorTexture_.isEmpty()) {
          assignTexture(material.ambient_texname,
                        [&](const QString& p) { setLastBaseColorTexture(p); },
                        "base color texture");
        }
      }
      if (lastMetallicRoughnessTexture_.isEmpty()) {
        assignTexture(material.roughness_texname,
                      [&](const QString& p) { setLastMetallicRoughnessTexture(p); },
                      "metallic-roughness texture");
        if (lastMetallicRoughnessTexture_.isEmpty()) {
          assignTexture(material.metallic_texname,
                        [&](const QString& p) { setLastMetallicRoughnessTexture(p); },
                        "metallic-roughness texture");
        }
      }
      if (lastNormalTexture_.isEmpty()) {
        assignTexture(material.normal_texname,
                      [&](const QString& p) { setLastNormalTexture(p); },
                      "normal texture");
        if (lastNormalTexture_.isEmpty()) {
          assignTexture(material.bump_texname,
                        [&](const QString& p) { setLastNormalTexture(p); },
                        "normal texture");
        }
      }
      if (lastEmissionTexture_.isEmpty()) {
        assignTexture(material.emissive_texname,
                      [&](const QString& p) { setLastEmissionTexture(p); },
                      "emission texture");
      }
      if (lastOpacityTexture_.isEmpty()) {
        assignTexture(material.alpha_texname,
                      [&](const QString& p) { setLastOpacityTexture(p); },
                      "opacity texture");
      }
    }
  }

  std::shared_ptr<Mesh> loadWithUfbx(const QString &path) {
    lastBackend_ = MeshImporter::Backend::Ufbx;
    lastError_.clear();
    lastBaseColorTexture_.clear();
    lastMetallicRoughnessTexture_.clear();
    lastNormalTexture_.clear();
    lastEmissionTexture_.clear();
    lastOcclusionTexture_.clear();
    lastOpacityTexture_.clear();

    // Detect glTF/glb from extension
    const QString lowerPath = path.toLower();
    const bool isGltf =
        lowerPath.endsWith(".gltf") || lowerPath.endsWith(".glb");
    const QString backendLabel =
        isGltf ? QStringLiteral("glTF via ufbx") : QStringLiteral("ufbx");
    if (isGltf) {
      lastBackend_ = MeshImporter::Backend::UfbxGltf;
    }

    ufbx_load_opts opts = {};
    opts.generate_missing_normals = true;

    ufbx_error error;
    ufbx_scene *scene =
        ufbx_load_file(path.toStdString().c_str(), &opts, &error);

    if (!scene) {
      lastError_ =
          QStringLiteral("%1: %2")
              .arg(backendLabel)
              .arg(QStringView{QString::fromUtf8(error.description.data)});
      qWarning() << backendLabel << "failed to load:" << path << "-"
                 << error.description.data;
      return nullptr;
    }

    auto mesh = std::make_shared<Mesh>();
    int totalVertices = 0;
    for (size_t i = 0; i < scene->meshes.count; ++i) {
      totalVertices += (int)scene->meshes[i]->num_indices;
    }

    if (totalVertices == 0) {
      ufbx_free_scene(scene);
      lastError_ = QStringLiteral("%1: no mesh data").arg(backendLabel);
      qWarning() << backendLabel << "loaded empty mesh:" << path;
      return nullptr;
    }

    mesh->setVertexCount(totalVertices);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
    auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");
    auto colorAttr = mesh->vertexAttributes().add<QVector4D>("color");

    int vertexOffset = 0;
    for (size_t i = 0; i < scene->meshes.count; ++i) {
      ufbx_mesh *srcMesh = scene->meshes[i];
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

    detectTexturesFromUfbx(path, scene);
    ufbx_free_scene(scene);
    mesh->updateBounds();
    return mesh;
  }

  std::shared_ptr<Mesh> loadWithTinyObj(const QString &path) {
    lastBackend_ = MeshImporter::Backend::TinyObj;
    lastError_.clear();
    lastBaseColorTexture_.clear();
    lastMetallicRoughnessTexture_.clear();
    lastNormalTexture_.clear();
    lastEmissionTexture_.clear();
    lastOcclusionTexture_.clear();
    lastOpacityTexture_.clear();
    tinyobj::ObjReaderConfig config;
    config.triangulate = false;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path.toStdString(), config)) {
      const auto &err = reader.Error();
      const auto &warn = reader.Warning();
      if (!warn.empty()) {
        qWarning() << "tinyobj warning:" << QString::fromStdString(warn);
      }
      if (!err.empty()) {
        lastError_ = QStringLiteral("tinyobj: %1")
                         .arg(QStringView{QString::fromStdString(err)});
        qWarning() << "tinyobj failed to load:" << path << "-"
                   << QString::fromStdString(err);
      } else {
        lastError_ = QStringLiteral("tinyobj: unknown error");
        qWarning() << "tinyobj failed to load:" << path;
      }
      return nullptr;
    }

    const auto &attrib = reader.GetAttrib();
    const auto &shapes = reader.GetShapes();
    if (shapes.empty()) {
      lastError_ = QStringLiteral("tinyobj: no shapes");
      qWarning() << "tinyobj loaded no shapes:" << path;
      return nullptr;
    }

    size_t totalVertices = 0;
    size_t totalPolygons = 0;
    for (const auto &shape : shapes) {
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
    for (const auto &shape : shapes) {
      size_t indexOffset = 0;
      for (size_t face = 0; face < shape.mesh.num_face_vertices.size();
           ++face) {
        const int fv = shape.mesh.num_face_vertices[face];
        QVector<int> polyIndices;
        polyIndices.reserve(fv);

        for (int v = 0; v < fv; ++v) {
          const tinyobj::index_t idx =
              shape.mesh.indices[indexOffset + static_cast<size_t>(v)];
          const int outIdx = static_cast<int>(vertexOffset++);

          QVector3D position(0.0f, 0.0f, 0.0f);
          if (idx.vertex_index >= 0) {
            const size_t vi = static_cast<size_t>(idx.vertex_index) * 3;
            if (vi + 2 < attrib.vertices.size()) {
              position =
                  QVector3D(attrib.vertices[vi + 0], attrib.vertices[vi + 1],
                            attrib.vertices[vi + 2]);
            }
          }
          (*posAttr)[outIdx] = position;

          QVector3D normal(0.0f, 0.0f, 1.0f);
          if (idx.normal_index >= 0) {
            const size_t ni = static_cast<size_t>(idx.normal_index) * 3;
            if (ni + 2 < attrib.normals.size()) {
              normal = QVector3D(attrib.normals[ni + 0], attrib.normals[ni + 1],
                                 attrib.normals[ni + 2]);
            }
          }
          (*normAttr)[outIdx] = normal;

          QVector2D uv(0.0f, 0.0f);
          if (idx.texcoord_index >= 0) {
            const size_t ti = static_cast<size_t>(idx.texcoord_index) * 2;
            if (ti + 1 < attrib.texcoords.size()) {
              uv =
                  QVector2D(attrib.texcoords[ti + 0], attrib.texcoords[ti + 1]);
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
    detectTexturesFromTinyObj(path, reader);
    return mesh;
  }
  std::shared_ptr<Mesh> loadPMD(const QString &path) {
    lastBackend_ = MeshImporter::Backend::PMD;
    lastError_.clear();
    lastBaseColorTexture_.clear();
    lastMetallicRoughnessTexture_.clear();
    lastNormalTexture_.clear();
    lastEmissionTexture_.clear();
    lastOcclusionTexture_.clear();
    lastOpacityTexture_.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      lastError_ = QStringLiteral("PMD: cannot open file");
      return nullptr;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    char magic[3];
    stream.readRawData(magic, 3);
    if (memcmp(magic, "Pmd", 3) != 0) {
      lastError_ = QStringLiteral("PMD: invalid magic number");
      return nullptr;
    }

    float version;
    stream >> version;

    char name[20];
    char comment[256];
    stream.readRawData(name, 20);
    stream.readRawData(comment, 256);

    quint32 vertexCount;
    stream >> vertexCount;

    if (vertexCount == 0 || vertexCount > 1000000) {
      lastError_ = QStringLiteral("PMD: invalid vertex count");
      return nullptr;
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->setVertexCount(static_cast<int>(vertexCount));

    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
    auto uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");

    for (quint32 i = 0; i < vertexCount; i++) {
      float x, y, z;
      float nx, ny, nz;
      float u, v;

      stream >> x >> y >> z;
      stream >> nx >> ny >> nz;
      stream >> u >> v;

      (*posAttr)[i] = QVector3D(x, y, z);
      (*normAttr)[i] = QVector3D(nx, ny, nz);
      (*uvAttr)[i] = QVector2D(u, 1.0f - v);

      // Skip bone weight data for now
      stream.skipRawData(6);
    }

    quint32 faceCount;
    stream >> faceCount;

    for (quint32 i = 0; i < faceCount / 3; i++) {
      quint16 i0, i1, i2;
      stream >> i0 >> i1 >> i2;
      mesh->addPolygon(
          {static_cast<int>(i0), static_cast<int>(i1), static_cast<int>(i2)});
    }

    mesh->updateBounds();

    qInfo() << "PMD loaded: " << vertexCount << " vertices, " << faceCount / 3
            << " faces";
    return mesh;
  }
};

MeshImporter::MeshImporter() : impl_(new Impl()) {}
MeshImporter::~MeshImporter() { delete impl_; }

std::shared_ptr<Mesh> MeshImporter::importMeshFromFile(const UniString &path) {
  QString qpath = path.toQString();
  const QString ext = QFileInfo(qpath).suffix().toLower();
  impl_->lastBackend_ = MeshImporter::Backend::None;
  impl_->lastError_.clear();
  impl_->lastBaseColorTexture_.clear();

  if (ext == QStringLiteral("fbx")) {
    return impl_->loadWithUfbx(qpath);
  }

  if (ext == QStringLiteral("obj")) {
    if (auto mesh = impl_->loadWithUfbx(qpath)) {
      return mesh;
    }
    qWarning() << "ufbx fallback to tinyobjloader for OBJ:" << qpath;
    return impl_->loadWithTinyObj(qpath);
  }

  if (ext == QStringLiteral("gltf") || ext == QStringLiteral("glb")) {
    return impl_->loadWithUfbx(qpath);
  }

  if (ext == QStringLiteral("pmd") || ext == QStringLiteral("pmx")) {
    return impl_->loadPMD(qpath);
  }

  qWarning() << "Unsupported mesh format:" << ext;
  impl_->lastError_ =
      QStringLiteral("unsupported format: %1").arg(QStringView{ext});
  return nullptr;
}

MeshImporter::Backend MeshImporter::lastBackend() const {
  return impl_ ? impl_->lastBackend_ : MeshImporter::Backend::None;
}

QString MeshImporter::lastError() const {
  return impl_ ? impl_->lastError_ : QString();
}

QString MeshImporter::lastBaseColorTexture() const {
  return impl_ ? impl_->lastBaseColorTexture_ : QString();
}

QString MeshImporter::lastMetallicRoughnessTexture() const {
  return impl_ ? impl_->lastMetallicRoughnessTexture_ : QString();
}

QString MeshImporter::lastNormalTexture() const {
  return impl_ ? impl_->lastNormalTexture_ : QString();
}

QString MeshImporter::lastEmissionTexture() const {
  return impl_ ? impl_->lastEmissionTexture_ : QString();
}

QString MeshImporter::lastOcclusionTexture() const {
  return impl_ ? impl_->lastOcclusionTexture_ : QString();
}

QString MeshImporter::lastOpacityTexture() const {
  return impl_ ? impl_->lastOpacityTexture_ : QString();
}

}; // namespace ArtifactCore
