module;
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
#include <QByteArray>
#include <QRegularExpression>
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
        const QString candidate = QString::fromUtf8(path.data(), static_cast<int>(path.size()));
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
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;

    ufbx_error error;
    const QByteArray pathUtf8 = path.toUtf8();
    ufbx_scene *scene =
        ufbx_load_file(pathUtf8.constData(), &opts, &error);

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
    const QByteArray pathUtf8 = path.toUtf8();
    if (!reader.ParseFromFile(pathUtf8.constData(), config)) {
      const auto &err = reader.Error();
      const auto &warn = reader.Warning();
      if (!warn.empty()) {
        qWarning() << "tinyobj warning:" << QString::fromUtf8(warn.data(), static_cast<int>(warn.size()));
      }
      if (!err.empty()) {
        lastError_ = QStringLiteral("tinyobj: %1")
                         .arg(QStringView{QString::fromUtf8(err.data(), static_cast<int>(err.size()))});
        qWarning() << "tinyobj failed to load:" << path << "-"
                   << QString::fromUtf8(err.data(), static_cast<int>(err.size()));
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

  static QVector3D parseUsdVec3fTuple(QStringView tupleText, bool* okOut = nullptr)
  {
    const QStringList parts =
        tupleText.toString().split(QChar(','), Qt::SkipEmptyParts);
    if (parts.size() != 3) {
      if (okOut) {
        *okOut = false;
      }
      return {};
    }

    bool okX = false;
    bool okY = false;
    bool okZ = false;
    const float x = parts[0].trimmed().toFloat(&okX);
    const float y = parts[1].trimmed().toFloat(&okY);
    const float z = parts[2].trimmed().toFloat(&okZ);
    const bool ok = okX && okY && okZ;
    if (okOut) {
      *okOut = ok;
    }
    return ok ? QVector3D(x, y, z) : QVector3D{};
  }

  static QVector<QVector3D> parseUsdVec3fArray(const QString& text,
                                               bool* okOut = nullptr)
  {
    QVector<QVector3D> result;
    const QRegularExpression tupleRegex(
        QStringLiteral(R"(\(\s*([^()]+?)\s*\))"));
    auto it = tupleRegex.globalMatch(text);
    while (it.hasNext()) {
      const auto match = it.next();
      bool tupleOk = false;
      const QVector3D value =
          parseUsdVec3fTuple(match.capturedView(1), &tupleOk);
      if (!tupleOk) {
        if (okOut) {
          *okOut = false;
        }
        return {};
      }
      result.push_back(value);
    }
    if (okOut) {
      *okOut = !result.isEmpty();
    }
    return result;
  }

  static QVector2D parseUsdVec2fTuple(QStringView tupleText, bool* okOut = nullptr)
  {
    const QStringList parts =
        tupleText.toString().split(QChar(','), Qt::SkipEmptyParts);
    if (parts.size() != 2) {
      if (okOut) {
        *okOut = false;
      }
      return {};
    }

    bool okX = false;
    bool okY = false;
    const float x = parts[0].trimmed().toFloat(&okX);
    const float y = parts[1].trimmed().toFloat(&okY);
    const bool ok = okX && okY;
    if (okOut) {
      *okOut = ok;
    }
    return ok ? QVector2D(x, y) : QVector2D{};
  }

  static QVector<QVector2D> parseUsdVec2fArray(const QString& text,
                                               bool* okOut = nullptr)
  {
    QVector<QVector2D> result;
    const QRegularExpression tupleRegex(
        QStringLiteral(R"(\(\s*([^()]+?)\s*\))"));
    auto it = tupleRegex.globalMatch(text);
    while (it.hasNext()) {
      const auto match = it.next();
      bool tupleOk = false;
      const QVector2D value =
          parseUsdVec2fTuple(match.capturedView(1), &tupleOk);
      if (!tupleOk) {
        if (okOut) {
          *okOut = false;
        }
        return {};
      }
      result.push_back(value);
    }
    if (okOut) {
      *okOut = !result.isEmpty();
    }
    return result;
  }

  static QVector<int> parseUsdIntArray(const QString& text, bool* okOut = nullptr)
  {
    QVector<int> result;
    const QStringList parts =
        text.split(QRegularExpression(QStringLiteral(R"([\s,]+)")),
                   Qt::SkipEmptyParts);
    result.reserve(parts.size());
    for (const QString& part : parts) {
      bool valueOk = false;
      const int value = part.trimmed().toInt(&valueOk);
      if (!valueOk) {
        if (okOut) {
          *okOut = false;
        }
        return {};
      }
      result.push_back(value);
    }
    if (okOut) {
      *okOut = !result.isEmpty();
    }
    return result;
  }

  static QVector3D computePolygonNormal(const QVector<QVector3D>& polygonPositions)
  {
    if (polygonPositions.size() < 3) {
      return QVector3D(0.0f, 0.0f, 1.0f);
    }

    QVector3D normal(0.0f, 0.0f, 0.0f);
    const QVector3D origin = polygonPositions[0];
    for (int i = 1; i + 1 < polygonPositions.size(); ++i) {
      const QVector3D a = polygonPositions[i] - origin;
      const QVector3D b = polygonPositions[i + 1] - origin;
      normal += QVector3D::crossProduct(a, b);
    }

    if (normal.lengthSquared() <= 1e-12f) {
      return QVector3D(0.0f, 0.0f, 1.0f);
    }
    return normal.normalized();
  }

  std::shared_ptr<Mesh> loadWithUsda(const QString& path) {
    lastBackend_ = MeshImporter::Backend::Usda;
    lastError_.clear();
    lastBaseColorTexture_.clear();
    lastMetallicRoughnessTexture_.clear();
    lastNormalTexture_.clear();
    lastEmissionTexture_.clear();
    lastOcclusionTexture_.clear();
    lastOpacityTexture_.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      lastError_ = QStringLiteral("usda: failed to open file");
      return nullptr;
    }

    const QString text = QString::fromUtf8(file.readAll());
    if (!text.contains(QStringLiteral("#usda"))) {
      lastError_ = QStringLiteral("usda: missing #usda header");
      return nullptr;
    }

    const QRegularExpression meshBlockRegex(
        QStringLiteral(R"(def\s+Mesh\s+"[^"]+"\s*\{([\s\S]*?)\n\})"));
    const auto meshBlockMatch = meshBlockRegex.match(text);
    if (!meshBlockMatch.hasMatch()) {
      lastError_ = QStringLiteral("usda: no Mesh prim found");
      return nullptr;
    }

    const QString meshBlock = meshBlockMatch.captured(1);
    const QRegularExpression pointsRegex(
        QStringLiteral(R"(point3f\[\]\s+points\s*=\s*\[([\s\S]*?)\])"));
    const QRegularExpression faceCountsRegex(
        QStringLiteral(R"(int\[\]\s+faceVertexCounts\s*=\s*\[([\s\S]*?)\])"));
    const QRegularExpression faceIndicesRegex(
        QStringLiteral(R"(int\[\]\s+faceVertexIndices\s*=\s*\[([\s\S]*?)\])"));
    const QRegularExpression normalsRegex(
        QStringLiteral(R"(normal3f\[\]\s+normals\s*=\s*\[([\s\S]*?)\])"));
    const QRegularExpression normalInterpolationRegex(
        QStringLiteral("uniform\\s+token\\s+normals:interpolation\\s*=\\s*\"([^\"]+)\""));
    const QRegularExpression stRegex(
        QStringLiteral(R"((?:texCoord2f|float2)\[\]\s+primvars:st\s*=\s*\[([\s\S]*?)\])"));
    const QRegularExpression stInterpolationRegex(
        QStringLiteral("uniform\\s+token\\s+primvars:st:interpolation\\s*=\\s*\"([^\"]+)\""));

    const auto pointsMatch = pointsRegex.match(meshBlock);
    const auto faceCountsMatch = faceCountsRegex.match(meshBlock);
    const auto faceIndicesMatch = faceIndicesRegex.match(meshBlock);
    if (!pointsMatch.hasMatch() || !faceCountsMatch.hasMatch() ||
        !faceIndicesMatch.hasMatch()) {
      lastError_ = QStringLiteral(
          "usda: Mesh prim is missing points or face topology arrays");
      return nullptr;
    }

    bool pointsOk = false;
    bool countsOk = false;
    bool indicesOk = false;
    const QVector<QVector3D> positions =
        parseUsdVec3fArray(pointsMatch.captured(1), &pointsOk);
    const QVector<int> faceCounts =
        parseUsdIntArray(faceCountsMatch.captured(1), &countsOk);
    const QVector<int> faceIndices =
        parseUsdIntArray(faceIndicesMatch.captured(1), &indicesOk);
    if (!pointsOk || !countsOk || !indicesOk || positions.isEmpty() ||
        faceCounts.isEmpty() || faceIndices.isEmpty()) {
      lastError_ = QStringLiteral("usda: failed to parse mesh arrays");
      return nullptr;
    }

    QVector<QVector3D> importedNormals;
    QVector<QVector2D> importedUvs;
    QString normalsInterpolation;
    QString stInterpolation;

    const auto normalsMatch = normalsRegex.match(meshBlock);
    if (normalsMatch.hasMatch()) {
      bool normalsOk = false;
      importedNormals = parseUsdVec3fArray(normalsMatch.captured(1), &normalsOk);
      if (!normalsOk) {
        lastError_ = QStringLiteral("usda: failed to parse normals");
        return nullptr;
      }
      const auto interpolationMatch = normalInterpolationRegex.match(meshBlock);
      normalsInterpolation = interpolationMatch.hasMatch()
                                 ? interpolationMatch.captured(1).trimmed()
                                 : QStringLiteral("vertex");
    }

    const auto stMatch = stRegex.match(meshBlock);
    if (stMatch.hasMatch()) {
      bool stOk = false;
      importedUvs = parseUsdVec2fArray(stMatch.captured(1), &stOk);
      if (!stOk) {
        lastError_ = QStringLiteral("usda: failed to parse primvars:st");
        return nullptr;
      }
      const auto interpolationMatch = stInterpolationRegex.match(meshBlock);
      stInterpolation = interpolationMatch.hasMatch()
                            ? interpolationMatch.captured(1).trimmed()
                            : QStringLiteral("vertex");
    }

    int expectedIndexCount = 0;
    for (int count : faceCounts) {
      if (count < 3) {
        lastError_ = QStringLiteral(
            "usda: faceVertexCounts contains a face with fewer than 3 vertices");
        return nullptr;
      }
      expectedIndexCount += count;
    }
    if (expectedIndexCount != faceIndices.size()) {
      lastError_ = QStringLiteral("usda: face topology array lengths do not match");
      return nullptr;
    }

    const bool normalsArePerVertex =
        !importedNormals.isEmpty() &&
        importedNormals.size() == positions.size() &&
        (normalsInterpolation.isEmpty() ||
         normalsInterpolation == QStringLiteral("vertex") ||
         normalsInterpolation == QStringLiteral("varying"));
    const bool normalsAreFaceVarying =
        !importedNormals.isEmpty() &&
        importedNormals.size() == faceIndices.size() &&
        normalsInterpolation == QStringLiteral("faceVarying");
    const bool uvsArePerVertex =
        !importedUvs.isEmpty() &&
        importedUvs.size() == positions.size() &&
        (stInterpolation.isEmpty() || stInterpolation == QStringLiteral("vertex") ||
         stInterpolation == QStringLiteral("varying"));
    const bool uvsAreFaceVarying =
        !importedUvs.isEmpty() &&
        importedUvs.size() == faceIndices.size() &&
        stInterpolation == QStringLiteral("faceVarying");
    const bool needsFaceVaryingExpansion =
        normalsAreFaceVarying || uvsAreFaceVarying;

    auto mesh = std::make_shared<Mesh>();
    const int meshVertexCount = needsFaceVaryingExpansion
                                    ? faceIndices.size()
                                    : positions.size();
    mesh->setVertexCount(meshVertexCount);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    auto normAttr = mesh->vertexAttributes().add<QVector3D>("normal");
    std::shared_ptr<MeshAttribute<QVector2D>> uvAttr;
    if (uvsArePerVertex || uvsAreFaceVarying) {
      uvAttr = mesh->vertexAttributes().add<QVector2D>("uv");
    }

    const bool shouldGenerateNormals =
        (!normalsArePerVertex && !normalsAreFaceVarying);
    int indexOffset = 0;
    for (int faceVertexCount : faceCounts) {
      QVector<int> polygon;
      QVector<QVector3D> polygonPositions;
      polygon.reserve(faceVertexCount);
      polygonPositions.reserve(faceVertexCount);

      for (int i = 0; i < faceVertexCount; ++i) {
        const int sourceVertexIndex = faceIndices[indexOffset];
        if (sourceVertexIndex < 0 || sourceVertexIndex >= positions.size()) {
          lastError_ = QStringLiteral(
              "usda: faceVertexIndices contains an out-of-range vertex index");
          return nullptr;
        }

        const int meshVertexIndex =
            needsFaceVaryingExpansion ? indexOffset : sourceVertexIndex;
        polygon.push_back(meshVertexIndex);
        polygonPositions.push_back(positions[sourceVertexIndex]);
        (*posAttr)[meshVertexIndex] = positions[sourceVertexIndex];

        if (normalsArePerVertex) {
          (*normAttr)[meshVertexIndex] = importedNormals[sourceVertexIndex];
        } else if (normalsAreFaceVarying) {
          (*normAttr)[meshVertexIndex] = importedNormals[indexOffset];
        } else {
          (*normAttr)[meshVertexIndex] = QVector3D(0.0f, 0.0f, 0.0f);
        }

        if (uvAttr) {
          if (uvsArePerVertex) {
            (*uvAttr)[meshVertexIndex] = importedUvs[sourceVertexIndex];
          } else if (uvsAreFaceVarying) {
            (*uvAttr)[meshVertexIndex] = importedUvs[indexOffset];
          } else {
            (*uvAttr)[meshVertexIndex] = QVector2D(0.0f, 0.0f);
          }
        }

        ++indexOffset;
      }

      if (shouldGenerateNormals) {
        const QVector3D polygonNormal = computePolygonNormal(polygonPositions);
        for (int vertexIndex : polygon) {
          (*normAttr)[vertexIndex] += polygonNormal;
        }
      }

      mesh->addPolygon(polygon);
    }

    if (shouldGenerateNormals) {
      for (int i = 0; i < normAttr->size(); ++i) {
        QVector3D normal = (*normAttr)[i];
        if (normal.lengthSquared() <= 1e-12f) {
          normal = QVector3D(0.0f, 0.0f, 1.0f);
        } else {
          normal.normalize();
        }
        (*normAttr)[i] = normal;
      }
    }

    mesh->updateBounds();
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

  if (ext == QStringLiteral("usda")) {
    return impl_->loadWithUsda(qpath);
  }

  if (ext == QStringLiteral("usd")) {
    QFile usdFile(qpath);
    if (usdFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QByteArray header = usdFile.read(64);
      if (header.contains("#usda")) {
        return impl_->loadWithUsda(qpath);
      }
    }
    qWarning() << "USD import requested but no binary/OpenUSD runtime is wired into MeshImporter:" << qpath;
    impl_->lastError_ = QStringLiteral(
        "USD import for .usd requires either ASCII USDA content or an OpenUSD runtime. "
        "This build only supports the ASCII USDA subset.");
    return nullptr;
  }

  if (ext == QStringLiteral("usdc") || ext == QStringLiteral("usdz")) {
    qWarning() << "USD import requested but no USD runtime is wired into MeshImporter:" << qpath;
    impl_->lastError_ = QStringLiteral(
        "USD import is not wired into this build yet (%1). "
        "This build currently supports ASCII USDA only; USDC/USDZ still require OpenUSD integration.")
                            .arg(QStringView{ext});
    return nullptr;
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
