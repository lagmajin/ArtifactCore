module ;
#include <QPaintEngine>
export module Image.CvMatPaintEngine;


namespace ArtifactCore {

 class CvMatPaintEngine:public QPaintEngine {
 private:
 protected:

 public:
  CvMatPaintEngine();
  ~CvMatPaintEngine();

  bool begin(QPaintDevice* pdev) override;
  bool end() override;
  void updateState(const QPaintEngineState& state) override;
  void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
  //Type type() const override;
 };

};