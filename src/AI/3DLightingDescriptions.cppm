module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

module Core.AI.3DLightingDescriptions;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// Scene3D Description
// ============================================================================

class Scene3DDescription : public IDescribable {
public:
    QString className() const override { return "Scene3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "3D scene container with objects, lights, cameras, and environment.",
            "オブジェクト、ライト、カメラ、環境を持つ3Dシーンコンテナ。",
            "包含物体、灯光、摄像机和环境的3D场景容器。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"objectCount", loc("Number of 3D objects", "3Dオブジェクト数", "3D物体数"), "int"},
            {"lightCount", loc("Number of lights", "ライト数", "灯光数"), "int"},
            {"ambientColor", loc("Ambient light color", "アンビエントライト色", "环境光颜色"), "QColor", "#1a1a1a"},
            {"fogEnabled", loc("Enable distance fog", "距離フォグを有効化", "启用距离雾"), "bool", "false"},
            {"fogColor", loc("Fog color", "フォグ色", "雾颜色"), "QColor", "#808080"},
            {"fogDensity", loc("Fog density", "フォグ密度", "雾密度"), "float", "0.01"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"addObject", loc("Add 3D object to scene", "3Dオブジェクトをシーンに追加", "向场景添加3D物体"), 
             "void", {"Object3D*"}, {"object"}},
            {"addLight", loc("Add light to scene", "ライトをシーンに追加", "向场景添加灯光"), 
             "void", {"Light3D*"}, {"light"}},
            {"addCamera", loc("Add camera to scene", "カメラをシーンに追加", "向场景添加摄像机"), 
             "void", {"Camera3D*"}, {"camera"}},
            {"setSkybox", loc("Set skybox cubemap", "スカイボックスキューブマップを設定", "设置天空盒立方体贴图"), 
             "void", {"QString"}, {"cubemapPath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Object3D", "Light3D", "Camera3D", "ArtifactCameraLayer"};
    }
};

// ============================================================================
// Object3D Description
// ============================================================================

class Object3DDescription : public IDescribable {
public:
    QString className() const override { return "Object3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for all 3D objects with transform and material.",
            "トランスフォームとマテリアルを持つ全3Dオブジェクトの基底クラス。",
            "具有变换和材质的所有3D物体的基类。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("World position", "ワールド位置", "世界坐标位置"), "QVector3D", "(0, 0, 0)"},
            {"rotation", loc("Euler rotation angles", "オイラー回転角度", "欧拉旋转角度"), "QVector3D", "(0, 0, 0)"},
            {"scale", loc("Scale factors", "スケール係数", "缩放系数"), "QVector3D", "(1, 1, 1)"},
            {"visible", loc("Whether object renders", "オブジェクトがレンダリングされるか", "物体是否渲染"), "bool", "true"},
            {"castShadow", loc("Cast shadows onto others", "他に影を投射するか", "是否向其他物体投射阴影"), "bool", "true"},
            {"receiveShadow", loc("Receive shadows from others", "他から影を受けるか", "是否接收其他物体的阴影"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Scene3D", "Mesh3D", "Material3D"};
    }
};

// ============================================================================
// Mesh3D Description
// ============================================================================

class Mesh3DDescription : public IDescribable {
public:
    QString className() const override { return "Mesh3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "3D mesh geometry with vertices, faces, and UV coordinates.",
            "頂点、面、UV座標を持つ3Dメッシュジオメトリ。",
            "具有顶点、面和UV坐标的3D网格几何体。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"vertexCount", loc("Number of vertices", "頂点数", "顶点数"), "int"},
            {"faceCount", loc("Number of triangles", "三角形数", "三角形数"), "int"},
            {"hasUVs", loc("Has UV coordinates", "UV座標を持つか", "是否有UV坐标"), "bool"},
            {"hasNormals", loc("Has vertex normals", "頂点法線を持つか", "是否有顶点法线"), "bool"},
            {"boundingBox", loc("Axis-aligned bounding box", "軸平行境界ボックス", "轴对齐包围盒"), "QRectF"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"loadOBJ", loc("Load from OBJ file", "OBJファイルから読み込み", "从OBJ文件加载"), 
             "bool", {"QString"}, {"filePath"}},
            {"loadFBX", loc("Load from FBX file", "FBXファイルから読み込み", "从FBX文件加载"), 
             "bool", {"QString"}, {"filePath"}},
            {"loadGLTF", loc("Load from glTF/GLB file", "glTF/GLBファイルから読み込み", "从glTF/GLB文件加载"), 
             "bool", {"QString"}, {"filePath"}},
            {"createPlane", loc("Create plane geometry", "平面ジオメトリを作成", "创建平面几何体"), 
             "void", {"float", "float"}, {"width", "height"}},
            {"createSphere", loc("Create sphere geometry", "球ジオメトリを作成", "创建球体几何体"), 
             "void", {"float", "int"}, {"radius", "segments"}},
            {"createBox", loc("Create box geometry", "ボックスジオメトリを作成", "创建盒子几何体"), 
             "void", {"QVector3D"}, {"size"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Object3D", "Material3D"};
    }
};

// ============================================================================
// Light3D Description
// ============================================================================

class Light3DDescription : public IDescribable {
public:
    QString className() const override { return "Light3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for 3D light sources illuminating the scene.",
            "シーンを照らす3Dライトソースの基底クラス。",
            "照亮场景的3D光源基类。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"type", loc("Light type (Point, Directional, Spot)", "ライトタイプ（ポイント、ディレクショナル、スポット）", "灯光类型（点、方向、聚光）"), "LightType"},
            {"color", loc("Light color", "ライト色", "灯光颜色"), "QColor", "white"},
            {"intensity", loc("Brightness multiplier", "明るさ倍率", "亮度倍数"), "float", "1.0"},
            {"enabled", loc("Whether light is active", "ライトが有効か", "灯光是否激活"), "bool", "true"},
            {"castShadow", loc("Cast shadows", "影を投射するか", "是否投射阴影"), "bool", "true"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"PointLight", "DirectionalLight", "SpotLight", "Scene3D"};
    }
};

// ============================================================================
// PointLight Description
// ============================================================================

class PointLightDescription : public IDescribable {
public:
    QString className() const override { return "PointLight"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Omnidirectional light emanating from a point in space.",
            "空間内のポイントから放射される全方向ライト。",
            "从空间中的点发出的全向光。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Light position in space", "空間内のライト位置", "空间中的灯光位置"), "QVector3D"},
            {"range", loc("Maximum influence distance", "最大影響距離", "最大影响距离"), "float", "10.0"},
            {"attenuation", loc("Distance falloff factor", "距離減衰係数", "距离衰减系数"), "float", "1.0"},
            {"radius", loc("Light source radius for soft shadows", "ソフトシャドウ用の光源半径", "用于柔和阴影的光源半径"), "float", "0.1"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Light3D", "SpotLight", "DirectionalLight"};
    }
};

// ============================================================================
// DirectionalLight Description
// ============================================================================

class DirectionalLightDescription : public IDescribable {
public:
    QString className() const override { return "DirectionalLight"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Infinite distance light with parallel rays, like the sun.",
            "太陽のような平行光線を持つ無限距離ライト。",
            "具有平行光线的无限远距离光，如太阳。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"direction", loc("Light direction vector", "ライト方向ベクトル", "灯光方向向量"), "QVector3D", "(0, -1, 0)"},
            {"shadowMapSize", loc("Shadow map resolution", "シャドウマップ解像度", "阴影贴图分辨率"), "int", "2048"},
            {"shadowBias", loc("Shadow depth bias", "シャドウ深度バイアス", "阴影深度偏移"), "float", "0.001"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Light3D", "PointLight", "SpotLight"};
    }
};

// ============================================================================
// SpotLight Description
// ============================================================================

class SpotLightDescription : public IDescribable {
public:
    QString className() const override { return "SpotLight"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Cone-shaped light with adjustable inner and outer angles.",
            "調整可能な内側・外側角度を持つコーン形状ライト。",
            "具有可调节内外角度的锥形光。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Light position", "ライト位置", "灯光位置"), "QVector3D"},
            {"direction", loc("Cone direction", "コーン方向", "锥体方向"), "QVector3D", "(0, -1, 0)"},
            {"innerAngle", loc("Full brightness angle (degrees)", "最大輝度角度（度）", "全亮度角度（度）"), "float", "15.0"},
            {"outerAngle", loc("Fade out angle (degrees)", "フェードアウト角度（度）", "衰减角度（度）"), "float", "30.0"},
            {"range", loc("Maximum distance", "最大距離", "最大距离"), "float", "20.0"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Light3D", "PointLight", "DirectionalLight"};
    }
};

// ============================================================================
// Material3D Description
// ============================================================================

class Material3DDescription : public IDescribable {
public:
    QString className() const override { return "Material3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Surface appearance properties for 3D rendering.",
            "3Dレンダリング用の表面外観プロパティ。",
            "用于3D渲染的表面外观属性。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"baseColor", loc("Diffuse/albedo color", "ディフューズ/アルベド色", "漫反射/反照率颜色"), "QColor", "#808080"},
            {"metallic", loc("Metallic factor (0-1)", "メタリック係数（0-1）", "金属系数（0-1）"), "float", "0.0"},
            {"roughness", loc("Surface roughness (0-1)", "表面粗さ（0-1）", "表面粗糙度（0-1）"), "float", "0.5"},
            {"emissive", loc("Self-illumination color", "自己発光色", "自发光颜色"), "QColor", "black"},
            {"emissiveStrength", loc("Emission intensity", "発光強度", "发光强度"), "float", "1.0"},
            {"opacity", loc("Transparency (0-1)", "透明度（0-1）", "不透明度（0-1）"), "float", "1.0"},
            {"doubleSided", loc("Render both sides", "両面をレンダリング", "双面渲染"), "bool", "false"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setDiffuseMap", loc("Set diffuse/albedo texture", "ディフューズ/アルベドテクスチャを設定", "设置漫反射/反照率纹理"), 
             "void", {"QString"}, {"texturePath"}},
            {"setNormalMap", loc("Set normal/bump texture", "法線/バンプテクスチャを設定", "设置法线/凹凸纹理"), 
             "void", {"QString"}, {"texturePath"}},
            {"setMetallicMap", loc("Set metallic texture", "メタリックテクスチャを設定", "设置金属纹理"), 
             "void", {"QString"}, {"texturePath"}},
            {"setRoughnessMap", loc("Set roughness texture", "ラフネステクスチャを設定", "设置粗糙度纹理"), 
             "void", {"QString"}, {"texturePath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Mesh3D", "Shader3D"};
    }
};

// ============================================================================
// Shader3D Description
// ============================================================================

class Shader3DDescription : public IDescribable {
public:
    QString className() const override { return "Shader3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Custom GLSL/HLSL shader for 3D rendering.",
            "3Dレンダリング用のカスタムGLSL/HLSLシェーダー。",
            "用于3D渲染的自定义GLSL/HLSL着色器。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "Shader3D allows custom vertex and fragment shaders for advanced 3D effects. "
            "Shaders can access transform matrices, lighting data, and material properties.",
            "Shader3Dは高度な3Dエフェクトのためのカスタム頂点シェーダーとフラグメントシェーダーを可能にします。"
            "シェーダーは変換行列、ライティングデータ、マテリアルプロパティにアクセスできます。",
            "Shader3D允许用于高级3D效果的自定义顶点和片段着色器。"
            "着色器可以访问变换矩阵、光照数据和材质属性。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"vertexShader", loc("Vertex shader source", "頂点シェーダーソース", "顶点着色器源码"), "QString"},
            {"fragmentShader", loc("Fragment shader source", "フラグメントシェーダーソース", "片段着色器源码"), "QString"},
            {"blendMode", loc("Alpha blending mode", "アルファブレンドモード", "Alpha混合模式"), "BlendMode", "Normal"},
            {"cullMode", loc("Backface culling", "バックフェイスカリング", "背面剔除"), "CullMode", "Back"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"compile", loc("Compile shader programs", "シェーダープログラムをコンパイル", "编译着色器程序"), 
             "bool"},
            {"setUniform", loc("Set shader uniform value", "シェーダーuniform値を設定", "设置着色器uniform值"), 
             "void", {"QString", "QVariant"}, {"name", "value"}},
            {"loadFromFile", loc("Load shader from files", "ファイルからシェーダーを読み込み", "从文件加载着色器"), 
             "bool", {"QString", "QString"}, {"vertexPath", "fragmentPath"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Material3D", "Mesh3D"};
    }
};

// ============================================================================
// Camera3D Description
// ============================================================================

class Camera3DDescription : public IDescribable {
public:
    QString className() const override { return "Camera3D"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "3D camera with projection settings for scene rendering.",
            "シーンレンダリング用の投影設定を持つ3Dカメラ。",
            "具有场景渲染投影设置的3D摄像机。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"position", loc("Camera position", "カメラ位置", "摄像机位置"), "QVector3D", "(0, 0, 5)"},
            {"target", loc("Look-at target point", "注視点", "注视点"), "QVector3D", "(0, 0, 0)"},
            {"fov", loc("Field of view (degrees)", "視野角（度）", "视野角度（度）"), "float", "45.0"},
            {"nearPlane", loc("Near clip distance", "近クリップ距離", "近裁剪距离"), "float", "0.1"},
            {"farPlane", loc("Far clip distance", "遠クリップ距離", "远裁剪距离"), "float", "1000.0"},
            {"projection", loc("Perspective or Orthographic", "パースペクティブまたはオルソグラフィック", "透视或正交"), "ProjectionType", "Perspective"},
            {"orthographicSize", loc("Ortho view height", "オルソビュー高さ", "正交视图高度"), "float", "10.0"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"lookAt", loc("Point camera at target", "カメラをターゲットに向ける", "将摄像机指向目标"), 
             "void", {"QVector3D"}, {"target"}},
            {"orbit", loc("Orbit around target", "ターゲットの周りを周回", "围绕目标旋转"), 
             "void", {"float", "float"}, {"horizontalAngle", "verticalAngle"}},
            {"pan", loc("Move camera parallel to view", "ビューに平行にカメラを移動", "平行于视图移动摄像机"), 
             "void", {"float", "float"}, {"x", "y"}},
            {"zoom", loc("Move camera closer/farther", "カメラを近づける/遠ざける", "将摄像机移近/移远"), 
             "void", {"float"}, {"delta"}}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"Scene3D", "ArtifactCameraLayer"};
    }
};

// ============================================================================
// Register All Descriptions
// ============================================================================

static AutoRegisterDescribable<Scene3DDescription> _reg_Scene3D("Scene3D");
static AutoRegisterDescribable<Object3DDescription> _reg_Object3D("Object3D");
static AutoRegisterDescribable<Mesh3DDescription> _reg_Mesh3D("Mesh3D");
static AutoRegisterDescribable<Light3DDescription> _reg_Light3D("Light3D");
static AutoRegisterDescribable<PointLightDescription> _reg_PointLight("PointLight");
static AutoRegisterDescribable<DirectionalLightDescription> _reg_DirectionalLight("DirectionalLight");
static AutoRegisterDescribable<SpotLightDescription> _reg_SpotLight("SpotLight");
static AutoRegisterDescribable<Material3DDescription> _reg_Material3D("Material3D");
static AutoRegisterDescribable<Shader3DDescription> _reg_Shader3D("Shader3D");
static AutoRegisterDescribable<Camera3DDescription> _reg_Camera3D("Camera3D");

} // namespace ArtifactCore