module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>
export module Core.AI.DescriptionExamples;


import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// ExampleLayer Description
// ============================================================================

class ExampleLayerDescription : public IDescribable {
public:
    QString className() const override { return "ExampleLayer"; }
    
    LocalizedText briefDescription() const override {
        return loc(
            "Base class for all layer types in the composition.",
            "コンポジション内の全レイヤータイプの基底クラス。",
            "合成中所有图层类型的基类。"
        );
    }
    
    LocalizedText detailedDescription() const override {
        return loc(
            "ExampleLayer provides common functionality for all layers including "
            "transform, opacity, blend modes, and effect stacking.",
            "ExampleLayerは、トランスフォーム、不透明度、ブレンドモード、"
            "エフェクトスタックなど、全レイヤー共通の機能を提供します。",
            "ExampleLayer为所有图层提供通用功能，包括变换、不透明度、"
            "混合模式和效果堆栈。"
        );
    }
    
    QList<PropertyDescription> propertyDescriptions() const override {
        return {
            {"name", loc("Layer display name", "レイヤー表示名", "图层显示名称"), "QString"},
            {"opacity", loc("Layer opacity 0-1", "レイヤー不透明度 0-1", "图层不透明度0-1"), 
             "float", "1.0", "0.0", "1.0"},
            {"visible", loc("Whether layer is rendered", "レイヤーがレンダリングされるか", "图层是否渲染"), 
             "bool", "true"},
            {"blendMode", loc("How layer blends with below", "下のレイヤーとのブレンド方法", "与下图层的混合方式"), 
             "BlendMode", "Normal"}
        };
    }
    
    QList<MethodDescription> methodDescriptions() const override {
        return {
            {"setName", loc("Set layer display name", "レイヤー表示名を設定", "设置图层显示名称"), 
             "void", {"QString"}, {"name"}},
            {"duplicate", loc("Create a copy of this layer", "このレイヤーのコピーを作成", "创建此图层的副本"), 
             "ExampleLayer*"}
        };
    }
    
    QStringList relatedClasses() const override {
        return {"ArtifactComposition", "ArtifactAbstractEffect"};
    }
};

static AutoRegisterDescribable<ExampleLayerDescription> _reg_ExampleLayer("ExampleLayer");

} // namespace ArtifactCore