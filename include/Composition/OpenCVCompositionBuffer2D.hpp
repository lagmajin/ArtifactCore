#pragma once

import std;

#include "AbstractCompositionBuffer.hpp"
#include "../Image/FloatImage.hpp"



namespace ArtifactCore {

class OpenCVCompsitionBuffer2DPrivate;

class OpenCVCompositionBuffer2D {
private:

public:
	OpenCVCompositionBuffer2D(int width,int height);
	~OpenCVCompositionBuffer2D();
	void clear();

};









};