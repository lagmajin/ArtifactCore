#pragma once

#if defined(BUILDING_MYLIB)
#define MYLIB_API __declspec(dllexport)
#elif defined(USING_MYLIB)
#define MYLIB_API __declspec(dllimport)
#else
#define MYLIB_API
#endif