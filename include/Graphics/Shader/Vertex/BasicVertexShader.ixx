module;
#include <QString>
#include <QByteArray>
#include "..\ArtifactWidgets\include\Define\DllExportMacro.hpp"

export module Graphics.Shader.HLSL.Basics.Vertex;


import std;

export namespace ArtifactCore {

 extern LIBRARY_DLL_API const QByteArray lineShaderVSText;

 //extern LIBRARY_DLL_API const QByteArray g_qsBasicSprite2DI;

 extern LIBRARY_DLL_API const QByteArray g_qsBasic2DVS;

 extern LIBRARY_DLL_API const QByteArray drawOutlineRectVSSource;


 extern LIBRARY_DLL_API const QByteArray drawSolidRectVSSource;



};