#pragma once



#if( LIBRARY_DLL_MODE==1)

#define LIBRARY_DLL_API __declspec(dllexport)
#elif( LIBRARY_DLL_MODE==2)
#define LIBRARY_DLL_API __declspec(dllimport)
#else
#define LIBRARY_DLL_API 
#endif