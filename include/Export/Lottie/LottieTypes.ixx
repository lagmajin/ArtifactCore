module;
#include <any>
#include <string>
#include <vector>

export module Export.Lottie.Types;

export namespace ArtifactCore::Export::Lottie {

struct LottieKeyframe {
    double t = 0.0;
    std::vector<double> s;
    std::vector<double> e;
    int hold = 0;
    double bezierX1 = 0.0;
    double bezierY1 = 0.0;
    double bezierX2 = 1.0;
    double bezierY2 = 1.0;
};

struct LottiePoint {
    std::vector<double> k;
    std::vector<LottieKeyframe> keyframes;
};

struct LottieColor {
    std::vector<double> k;
    std::vector<LottieKeyframe> keyframes;
};

struct LottieShapeRect {
    std::string ty = "rc";
    LottiePoint p;
    LottiePoint s;
    LottiePoint r;
    int direction = 1;
};

struct LottieShapeEllipse {
    std::string ty = "el";
    LottiePoint p;
    LottiePoint s;
    int direction = 1;
};

struct LottieShapeStar {
    std::string ty = "sr";
    LottiePoint position;
    LottiePoint points;
    LottiePoint rotation;
    LottiePoint outerRadius;
    LottiePoint innerRadius;
    int starType = 1;
    int direction = 1;
};

struct LottieShapeTransform {
    std::string ty = "tr";
    LottiePoint anchor;
    LottiePoint position;
    LottiePoint scale;
    LottiePoint rotation;
    LottiePoint opacity;
};

struct LottieShapeTrim {
    std::string ty = "tm";
    LottiePoint start;
    LottiePoint end;
    LottiePoint offset;
    int mode = 1;
};

struct LottieShapeRepeater {
    std::string ty = "rp";
    LottiePoint copies;
    LottiePoint offset;
    LottieShapeTransform transform;
    int startOpacity = 100;
    int endOpacity = 100;
};

struct LottieShapeMergePaths {
    std::string ty = "mm";
    int mode = 1;
    int direction = 1;
};

struct LottieShapeFill {
    std::string ty = "fl";
    LottieColor c;
    LottiePoint o;
    int blendMode = 0;
    int enabled = 1;
};

struct LottieShapeGradient {
    std::string ty = "gf";
    LottieColor colors;
    LottiePoint opacity;
    LottiePoint startPoint;
    LottiePoint endPoint;
    int gradientType = 1;
    int blendMode = 0;
    int enabled = 1;
};

struct LottieShapeGradientStroke {
    std::string ty = "gs";
    LottieColor colors;
    LottiePoint opacity;
    LottiePoint width;
    LottiePoint startPoint;
    LottiePoint endPoint;
    int gradientType = 1;
    int blendMode = 0;
    int enabled = 1;
};

struct LottieShapeStroke {
    std::string ty = "st";
    LottieColor c;
    LottiePoint o;
    LottiePoint w;
    int blendMode = 0;
    int enabled = 1;
    int lineCap = 1;
    int lineJoin = 1;
    double miterLimit = 4.0;
    std::vector<double> dashPattern;
};

struct LottieShapePath {
    std::string ty = "sh";
    std::vector<double> vertices;
    std::vector<double> inTangents;
    std::vector<double> outTangents;
    bool closed = false;
    int direction = 1;
};

struct LottieShapeGroup {
    std::string ty = "gr";
    std::vector<std::any> items;
};

struct LottieImageAsset {
    std::string id;
    std::string fileName;
    std::string directory;
    std::string embeddedData;
    int width = 0;
    int height = 0;
    bool embedded = false;
};

struct LottieLayer {
    int ddd = 0;
    int ind = 0;
    int ty = 4;
    std::string nm;
    std::string refId; // image/precomp asset reference (Lottie "refId")
    LottiePoint position;
    LottiePoint anchor;
    LottiePoint scale;
    LottiePoint rotation;
    LottiePoint opacity;
    std::vector<std::any> shapes;
    int ip = 0;
    int op = 0;
    int parent = 0;
    int hidden = 0;
    bool autoOrient = false;
    double stretch = 1.0;
    double startTime = 0.0;
    int matteType = 0;
    int matteTarget = 0;
    int blendMode = 0;
    // Solid-layer payload (ty == 1). Lottie expects dimensions in pixels
    // and a normalized RGBA color array in the `sc` field.
    int solidWidth = 0;
    int solidHeight = 0;
    std::vector<double> solidColor;
    // Text-layer payload (ty == 5). This is intentionally a single static
    // document; animated text can still be represented by a future schema
    // extension without changing ordinary layer fields.
    std::string text;
    std::string textFont = "Arial";
    double textFontSize = 24.0;
    std::vector<double> textColor{1.0, 1.0, 1.0};
    int textAlignment = 0;
};

struct LottiePrecompAsset {
    std::string id;
    int width = 0;
    int height = 0;
    std::vector<LottieLayer> layers;
};

struct LottieDocument {
    std::string v = "5.12.0";
    double fr = 30.0;
    int ip = 0;
    int op = 0;
    int w = 1920;
    int h = 1080;
    std::string nm;
    std::vector<std::any> assets;
    std::vector<LottieLayer> layers;
};

} // namespace ArtifactCore::Export::Lottie
