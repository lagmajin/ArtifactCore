module;
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "../Define/DllExportMacro.hpp"

export module Channel;

export namespace ArtifactCore {

/**
 * @brief 映像チャンネルの型定義
 * RGBAの基本4チャンネルに加え、深度、法線、速度、IDなどの補助チャンネルをサポート
 */
enum class ChannelType {
    Red,
    Green,
    Blue,
    Alpha,
    Depth,          // Z-Buffer (Depth)
    NormalX,        //法線 X
    NormalY,        //法線 Y
    NormalZ,        //法線 Z
    VelocityX,      //速度 X (Motion Vector)
    VelocityY,      //速度 Y
    ObjectId,       //オブジェクトID (Selection/Masking)
    MaterialId,     //マテリアルID
    Emission,       //発光チャンネル
    Custom          //ユーザー定義チャンネル
};

/**
 * @brief 単一の映像チャンネルデータを保持するクラス
 * Float32 プレシジョンで保持し、ハイダイナミックレンジ(HDR)に対応
 */
class LIBRARY_DLL_API VideoChannel {
public:
    VideoChannel(int width, int height);
    ~VideoChannel();

    void resize(int width, int height);
    void clear(float value = 0.0f);

    float* data() { return data_.data(); }
    const float* data() const { return data_.data(); }
    
    int width() const { return width_; }
    int height() const { return height_; }
    size_t size() const { return data_.size(); }

private:
    int width_, height_;
    std::vector<float> data_;
};

/**
 * @brief マルチチャンネル映像コンテナ
 * 必要なチャンネルを動的に追加・管理可能
 */
class LIBRARY_DLL_API VideoFrame {
public:
    VideoFrame(int width, int height);
    ~VideoFrame();

    void addChannel(ChannelType type, const std::string& name = "");
    void removeChannel(ChannelType type);
    bool hasChannel(ChannelType type) const;
    
    std::shared_ptr<VideoChannel> getChannel(ChannelType type);
    std::shared_ptr<const VideoChannel> getChannel(ChannelType type) const;
    
    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_, height_;
    std::map<ChannelType, std::shared_ptr<VideoChannel>> channels_;
};

} // namespace ArtifactCore
