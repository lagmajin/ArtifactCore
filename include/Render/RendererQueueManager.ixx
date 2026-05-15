module;

#include <QObject>
#include <QString>
#include "../Define/DllExportMacro.hpp"
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Render.Queue.Manager;

import Render.JobModel;
import RendererQueueSetting;
import Utils.Id;

export namespace ArtifactCore {

// レンダリング実行用のコールバック関数型
// 引数: コンポジションID, フレーム番号, 出力パス
using RenderFrameFunc = std::function<void(const ArtifactCore::Id&, int, const QString&)>;

class LIBRARY_DLL_API RendererQueueManager:public QObject {
private:
 class Impl;
 Impl* impl_;
 explicit RendererQueueManager(QObject* parent = nullptr);
 //~RendererQueueManager();
 RendererQueueManager(const RendererQueueManager&) = delete;
 RendererQueueManager& operator=(const RendererQueueManager&) = delete;

public:
 //explicit RendererQueueManager(QObject* parent=nullptr);
 ~RendererQueueManager();
 RenderJobModel* model();
 static RendererQueueManager& instance();

 void startRendering();
 void startRenderingAllQueue();
 void addJob(const ArtifactCore::Id& compositionId, const QString& name);
 void clearRenderQueue();
 bool isRenderNow() const;

 // レンダリング実行関数の登録
 void setRenderFrameFunc(RenderFrameFunc func);

#ifdef _DEBUG
 void testRendering();
#else
 void testRendering() {};
#endif
};


};
 



