module;
#include <QString>
#include <boost/mpl/aux_/na_fwd.hpp>
export module Graphics.GPU.Info;


export namespace ArtifactCore
{
 struct GPUInfo
 {
  QString Vendor;      // "NVIDIA", "AMD", "Intel", "Unknown"
  QString DeviceName;  // 例: "GeForce RTX 4090"
  QString Type;        // "Discrete", "Integrated", "Virtual", "Unknown"


 	
 };


	

};
