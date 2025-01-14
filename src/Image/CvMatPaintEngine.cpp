#include "../../include/Image/CvMatPaintEngine.hpp"


namespace ArtifactCore {



bool CvMatPaintEngine::begin(QPaintDevice* pdev)
{
	return true;
}

bool CvMatPaintEngine::end()
{
	return true;
}

void CvMatPaintEngine::updateState(const QPaintEngineState& state)
{
}

void CvMatPaintEngine::drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
{
}

CvMatPaintEngine::CvMatPaintEngine() :QPaintEngine(QPaintEngine::AllFeatures)
{

}

CvMatPaintEngine::~CvMatPaintEngine()
{

}
}