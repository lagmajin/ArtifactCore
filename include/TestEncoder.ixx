module;
#include <string>
#include <opencv2/core.hpp>
export module TestEncoder;





export namespace ArtifactCore {

 class TestEncoder {
 public:
  TestEncoder(const std::string& outputDir, const std::string& prefix = "frame", const std::string& ext = ".png");

  void save(const cv::Mat& image);
  void reset();

 private:
  std::string m_outputDir;
  std::string m_prefix;
  std::string m_ext;
  int m_frameIndex;
 };



}