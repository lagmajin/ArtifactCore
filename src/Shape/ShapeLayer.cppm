module;
class tst_QList;

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QFont>
#include <QStringList>
#include <QTransform>
#include <QRegularExpression>
#include <algorithm>
#include <atomic>
#include <memory>
#include <sstream>
#include <vector>

module Shape.Layer:Impl;

import Shape.Layer;

namespace ArtifactCore {

namespace {

QTransform shapeTransformMatrix(const ShapeTransform& tf)
{
    QTransform matrix;
    matrix.translate(tf.position.x, tf.position.y);
    matrix.translate(tf.anchor.x, tf.anchor.y);
    matrix.rotate(tf.rotation);
    matrix.scale(tf.scale.x, tf.scale.y);
    matrix.translate(-tf.anchor.x, -tf.anchor.y);
    return matrix;
}

QColor toQColor(const FillSettings& fill)
{
    return fill.color;
}

QColor toQColor(const StrokeSettings& stroke)
{
    return stroke.color;
}

QPen makeStrokePen(const StrokeSettings& stroke)
{
    QPen pen(toQColor(stroke), stroke.width);

    switch (stroke.cap) {
    case LineCap::Round:
        pen.setCapStyle(Qt::RoundCap);
        break;
    case LineCap::Square:
        pen.setCapStyle(Qt::SquareCap);
        break;
    case LineCap::Butt:
    default:
        pen.setCapStyle(Qt::FlatCap);
        break;
    }

    switch (stroke.join) {
    case LineJoin::Round:
        pen.setJoinStyle(Qt::RoundJoin);
        break;
    case LineJoin::Bevel:
        pen.setJoinStyle(Qt::BevelJoin);
        break;
    case LineJoin::Miter:
    default:
        pen.setJoinStyle(Qt::MiterJoin);
        break;
    }

    pen.setMiterLimit(stroke.miterLimit);
    if (!stroke.dashPattern.empty()) {
        QVector<qreal> pattern;
        pattern.reserve(static_cast<int>(stroke.dashPattern.size()));
        for (double dash : stroke.dashPattern) {
            pattern.append(static_cast<qreal>(std::max(0.0, dash)));
        }
        pen.setDashPattern(pattern);
        pen.setDashOffset(stroke.dashOffset);
    }
    return pen;
}

QString svgStyleString(const FillSettings& fill, const StrokeSettings& stroke)
{
    QStringList parts;
    parts << QStringLiteral("fill:%1").arg(fill.enabled ? fill.color.name(QColor::HexArgb) : QStringLiteral("none"));
    parts << QStringLiteral("stroke:%1").arg(stroke.enabled ? stroke.color.name(QColor::HexArgb) : QStringLiteral("none"));
    parts << QStringLiteral("stroke-width:%1").arg(stroke.width, 0, 'f', 3);
    switch (stroke.cap) {
    case LineCap::Round:
        parts << QStringLiteral("stroke-linecap:round");
        break;
    case LineCap::Square:
        parts << QStringLiteral("stroke-linecap:square");
        break;
    case LineCap::Butt:
    default:
        parts << QStringLiteral("stroke-linecap:butt");
        break;
    }
    switch (stroke.join) {
    case LineJoin::Round:
        parts << QStringLiteral("stroke-linejoin:round");
        break;
    case LineJoin::Bevel:
        parts << QStringLiteral("stroke-linejoin:bevel");
        break;
    case LineJoin::Miter:
    default:
        parts << QStringLiteral("stroke-linejoin:miter");
        break;
    }
    parts << QStringLiteral("stroke-miterlimit:%1").arg(stroke.miterLimit, 0, 'f', 3);
    if (!stroke.dashPattern.empty()) {
        QStringList dashParts;
        dashParts.reserve(static_cast<int>(stroke.dashPattern.size()));
        for (double dash : stroke.dashPattern) {
            dashParts << QString::number(std::max(0.0, dash), 'f', 3);
        }
        parts << QStringLiteral("stroke-dasharray:%1").arg(dashParts.join(QStringLiteral(",")));
    }
    return parts.join(QStringLiteral(";"));
}

QString shapePathToSvgData(const ShapePath& path)
{
    QStringList tokens;
    for (const auto& cmd : path.commands()) {
        switch (cmd.type) {
        case PathCommandType::MoveTo:
            tokens << QStringLiteral("M %1 %2").arg(cmd.points[0].x(), 0, 'f', 3).arg(cmd.points[0].y(), 0, 'f', 3);
            break;
        case PathCommandType::LineTo:
            tokens << QStringLiteral("L %1 %2").arg(cmd.points[0].x(), 0, 'f', 3).arg(cmd.points[0].y(), 0, 'f', 3);
            break;
        case PathCommandType::CubicTo:
            tokens << QStringLiteral("C %1 %2 %3 %4 %5 %6")
                           .arg(cmd.points[0].x(), 0, 'f', 3)
                           .arg(cmd.points[0].y(), 0, 'f', 3)
                           .arg(cmd.points[1].x(), 0, 'f', 3)
                           .arg(cmd.points[1].y(), 0, 'f', 3)
                           .arg(cmd.points[2].x(), 0, 'f', 3)
                           .arg(cmd.points[2].y(), 0, 'f', 3);
            break;
        case PathCommandType::QuadTo:
            tokens << QStringLiteral("Q %1 %2 %3 %4")
                           .arg(cmd.points[0].x(), 0, 'f', 3)
                           .arg(cmd.points[0].y(), 0, 'f', 3)
                           .arg(cmd.points[1].x(), 0, 'f', 3)
                           .arg(cmd.points[1].y(), 0, 'f', 3);
            break;
        case PathCommandType::Close:
            tokens << QStringLiteral("Z");
            break;
        }
    }
    return tokens.join(QStringLiteral(" "));
}

QTransform parseSvgTransform(const QString& value)
{
    QTransform transform;
    const auto applyNumbers = [](const QString& raw) {
        QString normalized = raw;
        normalized.replace(',', ' ');
        return normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    };

    static const QRegularExpression transformRegex(
        QStringLiteral(R"((matrix|translate|scale|rotate)\s*\(([^)]*)\))"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = transformRegex.globalMatch(value);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString kind = match.captured(1).toLower();
        const QStringList args = applyNumbers(match.captured(2));

        if (kind == QStringLiteral("matrix") && args.size() >= 6) {
            bool ok[6] = {false, false, false, false, false, false};
            const double a = args[0].toDouble(&ok[0]);
            const double b = args[1].toDouble(&ok[1]);
            const double c = args[2].toDouble(&ok[2]);
            const double d = args[3].toDouble(&ok[3]);
            const double e = args[4].toDouble(&ok[4]);
            const double f = args[5].toDouble(&ok[5]);
            if (ok[0] && ok[1] && ok[2] && ok[3] && ok[4] && ok[5]) {
                transform *= QTransform(a, b, c, d, e, f);
            }
        } else if (kind == QStringLiteral("translate") && !args.isEmpty()) {
            bool okX = false;
            bool okY = true;
            const double tx = args[0].toDouble(&okX);
            const double ty = args.size() > 1 ? args[1].toDouble(&okY) : 0.0;
            if (okX && okY) {
                transform.translate(tx, ty);
            }
        } else if (kind == QStringLiteral("scale") && !args.isEmpty()) {
            bool okX = false;
            bool okY = true;
            const double sx = args[0].toDouble(&okX);
            const double sy = args.size() > 1 ? args[1].toDouble(&okY) : sx;
            if (okX && okY) {
                transform.scale(sx, sy);
            }
        } else if (kind == QStringLiteral("rotate") && !args.isEmpty()) {
            bool okA = false;
            const double angle = args[0].toDouble(&okA);
            if (okA) {
                if (args.size() >= 3) {
                    bool okCx = false;
                    bool okCy = false;
                    const double cx = args[1].toDouble(&okCx);
                    const double cy = args[2].toDouble(&okCy);
                    if (okCx && okCy) {
                        transform.translate(cx, cy);
                        transform.rotate(angle);
                        transform.translate(-cx, -cy);
                    }
                } else {
                    transform.rotate(angle);
                }
            }
        }
    }

    return transform;
}

bool readSvgNumber(const QString& src, int& index, double& out)
{
    while (index < src.size() && (src[index].isSpace() || src[index] == ',')) {
        ++index;
    }
    if (index >= src.size()) {
        return false;
    }
    int start = index;
    bool seenDigit = false;
    while (index < src.size()) {
        const QChar c = src[index];
        if (c.isDigit() || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
            seenDigit = true;
            ++index;
            continue;
        }
        break;
    }
    if (!seenDigit) {
        return false;
    }
    bool ok = false;
    out = src.mid(start, index - start).toDouble(&ok);
    return ok;
}

QVector<QPointF> parsePointsAttribute(const QString& value)
{
    QVector<QPointF> points;
    QString normalized = value;
    normalized.replace(',', ' ');
    const QStringList parts = normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        bool okX = false;
        bool okY = false;
        const double x = parts[i].toDouble(&okX);
        const double y = parts[i + 1].toDouble(&okY);
        if (okX && okY) {
            points.push_back(QPointF(x, y));
        }
    }
    return points;
}

ShapePath parseSvgPathData(const QString& data)
{
    ShapePath path;
    int index = 0;
    QChar currentCommand;
    while (index < data.size()) {
        while (index < data.size() && data[index].isSpace()) {
            ++index;
        }
        if (index >= data.size()) {
            break;
        }
        const QChar c = data[index];
        if (c.isLetter()) {
            currentCommand = c.toUpper();
            ++index;
            if (currentCommand == QChar('Z')) {
                path.close();
            }
            continue;
        }

        if (currentCommand == QChar('M')) {
            double x = 0.0;
            double y = 0.0;
            if (readSvgNumber(data, index, x) && readSvgNumber(data, index, y)) {
                path.moveTo(x, y);
                currentCommand = QChar('L');
            } else {
                break;
            }
        } else if (currentCommand == QChar('L')) {
            double x = 0.0;
            double y = 0.0;
            if (readSvgNumber(data, index, x) && readSvgNumber(data, index, y)) {
                path.lineTo(x, y);
            } else {
                break;
            }
        } else if (currentCommand == QChar('Q')) {
            double cx = 0.0;
            double cy = 0.0;
            double ex = 0.0;
            double ey = 0.0;
            if (readSvgNumber(data, index, cx) && readSvgNumber(data, index, cy) &&
                readSvgNumber(data, index, ex) && readSvgNumber(data, index, ey)) {
                path.quadTo(cx, cy, ex, ey);
            } else {
                break;
            }
        } else if (currentCommand == QChar('C')) {
            double c1x = 0.0;
            double c1y = 0.0;
            double c2x = 0.0;
            double c2y = 0.0;
            double ex = 0.0;
            double ey = 0.0;
            if (readSvgNumber(data, index, c1x) && readSvgNumber(data, index, c1y) &&
                readSvgNumber(data, index, c2x) && readSvgNumber(data, index, c2y) &&
                readSvgNumber(data, index, ex) && readSvgNumber(data, index, ey)) {
                path.cubicTo(c1x, c1y, c2x, c2y, ex, ey);
            } else {
                break;
            }
        } else {
            ++index;
        }
    }
    return path;
}

void renderElement(QPainter& painter, const ShapeElement& element, const QTransform& parentMatrix)
{
    const QTransform localMatrix = parentMatrix * shapeTransformMatrix(element.transform());
    if (!element.isVisible()) {
        return;
    }

    if (const auto* pathShape = dynamic_cast<const PathShape*>(&element)) {
        QPainterPath path = pathShape->toPainterPath();
        path = localMatrix.map(path);

        const auto& fill = pathShape->fill();
        if (fill.enabled) {
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(toQColor(fill));
            painter.drawPath(path);
            painter.restore();
        }

        const auto& stroke = pathShape->stroke();
        if (stroke.enabled && stroke.width > 0.0) {
            QPen pen = makeStrokePen(stroke);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
    } else if (const auto* group = dynamic_cast<const ShapeGroup*>(&element)) {
        for (const ShapeElement* child : group->children()) {
            if (child) {
                renderElement(painter, *child, localMatrix);
            }
        }
    }
}

QString elementToSvg(const ShapeElement& element, const QTransform& parentMatrix)
{
    const QTransform localMatrix = parentMatrix * shapeTransformMatrix(element.transform());
    if (!element.isVisible()) {
        return QString();
    }

    QString output;
    if (const auto* pathShape = dynamic_cast<const PathShape*>(&element)) {
        QPainterPath path = pathShape->toPainterPath();
        path = localMatrix.map(path);
        ShapePath exportPath = ShapePath::fromPainterPath(path);
        output += QStringLiteral("<path d=\"%1\" style=\"%2\" />\n")
                      .arg(shapePathToSvgData(exportPath),
                           svgStyleString(pathShape->fill(), pathShape->stroke()));
    } else if (const auto* group = dynamic_cast<const ShapeGroup*>(&element)) {
        output += QStringLiteral("<g>\n");
        for (const ShapeElement* child : group->children()) {
            if (child) {
                output += elementToSvg(*child, localMatrix);
            }
        }
        output += QStringLiteral("</g>\n");
    }
    return output;
}

} // namespace

class ShapeLayer::Impl {
public:
    QString name_;
    int layerId_ = 0;
    bool visible_ = true;
    bool locked_ = false;
    bool solo_ = false;
    BlendMode blendMode_ = BlendMode::Normal;
    std::unique_ptr<ShapeGroup> root_;
    ShapeTransform transform_;

    Impl()
        : layerId_(nextId())
        , root_(std::make_unique<ShapeGroup>()) {}

    Impl(const Impl& other)
        : name_(other.name_)
        , layerId_(nextId())
        , visible_(other.visible_)
        , locked_(other.locked_)
        , solo_(other.solo_)
        , blendMode_(other.blendMode_)
        , root_(std::make_unique<ShapeGroup>())
        , transform_(other.transform_) {
        if (other.root_) {
            for (const ShapeElement* child : other.root_->children()) {
                if (child) {
                    root_->addChild(child->clone());
                }
            }
        }
    }

    Impl& operator=(const Impl& other) {
        if (this == &other) {
            return *this;
        }
        name_ = other.name_;
        visible_ = other.visible_;
        locked_ = other.locked_;
        solo_ = other.solo_;
        blendMode_ = other.blendMode_;
        transform_ = other.transform_;
        root_ = std::make_unique<ShapeGroup>();
        for (const ShapeElement* child : other.root_->children()) {
            if (child) {
                root_->addChild(child->clone());
            }
        }
        return *this;
    }

    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;

    static int nextId() {
        static std::atomic<int> value{1};
        return value.fetch_add(1, std::memory_order_relaxed);
    }
};

ShapeLayer::ShapeLayer() : impl_(new Impl()) {}
ShapeLayer::~ShapeLayer() { delete impl_; }

ShapeLayer::ShapeLayer(const ShapeLayer& other)
    : impl_(other.impl_ ? new Impl(*other.impl_) : new Impl()) {}

ShapeLayer& ShapeLayer::operator=(const ShapeLayer& other)
{
    if (this != &other) {
        if (!impl_) {
            impl_ = new Impl(*other.impl_);
            return *this;
        }
        *impl_ = *other.impl_;
    }
    return *this;
}

ShapeLayer::ShapeLayer(ShapeLayer&& other) noexcept : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

ShapeLayer& ShapeLayer::operator=(ShapeLayer&& other) noexcept
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

QString ShapeLayer::name() const { return impl_->name_; }
void ShapeLayer::setName(const QString& name) { impl_->name_ = name; }
int ShapeLayer::layerId() const { return impl_->layerId_; }
bool ShapeLayer::isVisible() const { return impl_->visible_; }
void ShapeLayer::setVisible(bool visible) { impl_->visible_ = visible; }
bool ShapeLayer::isLocked() const { return impl_->locked_; }
void ShapeLayer::setLocked(bool locked) { impl_->locked_ = locked; }
bool ShapeLayer::isSolo() const { return impl_->solo_; }
void ShapeLayer::setSolo(bool solo) { impl_->solo_ = solo; }

ShapeGroup* ShapeLayer::content() { return impl_->root_.get(); }
const ShapeGroup* ShapeLayer::content() const { return impl_->root_.get(); }

void ShapeLayer::addShape(std::unique_ptr<ShapeElement> shape)
{
    if (shape && impl_->root_) {
        impl_->root_->addChild(std::move(shape));
    }
}

void ShapeLayer::clearContent()
{
    if (impl_->root_) {
        impl_->root_->clearChildren();
    }
}

int ShapeLayer::shapeCount() const
{
    return impl_->root_ ? impl_->root_->childCount() : 0;
}

ShapeTransform& ShapeLayer::transform() { return impl_->transform_; }
const ShapeTransform& ShapeLayer::transform() const { return impl_->transform_; }

Point2DValue ShapeLayer::anchorPoint() const { return impl_->transform_.anchor; }
void ShapeLayer::setAnchorPoint(const Point2DValue& anchor) { impl_->transform_.anchor = anchor; }
Point2DValue ShapeLayer::position() const { return impl_->transform_.position; }
void ShapeLayer::setPosition(const Point2DValue& position) { impl_->transform_.position = position; }
Point2DValue ShapeLayer::scale() const { return impl_->transform_.scale; }
void ShapeLayer::setScale(const Point2DValue& scale) { impl_->transform_.scale = scale; }
double ShapeLayer::rotation() const { return impl_->transform_.rotation; }
void ShapeLayer::setRotation(double angle) { impl_->transform_.rotation = angle; }
double ShapeLayer::opacity() const { return impl_->transform_.opacity; }
void ShapeLayer::setOpacity(double opacity) { impl_->transform_.opacity = std::clamp(opacity, 0.0, 1.0); }

QRectF ShapeLayer::boundingRect() const
{
    if (!impl_->root_) {
        return {};
    }
    QRectF bounds = impl_->root_->boundingRect();
    if (bounds.isNull()) {
        return bounds;
    }
    return shapeTransformMatrix(impl_->transform_).mapRect(bounds);
}

bool ShapeLayer::contains(const QPointF& point) const
{
    return toPainterPath().contains(point);
}

QImage ShapeLayer::render(const QSize& size, double time) const
{
    Q_UNUSED(time);
    if (size.isEmpty() || !impl_->root_) {
        return {};
    }

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(impl_->transform_.opacity);

    for (const ShapeElement* child : impl_->root_->children()) {
        if (child) {
            renderElement(painter, *child, shapeTransformMatrix(impl_->transform_));
        }
    }

    painter.end();
    return image;
}

QPainterPath ShapeLayer::toPainterPath() const
{
    QPainterPath combined;
    if (!impl_->root_) {
        return combined;
    }

    const QTransform matrix = shapeTransformMatrix(impl_->transform_);
    for (const ShapeElement* child : impl_->root_->children()) {
        if (const auto* pathShape = dynamic_cast<const PathShape*>(child)) {
            QPainterPath path = pathShape->toPainterPath();
            combined.addPath(matrix.map(path));
        } else if (const auto* group = dynamic_cast<const ShapeGroup*>(child)) {
            for (const ShapePath& path : group->processedPaths()) {
                combined.addPath(matrix.map(path.toPainterPath()));
            }
        }
    }
    return combined;
}

ShapeLayer::BlendMode ShapeLayer::blendMode() const { return impl_->blendMode_; }
void ShapeLayer::setBlendMode(BlendMode mode) { impl_->blendMode_ = mode; }

ShapeLayer ShapeLayer::clone() const
{
    ShapeLayer copy;
    if (impl_) {
        *copy.impl_ = *impl_;
    }
    copy.impl_->layerId_ = Impl::nextId();
    return copy;
}

QString ShapeLayer::toSvg() const
{
    QString svg;
    svg += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n");
    if (impl_->root_) {
        for (const ShapeElement* child : impl_->root_->children()) {
            if (child) {
                svg += elementToSvg(*child, shapeTransformMatrix(impl_->transform_));
            }
        }
    }
    svg += QStringLiteral("</svg>\n");
    return svg;
}

ShapeLayer ShapeLayer::fromSvg(const QString& svgContent)
{
    ShapeLayer layer;
    auto readStyle = [](const auto& attrs, FillSettings& fill, StrokeSettings& stroke) {
        const QColor fillColor(attrs.value(QStringLiteral("fill")).toString());
        const QColor strokeColor(attrs.value(QStringLiteral("stroke")).toString());
        bool strokeWidthOk = false;
        const double strokeWidth = attrs.value(QStringLiteral("stroke-width")).toString().toDouble(&strokeWidthOk);

        fill = FillSettings(fillColor.isValid() ? fillColor : QColor(Qt::white));
        stroke = StrokeSettings(strokeColor.isValid() ? strokeColor : QColor(Qt::black),
                                strokeWidthOk ? strokeWidth : 1.0);
        fill.enabled = attrs.value(QStringLiteral("fill")).toString() != QStringLiteral("none");
        stroke.enabled = attrs.value(QStringLiteral("stroke")).toString() != QStringLiteral("none");

        const QString cap = attrs.value(QStringLiteral("stroke-linecap")).toString().toLower();
        if (cap == QStringLiteral("round")) {
            stroke.cap = LineCap::Round;
        } else if (cap == QStringLiteral("square")) {
            stroke.cap = LineCap::Square;
        } else {
            stroke.cap = LineCap::Butt;
        }

        const QString join = attrs.value(QStringLiteral("stroke-linejoin")).toString().toLower();
        if (join == QStringLiteral("round")) {
            stroke.join = LineJoin::Round;
        } else if (join == QStringLiteral("bevel")) {
            stroke.join = LineJoin::Bevel;
        } else {
            stroke.join = LineJoin::Miter;
        }

        bool miterOk = false;
        const double miter = attrs.value(QStringLiteral("stroke-miterlimit")).toString().toDouble(&miterOk);
        if (miterOk) {
            stroke.miterLimit = miter;
        }

        const QString dashArray = attrs.value(QStringLiteral("stroke-dasharray")).toString().trimmed();
        if (!dashArray.isEmpty() && dashArray != QStringLiteral("none")) {
            stroke.dashPattern.clear();
            const QStringList items = dashArray.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
            for (const QString& item : items) {
                bool ok = false;
                const double dash = item.toDouble(&ok);
                if (ok && dash > 0.0) {
                    stroke.dashPattern.push_back(dash);
                }
            }
        }

        bool dashOffsetOk = false;
        const double dashOffset = attrs.value(QStringLiteral("stroke-dashoffset")).toString().toDouble(&dashOffsetOk);
        if (dashOffsetOk) {
            stroke.dashOffset = dashOffset;
        }
    };

    QXmlStreamReader reader(svgContent);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }

        const auto attrs = reader.attributes();
        if (reader.name() == QLatin1String("path")) {
            const QString d = attrs.value(QStringLiteral("d")).toString();
            ShapePath path = parseSvgPathData(d);
            const QTransform elementTransform = parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
            if (!elementTransform.isIdentity()) {
                path = ShapePath::fromPainterPath(elementTransform.map(path.toPainterPath()));
            }
            auto element = std::make_unique<PathShape>(path);
            FillSettings fill;
            StrokeSettings stroke;
            readStyle(attrs, fill, stroke);
            element->setFill(fill);
            element->setStroke(stroke);
            layer.addShape(std::move(element));
        } else if (reader.name() == QLatin1String("rect")) {
            const double x = attrs.value(QStringLiteral("x")).toDouble();
            const double y = attrs.value(QStringLiteral("y")).toDouble();
            const double w = attrs.value(QStringLiteral("width")).toDouble();
            const double h = attrs.value(QStringLiteral("height")).toDouble();
            ShapePath path;
            path.setRectangle(QRectF(x, y, w, h));
            const QTransform elementTransform = parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
            if (!elementTransform.isIdentity()) {
                path = ShapePath::fromPainterPath(elementTransform.map(path.toPainterPath()));
            }
            auto element = std::make_unique<PathShape>(path);
            FillSettings fill;
            StrokeSettings stroke;
            readStyle(attrs, fill, stroke);
            element->setFill(fill);
            element->setStroke(stroke);
            layer.addShape(std::move(element));
        } else if (reader.name() == QLatin1String("ellipse")) {
            const double cx = attrs.value(QStringLiteral("cx")).toDouble();
            const double cy = attrs.value(QStringLiteral("cy")).toDouble();
            const double rx = attrs.value(QStringLiteral("rx")).toDouble();
            const double ry = attrs.value(QStringLiteral("ry")).toDouble();
            ShapePath path;
            path.setEllipse(cx, cy, rx, ry);
            const QTransform elementTransform = parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
            if (!elementTransform.isIdentity()) {
                path = ShapePath::fromPainterPath(elementTransform.map(path.toPainterPath()));
            }
            auto element = std::make_unique<PathShape>(path);
            FillSettings fill;
            StrokeSettings stroke;
            readStyle(attrs, fill, stroke);
            element->setFill(fill);
            element->setStroke(stroke);
            layer.addShape(std::move(element));
        } else if (reader.name() == QLatin1String("circle")) {
            const double cx = attrs.value(QStringLiteral("cx")).toDouble();
            const double cy = attrs.value(QStringLiteral("cy")).toDouble();
            const double r = attrs.value(QStringLiteral("r")).toDouble();
            ShapePath path;
            path.setEllipse(cx - r, cy - r, r, r);
            const QTransform elementTransform = parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
            if (!elementTransform.isIdentity()) {
                path = ShapePath::fromPainterPath(elementTransform.map(path.toPainterPath()));
            }
            auto element = std::make_unique<PathShape>(path);
            FillSettings fill;
            StrokeSettings stroke;
            readStyle(attrs, fill, stroke);
            element->setFill(fill);
            element->setStroke(stroke);
            layer.addShape(std::move(element));
        } else if (reader.name() == QLatin1String("polygon") || reader.name() == QLatin1String("polyline")) {
            const QVector<QPointF> points = parsePointsAttribute(attrs.value(QStringLiteral("points")).toString());
            ShapePath path;
            path.setPolygon(std::vector<QPointF>(points.begin(), points.end()), reader.name() == QLatin1String("polygon"));
            const QTransform elementTransform = parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
            if (!elementTransform.isIdentity()) {
                path = ShapePath::fromPainterPath(elementTransform.map(path.toPainterPath()));
            }
            auto element = std::make_unique<PathShape>(path);
            FillSettings fill;
            StrokeSettings stroke;
            readStyle(attrs, fill, stroke);
            element->setFill(fill);
            element->setStroke(stroke);
            layer.addShape(std::move(element));
        }
    }

    return layer;
}

ShapeLayer ShapeLayer::createEmpty()
{
    return ShapeLayer();
}

ShapeLayer ShapeLayer::createRectangle(const QRectF& rect, const FillSettings& fill, const StrokeSettings& stroke)
{
    ShapeLayer layer;
    ShapePath path;
    path.setRectangle(rect);
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayer::createSquare(const QPointF& topLeft, double size, const FillSettings& fill, const StrokeSettings& stroke)
{
    return createRectangle(QRectF(topLeft, QSizeF(size, size)), fill, stroke);
}

ShapeLayer ShapeLayer::createTriangle(const QRectF& bounds, bool pointUp, const FillSettings& fill, const StrokeSettings& stroke)
{
    ShapeLayer layer;
    ShapePath path;
    const QPointF top(bounds.center().x(), pointUp ? bounds.top() : bounds.bottom());
    const QPointF left(bounds.left(), pointUp ? bounds.bottom() : bounds.top());
    const QPointF right(bounds.right(), pointUp ? bounds.bottom() : bounds.top());
    std::vector<QPointF> points;
    points.reserve(3);
    points.push_back(top);
    points.push_back(right);
    points.push_back(left);
    path.setPolygon(points, true);
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayer::createEllipse(const QRectF& rect, const FillSettings& fill, const StrokeSettings& stroke)
{
    ShapeLayer layer;
    ShapePath path;
    path.setEllipse(rect);
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayer::createStar(const QPointF& center, int points, double outerRadius, double innerRadius,
                                  const FillSettings& fill, const StrokeSettings& stroke)
{
    ShapeLayer layer;
    ShapePath path;
    path.setStar(center, points, outerRadius, innerRadius);
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayer::createPolygon(const std::vector<QPointF>& points,
                                     const FillSettings& fill, const StrokeSettings& stroke)
{
    ShapeLayer layer;
    ShapePath path;
    path.setPolygon(points, true);
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayerFactory::fromPath(const ShapePath& path,
                                       const FillSettings& fill,
                                       const StrokeSettings& stroke)
{
    ShapeLayer layer;
    auto shape = std::make_unique<PathShape>(path);
    shape->setFill(fill);
    shape->setStroke(stroke);
    layer.addShape(std::move(shape));
    return layer;
}

ShapeLayer ShapeLayerFactory::fromPainterPath(const QPainterPath& path,
                                              const FillSettings& fill,
                                              const StrokeSettings& stroke)
{
    return fromPath(ShapePath::fromPainterPath(path), fill, stroke);
}

ShapeLayer ShapeLayerFactory::fromSquare(const QPointF& topLeft, double size,
                                         const FillSettings& fill,
                                         const StrokeSettings& stroke)
{
    return ShapeLayer::createSquare(topLeft, size, fill, stroke);
}

ShapeLayer ShapeLayerFactory::fromTriangle(const QRectF& bounds, bool pointUp,
                                           const FillSettings& fill,
                                           const StrokeSettings& stroke)
{
    return ShapeLayer::createTriangle(bounds, pointUp, fill, stroke);
}

ShapeLayer ShapeLayerFactory::fromText(const QString& text,
                                      const QString& fontFamily,
                                      int fontSize,
                                      const QPointF& position,
                                      const FillSettings& fill)
{
    QFont font(fontFamily, fontSize);
    QPainterPath path;
    path.addText(position, font, text);
    StrokeSettings stroke;
    stroke.enabled = false;
    return fromPath(ShapePath::fromPainterPath(path), fill, stroke);
}

} // namespace ArtifactCore
