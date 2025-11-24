module ;
#include <QString>
#include <QByteArray>

export module Graphics.Shader.HLSL.Basics.Pixel;
#include "../../../Define/DllExportMacro.hpp"
import std;

export namespace ArtifactCore {

 extern LIBRARY_DLL_API const QByteArray g_qsBasicSprite2DImagePS;
	
 extern LIBRARY_DLL_API const QByteArray g_qsSolidColorPS2;

 extern LIBRARY_DLL_API const QByteArray g_SolidColorPS;

 extern LIBRARY_DLL_API const QByteArray drawOutlineRectPSSource;


 extern LIBRARY_DLL_API const QByteArray g_qsSolidColorPSSource;

};