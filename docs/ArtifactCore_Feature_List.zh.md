# ArtifactCore 功能列表

最后更新: 2026-03-02

本文档提供 ArtifactCore 库的高层功能地图。
用于帮助开发者快速定位主要系统与模块。

## 核心架构

- 基于 C++20 Modules 的核心库
- 领域导向模块位于 `include/`
- 运行时实现位于 `src/`
- 优先采用与 UI 解耦、可复用的核心逻辑

## 功能领域

### 时间线与播放

- `Playback/PlaybackClock`: 高精度播放时钟、速度与循环控制
- `Frame/*`: 帧位置、范围、帧率、偏移与时间
- `Time/*`: 时间码、时长、有理时间、重映射辅助
- `Media/MediaPlaybackController`: 面向媒体的播放控制

### 输入与键位系统（Blender 风格基础）

- `UI/InputOperator`: 基于上下文的输入处理
- `UI/KeyMap`: 可配置键位映射定义与绑定
- `UI/InputEvent`: 规范化输入事件模型
- `ActionManager` 与 `Action`: 动作注册与执行中心

### 合成与图层基础

- `Composition/*`: 合成缓冲与预合成支持
- `Layer/*`: 图层状态、条带、混合/遮罩相关数据
- `Track/LayerTrack`: 图层轨道抽象

### 图像与图形

- `Image/*`: 图像容器与格式桥接
- `ImageProcessing/*`: OpenCV、DirectCompute、Halide 管线
- `Graphics/*`: GPU 纹理、着色器、PSO/缓存、渲染辅助
- `Render/*`: 渲染队列、设置、渲染任务元数据

### 视频与编解码

- `Codec/*`: MF/FFmpeg/GStreamer 解码与编码工具
- `Video/*`: 解码器/编码器接口与播放辅助
- `Media/*`: 媒体源、探测、视频帧/音频解码封装

### 音频

- `Audio/*`: 混音、渲染、声像、压混、环形缓冲、波形
- 音频格式/音频帧/提供者抽象

### 效果与处理

- `Effect/*`: 转场与效果基础工具
- `Color/*` 与 `ColorCollection/*`: 颜色类型、LUT、调色辅助
- `Mask/RotoMask`: 转描与遮罩数据模型
- `Tracking/MotionTracker`: 运动跟踪基础能力

### 几何、变换与形状

- `Geometry/*`: 网格导入导出与数学辅助
- `Mesh/*`: 网格数据与 Wavefront 支持
- `Transform/*`: 2D/3D 变换组件
- `Shape/*`: 矢量路径/图层/分组定义

### 脚本与表达式

- `Script/*`: 表达式解析/求值与脚本引擎组件
- 内建函数与环境管理模块

### 资产、IO 与平台

- `Asset/*`: 资产元数据、代理生成与缓存
- `IO/*`: 异步文件/图像导入导出工具
- `File/*` 与 `FileSystem/*`: 文件信息/类型与目录支持
- `Platform/*`: OS/平台辅助、进程与 Shell 工具

### 工具与基础设施

- `Utils/*`: ID、字符串、路径、计时器、资源管理器辅助
- `Container/*`: 多索引容器工具
- `Event/EventBus`: 事件分发基础设施
- `Thread/*`: 线程辅助
- `EnvironmentVariable/*`: 环境变量读取与更新支持

## 仓库内已有专题文档

- `docs/TimelineClock_Architecture.md`
- `docs/PlaybackClock_Usage.md`
- `docs/MFFrameExtractor_Usage.md`
- `docs/FrameRange_Usage.md`

## 说明

- 本文件是功能检索索引，不是完整 API 参考。
- 实际开发请从以上专题文档及对应 `include/*` 模块开始。
