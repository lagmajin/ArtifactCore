#pragma once



#ifdef LIBRARY_DLL_EXPORT
#define LIBRARY_DLL_API __declspec(dllexport)
#else
#define LIBRARY_DLL_API __declspec(dllimport)
#endif