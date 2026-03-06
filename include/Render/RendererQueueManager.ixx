module;

#include <QObject>
#include "../Define/DllExportMacro.hpp"
export module Render.Queue.Manager;

import std;

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
 



