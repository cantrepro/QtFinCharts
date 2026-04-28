#include "FinancialChartItem.h"

#include "AreaSeries.h"
#include "BarSeries.h"
#include "BaselineSeries.h"
#include "CandlestickSeries.h"
#include "ChartSeries.h"
#include "ColorParsing.h"
#include "HistogramSeries.h"
#include "LineSeries.h"

#include <QSGClipNode>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QSGVertexColorMaterial>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTimeZone>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QCursor>
#include <QtGui/QFontMetrics>
#include <QtGui/QHoverEvent>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QNativeGestureEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QTouchEvent>
#include <QtGui/QWheelEvent>
#include <QtQml/QJSValue>
#include <QtQuick/QQuickItemGrabResult>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>

namespace QtFinCharts {

namespace {

    using Vertices = QVector<QPointF>;

    struct StrokeRun {
        QColor color;
        Vertices points;
    };

    qreal devicePixelRatioFor(const QQuickWindow* window)
    {
        return window ? std::max<qreal>(1.0, window->effectiveDevicePixelRatio()) : 1.0;
    }

    qreal alignToDevice(qreal value, qreal dpr)
    {
        return std::round(value * dpr) / dpr;
    }

    qreal oneDevicePixel(qreal dpr)
    {
        return 1.0 / std::max<qreal>(1.0, dpr);
    }

    bool isFinitePoint(const QPointF& point)
    {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    }

    bool isFiniteRect(const QRectF& rect)
    {
        return std::isfinite(rect.x())
            && std::isfinite(rect.y())
            && std::isfinite(rect.width())
            && std::isfinite(rect.height());
    }

    QSGGeometryNode* solidTriangles(const Vertices& vertices, const QColor& color)
    {
        if (vertices.isEmpty() || !color.isValid() || color.alpha() == 0) {
            return nullptr;
        }

        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(vertices.size()));
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        auto* points = geometry->vertexDataAsPoint2D();
        for (qsizetype i = 0; i < vertices.size(); ++i) {
            points[i].set(static_cast<float>(vertices.at(i).x()), static_cast<float>(vertices.at(i).y()));
        }

        auto* material = new QSGFlatColorMaterial;
        material->setColor(color);

        auto* node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        return node;
    }

    QSGGeometryNode* gradientTriangles(const Vertices& vertices, const std::function<QColor(qreal)>& colorForY)
    {
        if (vertices.isEmpty()) {
            return nullptr;
        }

        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), static_cast<int>(vertices.size()));
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        auto* points = geometry->vertexDataAsColoredPoint2D();
        for (qsizetype i = 0; i < vertices.size(); ++i) {
            const QColor color = colorForY(vertices.at(i).y());
            if (!color.isValid() || color.alpha() == 0) {
                points[i].set(static_cast<float>(vertices.at(i).x()),
                    static_cast<float>(vertices.at(i).y()),
                    0,
                    0,
                    0,
                    0);
            } else {
                points[i].set(static_cast<float>(vertices.at(i).x()),
                    static_cast<float>(vertices.at(i).y()),
                    color.red(),
                    color.green(),
                    color.blue(),
                    color.alpha());
            }
        }

        auto* material = new QSGVertexColorMaterial;

        auto* node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        return node;
    }

    void appendRect(Vertices& vertices, const QRectF& rect)
    {
        if (!isFiniteRect(rect) || rect.width() <= 0.0 || rect.height() <= 0.0) {
            return;
        }

        const QPointF topLeft = rect.topLeft();
        const QPointF topRight = rect.topRight();
        const QPointF bottomLeft = rect.bottomLeft();
        const QPointF bottomRight = rect.bottomRight();
        vertices << topLeft << bottomLeft << topRight;
        vertices << topRight << bottomLeft << bottomRight;
    }

    void appendVerticalLine(Vertices& vertices, qreal x, qreal top, qreal bottom, qreal width, qreal dpr)
    {
        const qreal alignedX = alignToDevice(x, dpr);
        const qreal half = width * 0.5;
        appendRect(vertices, QRectF(alignedX - half, top, width, bottom - top));
    }

    void appendHorizontalLine(Vertices& vertices, qreal left, qreal right, qreal y, qreal width, qreal dpr)
    {
        const qreal alignedY = alignToDevice(y, dpr);
        const qreal half = width * 0.5;
        appendRect(vertices, QRectF(left, alignedY - half, right - left, width));
    }

    void appendLineSegment(Vertices& vertices, QPointF a, QPointF b, qreal width)
    {
        if (!isFinitePoint(a) || !isFinitePoint(b) || !std::isfinite(width)) {
            return;
        }

        const qreal dx = b.x() - a.x();
        const qreal dy = b.y() - a.y();
        const qreal length = std::hypot(dx, dy);
        if (length <= 0.0001) {
            return;
        }

        const QPointF normal(-dy / length * width * 0.5, dx / length * width * 0.5);
        const QPointF a1 = a + normal;
        const QPointF a2 = a - normal;
        const QPointF b1 = b + normal;
        const QPointF b2 = b - normal;
        vertices << a1 << a2 << b1;
        vertices << b1 << a2 << b2;
    }

    LineStyle lineStyleFromInt(int style)
    {
        return static_cast<LineStyle>(std::clamp(style,
            static_cast<int>(LineStyle::Solid),
            static_cast<int>(LineStyle::SparseDotted)));
    }

    LineType lineTypeFromInt(int type)
    {
        return static_cast<LineType>(std::clamp(type,
            static_cast<int>(LineType::Simple),
            static_cast<int>(LineType::Curved)));
    }

    PriceFormat priceFormatFromString(const QString& format)
    {
        if (format.compare(QStringLiteral("volume"), Qt::CaseInsensitive) == 0) {
            return PriceFormat::Volume;
        }
        if (format.compare(QStringLiteral("percent"), Qt::CaseInsensitive) == 0) {
            return PriceFormat::Percent;
        }
        return PriceFormat::Price;
    }

    QString priceFormatName(PriceFormat format)
    {
        switch (format) {
        case PriceFormat::Price:
            return QStringLiteral("price");
        case PriceFormat::Volume:
            return QStringLiteral("volume");
        case PriceFormat::Percent:
            return QStringLiteral("percent");
        }
        return QStringLiteral("price");
    }

    PriceScaleMode priceScaleModeFromString(const QString& mode)
    {
        if (mode.compare(QStringLiteral("logarithmic"), Qt::CaseInsensitive) == 0
            || mode.compare(QStringLiteral("log"), Qt::CaseInsensitive) == 0) {
            return PriceScaleMode::Logarithmic;
        }
        if (mode.compare(QStringLiteral("percentage"), Qt::CaseInsensitive) == 0
            || mode.compare(QStringLiteral("percent"), Qt::CaseInsensitive) == 0) {
            return PriceScaleMode::Percentage;
        }
        if (mode.compare(QStringLiteral("indexedTo100"), Qt::CaseInsensitive) == 0
            || mode.compare(QStringLiteral("indexed-to-100"), Qt::CaseInsensitive) == 0
            || mode.compare(QStringLiteral("indexed"), Qt::CaseInsensitive) == 0
            || mode.compare(QStringLiteral("index100"), Qt::CaseInsensitive) == 0) {
            return PriceScaleMode::IndexedTo100;
        }
        return PriceScaleMode::Normal;
    }

    QString priceScaleModeName(PriceScaleMode mode)
    {
        switch (mode) {
        case PriceScaleMode::Normal:
            return QStringLiteral("normal");
        case PriceScaleMode::Logarithmic:
            return QStringLiteral("logarithmic");
        case PriceScaleMode::Percentage:
            return QStringLiteral("percentage");
        case PriceScaleMode::IndexedTo100:
            return QStringLiteral("indexedTo100");
        }
        return QStringLiteral("normal");
    }

    QString normalizedPriceAxisLabelAlignment(const QString& alignment)
    {
        if (alignment.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("left");
        }
        if (alignment.compare(QStringLiteral("center"), Qt::CaseInsensitive) == 0
            || alignment.compare(QStringLiteral("centre"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("center");
        }
        if (alignment.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("right");
        }
        return QStringLiteral("auto");
    }

    int crosshairModeFromVariant(const QVariant& value)
    {
        bool ok = false;
        const int numeric = value.toInt(&ok);
        if (ok) {
            return std::clamp(numeric,
                static_cast<int>(CrosshairMode::Normal),
                static_cast<int>(CrosshairMode::Hidden));
        }

        const QString text = value.toString();
        if (text.compare(QStringLiteral("magnet"), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(CrosshairMode::Magnet);
        }
        if (text.compare(QStringLiteral("magnetOHLC"), Qt::CaseInsensitive) == 0
            || text.compare(QStringLiteral("magnetOhlc"), Qt::CaseInsensitive) == 0
            || text.compare(QStringLiteral("magnet-ohlc"), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(CrosshairMode::MagnetOHLC);
        }
        if (text.compare(QStringLiteral("hidden"), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(CrosshairMode::Hidden);
        }
        return static_cast<int>(CrosshairMode::Normal);
    }

    QColor colorFromOption(const QVariant& value, const QColor& fallback = QColor())
    {
        return parseColorVariant(value, fallback);
    }

    QVector<qreal> dashPattern(LineStyle style, qreal lineWidth)
    {
        switch (style) {
        case LineStyle::Solid:
            return { };
        case LineStyle::Dotted:
            return { lineWidth, lineWidth };
        case LineStyle::Dashed:
            return { 2.0 * lineWidth, 2.0 * lineWidth };
        case LineStyle::LargeDashed:
            return { 6.0 * lineWidth, 6.0 * lineWidth };
        case LineStyle::SparseDotted:
            return { lineWidth, 4.0 * lineWidth };
        }
        return { };
    }

    void appendStyledLineSegment(Vertices& vertices, QPointF a, QPointF b, qreal width, LineStyle style)
    {
        const QVector<qreal> pattern = dashPattern(style, width);
        if (pattern.isEmpty()) {
            appendLineSegment(vertices, a, b, width);
            return;
        }

        const qreal dx = b.x() - a.x();
        const qreal dy = b.y() - a.y();
        const qreal length = std::hypot(dx, dy);
        if (length <= 0.0001) {
            return;
        }

        const QPointF direction(dx / length, dy / length);
        qreal offset = 0.0;
        int patternIndex = 0;
        bool drawing = true;
        while (offset < length) {
            const qreal run = std::max<qreal>(0.1, pattern.at(patternIndex % pattern.size()));
            const qreal nextOffset = std::min(length, offset + run);
            if (drawing && nextOffset > offset) {
                appendLineSegment(vertices, a + direction * offset, a + direction * nextOffset, width);
            }
            offset = nextOffset;
            ++patternIndex;
            drawing = !drawing;
        }
    }

    void appendStyledVerticalLine(Vertices& vertices, qreal x, qreal top, qreal bottom, qreal width, qreal dpr, LineStyle style)
    {
        if (style == LineStyle::Solid) {
            appendVerticalLine(vertices, x, top, bottom, width, dpr);
            return;
        }

        const qreal alignedX = alignToDevice(x, dpr);
        appendStyledLineSegment(vertices, QPointF(alignedX, top), QPointF(alignedX, bottom), width, style);
    }

    void appendStyledHorizontalLine(Vertices& vertices, qreal left, qreal right, qreal y, qreal width, qreal dpr, LineStyle style)
    {
        if (style == LineStyle::Solid) {
            appendHorizontalLine(vertices, left, right, y, width, dpr);
            return;
        }

        const qreal alignedY = alignToDevice(y, dpr);
        appendStyledLineSegment(vertices, QPointF(left, alignedY), QPointF(right, alignedY), width, style);
    }

    QPointF cubicPoint(const QPointF& p0, const QPointF& c1, const QPointF& c2, const QPointF& p3, qreal t)
    {
        const qreal u = 1.0 - t;
        return p0 * (u * u * u)
            + c1 * (3.0 * u * u * t)
            + c2 * (3.0 * u * t * t)
            + p3 * (t * t * t);
    }

    Vertices linePathPoints(const Vertices& points, LineType type)
    {
        if (points.size() <= 1 || type == LineType::Simple) {
            return points;
        }

        Vertices path;
        path.reserve(type == LineType::WithSteps ? points.size() * 2 : points.size() * 8);
        path.push_back(points.first());

        for (qsizetype i = 1; i < points.size(); ++i) {
            const QPointF previous = points.at(i - 1);
            const QPointF current = points.at(i);

            if (type == LineType::WithSteps) {
                path.push_back(QPointF(current.x(), previous.y()));
                path.push_back(current);
                continue;
            }

            const qreal dx = std::abs(current.x() - previous.x());
            const int segments = std::clamp(static_cast<int>(std::ceil(dx / 12.0)), 4, 16);
            const QPointF c1(previous.x() + (current.x() - previous.x()) * 0.5, previous.y());
            const QPointF c2(previous.x() + (current.x() - previous.x()) * 0.5, current.y());
            for (int segment = 1; segment <= segments; ++segment) {
                path.push_back(cubicPoint(previous, c1, c2, current, static_cast<qreal>(segment) / segments));
            }
        }

        return path;
    }

    bool samePoint(const QPointF& first, const QPointF& second)
    {
        return std::abs(first.x() - second.x()) <= 0.001
            && std::abs(first.y() - second.y()) <= 0.001;
    }

    void appendStrokeRun(QVector<StrokeRun>& runs, const QPointF& first, const QPointF& second, const QColor& color)
    {
        if (!color.isValid() || color.alpha() == 0 || !isFinitePoint(first) || !isFinitePoint(second) || samePoint(first, second)) {
            return;
        }

        if (!runs.isEmpty()
            && runs.last().color == color
            && !runs.last().points.isEmpty()
            && samePoint(runs.last().points.last(), first)) {
            runs.last().points.push_back(second);
            return;
        }

        StrokeRun run;
        run.color = color;
        run.points << first << second;
        runs.push_back(std::move(run));
    }

    QVector<StrokeRun> linePathStrokeRuns(const QVector<StrokeRun>& runs, LineType type)
    {
        QVector<StrokeRun> paths;
        paths.reserve(runs.size());
        for (const StrokeRun& run : runs) {
            if (run.points.size() < 2) {
                continue;
            }

            StrokeRun pathRun;
            pathRun.color = run.color;
            pathRun.points = linePathPoints(run.points, type);
            if (pathRun.points.size() >= 2) {
                paths.push_back(std::move(pathRun));
            }
        }
        return paths;
    }

    QPainterPath painterPathFromPoints(const Vertices& points)
    {
        QPainterPath path;
        if (points.isEmpty()) {
            return path;
        }

        path.moveTo(points.first());
        for (qsizetype i = 1; i < points.size(); ++i) {
            path.lineTo(points.at(i));
        }
        return path;
    }

    QVector<qreal> painterDashPattern(LineStyle style)
    {
        switch (style) {
        case LineStyle::Solid:
            return { };
        case LineStyle::Dotted:
            return { 1.0, 1.0 };
        case LineStyle::Dashed:
            return { 2.0, 2.0 };
        case LineStyle::LargeDashed:
            return { 6.0, 6.0 };
        case LineStyle::SparseDotted:
            return { 1.0, 4.0 };
        }
        return { };
    }

    void drawStrokeRun(QPainter& painter, const StrokeRun& run, LineStyle style, qreal width)
    {
        if (run.points.size() < 2 || width <= 0.0 || !run.color.isValid() || run.color.alpha() == 0) {
            return;
        }

        QPen pen(run.color, width, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        const QVector<qreal> pattern = painterDashPattern(style);
        if (!pattern.isEmpty()) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern(pattern);
        }

        painter.setPen(pen);
        painter.drawPath(painterPathFromPoints(run.points));
    }

    QSGSimpleTextureNode* strokedLinesTextureNode(QQuickWindow* window,
        const QRectF& logicalRect,
        const QVector<StrokeRun>& runs,
        LineStyle style,
        qreal width,
        qreal dpr)
    {
        if (!window || logicalRect.width() <= 0.0 || logicalRect.height() <= 0.0 || width <= 0.0) {
            return nullptr;
        }

        const bool hasDrawableRun = std::any_of(runs.cbegin(), runs.cend(), [](const StrokeRun& run) {
            return run.points.size() >= 2 && run.color.isValid() && run.color.alpha() > 0;
        });
        if (!hasDrawableRun) {
            return nullptr;
        }

        const QSize imageSize(std::max(1, static_cast<int>(std::ceil(logicalRect.width() * dpr))),
            std::max(1, static_cast<int>(std::ceil(logicalRect.height() * dpr))));
        QImage image(imageSize, QImage::Format_RGBA8888_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(-logicalRect.topLeft());
        for (const StrokeRun& run : runs) {
            drawStrokeRun(painter, run, style, width);
        }
        painter.end();

        QSGTexture* texture = window->createTextureFromImage(image);
        if (!texture) {
            return nullptr;
        }

        auto* node = new QSGSimpleTextureNode;
        node->setTexture(texture);
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        node->setRect(QRectF(logicalRect.topLeft(),
            QSizeF(imageSize.width() / dpr, imageSize.height() / dpr)));
        return node;
    }

    void appendFillToBoundary(Vertices& vertices, const Vertices& path, qreal boundaryY)
    {
        for (qsizetype i = 1; i < path.size(); ++i) {
            const QPointF previous = path.at(i - 1);
            const QPointF current = path.at(i);
            vertices << previous << QPointF(previous.x(), boundaryY) << current;
            vertices << current << QPointF(previous.x(), boundaryY) << QPointF(current.x(), boundaryY);
        }
    }

    void appendCircle(Vertices& vertices, const QPointF& center, qreal radius, int segments = 16)
    {
        if (radius <= 0.0) {
            return;
        }
        constexpr qreal twoPi = 6.28318530717958647692;
        for (int i = 0; i < segments; ++i) {
            const qreal a0 = (twoPi * i) / segments;
            const qreal a1 = (twoPi * (i + 1)) / segments;
            vertices << center
                     << QPointF(center.x() + std::cos(a0) * radius, center.y() + std::sin(a0) * radius)
                     << QPointF(center.x() + std::cos(a1) * radius, center.y() + std::sin(a1) * radius);
        }
    }

    void appendTriangle(Vertices& vertices, const QPointF& a, const QPointF& b, const QPointF& c)
    {
        vertices << a << b << c;
    }

    qreal candleBodyWidth(qreal barSpacing, qreal dpr)
    {
        const qreal px = oneDevicePixel(dpr);
        const qreal width = std::clamp(barSpacing * 0.72, px, std::max(px, barSpacing - px));
        return std::max(px, alignToDevice(width, dpr));
    }

    void clearChildren(QSGNode* node)
    {
        while (QSGNode* child = node->firstChild()) {
            node->removeChildNode(child);
            delete child;
        }
    }

    QColor transparent()
    {
        return QColor(0, 0, 0, 0);
    }

    QColor averageColor(const QColor& first, const QColor& second)
    {
        if (!first.isValid()) {
            return second;
        }
        if (!second.isValid()) {
            return first;
        }
        return QColor((first.red() + second.red()) / 2,
            (first.green() + second.green()) / 2,
            (first.blue() + second.blue()) / 2,
            (first.alpha() + second.alpha()) / 2);
    }

    QColor colorOr(const QColor& color, const QColor& fallback)
    {
        return color.isValid() ? color : fallback;
    }

    QColor mixColor(const QColor& first, const QColor& second, qreal ratio)
    {
        const qreal t = std::clamp<qreal>(ratio, 0.0, 1.0);
        return QColor(std::round(first.red() + (second.red() - first.red()) * t),
            std::round(first.green() + (second.green() - first.green()) * t),
            std::round(first.blue() + (second.blue() - first.blue()) * t),
            std::round(first.alpha() + (second.alpha() - first.alpha()) * t));
    }

    QColor colorForY(qreal y, qreal top, qreal bottom, const QColor& topColor, const QColor& bottomColor)
    {
        if (std::abs(bottom - top) <= 0.0001) {
            return averageColor(topColor, bottomColor);
        }
        return mixColor(topColor, bottomColor, (y - top) / (bottom - top));
    }

    bool isAtPricePosition(MarkerPosition position)
    {
        return position == MarkerPosition::AtPriceTop
            || position == MarkerPosition::AtPriceMiddle
            || position == MarkerPosition::AtPriceBottom;
    }

    qreal resolvedPointMarkerRadius(qreal configuredRadius, qreal lineWidth)
    {
        return configuredRadius > 0.0 ? configuredRadius : lineWidth * 0.5 + 2.0;
    }

    qreal boundedZoomFactor(qreal factor)
    {
        if (!std::isfinite(factor)) {
            return 1.0;
        }
        return std::clamp<qreal>(factor, 0.05, 20.0);
    }

    template <typename Point>
    const Point* findPointByTime(const QVector<Point>& points, TimePoint time)
    {
        const auto it = std::lower_bound(points.cbegin(), points.cend(), time, [](const Point& point, TimePoint value) {
            return point.time < value;
        });
        return it != points.cend() && it->time == time ? &(*it) : nullptr;
    }

    bool hasWhitespaceBetween(const QVector<TimePoint>& whitespaceTimes, TimePoint first, TimePoint second)
    {
        if (whitespaceTimes.isEmpty() || first == second) {
            return false;
        }

        const TimePoint from = std::min(first, second);
        const TimePoint to = std::max(first, second);
        const auto it = std::upper_bound(whitespaceTimes.cbegin(), whitespaceTimes.cend(), from);
        return it != whitespaceTimes.cend() && *it < to;
    }

    QVariantMap lineDataMap(const LineDataPoint& point)
    {
        QVariantMap row {
            { QStringLiteral("time"), point.time },
            { QStringLiteral("value"), point.value },
        };
        if (point.color.isValid()) {
            row.insert(QStringLiteral("color"), point.color);
        }
        if (point.lineColor.isValid()) {
            row.insert(QStringLiteral("lineColor"), point.lineColor);
        }
        if (point.topColor.isValid()) {
            row.insert(QStringLiteral("topColor"), point.topColor);
        }
        if (point.bottomColor.isValid()) {
            row.insert(QStringLiteral("bottomColor"), point.bottomColor);
        }
        if (point.topFillColor1.isValid()) {
            row.insert(QStringLiteral("topFillColor1"), point.topFillColor1);
        }
        if (point.topFillColor2.isValid()) {
            row.insert(QStringLiteral("topFillColor2"), point.topFillColor2);
        }
        if (point.topLineColor.isValid()) {
            row.insert(QStringLiteral("topLineColor"), point.topLineColor);
        }
        if (point.bottomFillColor1.isValid()) {
            row.insert(QStringLiteral("bottomFillColor1"), point.bottomFillColor1);
        }
        if (point.bottomFillColor2.isValid()) {
            row.insert(QStringLiteral("bottomFillColor2"), point.bottomFillColor2);
        }
        if (point.bottomLineColor.isValid()) {
            row.insert(QStringLiteral("bottomLineColor"), point.bottomLineColor);
        }
        return row;
    }

    QVariantMap ohlcDataMap(const CandlestickDataPoint& point)
    {
        QVariantMap row {
            { QStringLiteral("time"), point.time },
            { QStringLiteral("open"), point.open },
            { QStringLiteral("high"), point.high },
            { QStringLiteral("low"), point.low },
            { QStringLiteral("close"), point.close },
        };
        if (point.color.isValid()) {
            row.insert(QStringLiteral("color"), point.color);
        }
        if (point.borderColor.isValid()) {
            row.insert(QStringLiteral("borderColor"), point.borderColor);
        }
        if (point.wickColor.isValid()) {
            row.insert(QStringLiteral("wickColor"), point.wickColor);
        }
        return row;
    }

    QVariantMap histogramDataMap(const HistogramDataPoint& point)
    {
        QVariantMap row {
            { QStringLiteral("time"), point.time },
            { QStringLiteral("value"), point.value },
        };
        if (point.color.isValid()) {
            row.insert(QStringLiteral("color"), point.color);
        }
        return row;
    }

    QVariantMap seriesDataAt(const ChartSeries* series, TimePoint time)
    {
        if (!series) {
            return { };
        }

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            if (const CandlestickDataPoint* point = findPointByTime(series->model().candlesticks(), time)) {
                return ohlcDataMap(*point);
            }
            return { };
        }
        if (qobject_cast<const LineSeries*>(series) || qobject_cast<const AreaSeries*>(series) || qobject_cast<const BaselineSeries*>(series)) {
            if (const LineDataPoint* point = findPointByTime(series->model().linePoints(), time)) {
                return lineDataMap(*point);
            }
            return { };
        }
        if (qobject_cast<const HistogramSeries*>(series)) {
            if (const HistogramDataPoint* point = findPointByTime(series->model().histogramPoints(), time)) {
                return histogramDataMap(*point);
            }
        }
        return { };
    }

    QVector<double> crosshairCandidatePrices(const ChartSeries* series,
        TimePoint time,
        CrosshairMode mode)
    {
        QVector<double> prices;
        if (!series) {
            return prices;
        }

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            const CandlestickDataPoint* point = findPointByTime(series->model().candlesticks(), time);
            if (!point) {
                return prices;
            }
            if (mode == CrosshairMode::MagnetOHLC) {
                prices << point->open << point->high << point->low << point->close;
            } else {
                prices << point->close;
            }
            return prices;
        }

        if (qobject_cast<const LineSeries*>(series) || qobject_cast<const AreaSeries*>(series) || qobject_cast<const BaselineSeries*>(series)) {
            if (const LineDataPoint* point = findPointByTime(series->model().linePoints(), time)) {
                prices << point->value;
            }
            return prices;
        }

        if (qobject_cast<const HistogramSeries*>(series)) {
            if (const HistogramDataPoint* point = findPointByTime(series->model().histogramPoints(), time)) {
                prices << point->value;
            }
        }
        return prices;
    }

    QVariantMap pointPayload(const QPointF& point)
    {
        return {
            { QStringLiteral("x"), point.x() },
            { QStringLiteral("y"), point.y() },
        };
    }

    QVariantMap mouseEventPayload(const QMouseEvent* event, const QString& type)
    {
        if (!event) {
            return { };
        }
        return {
            { QStringLiteral("type"), type },
            { QStringLiteral("point"), pointPayload(event->position()) },
            { QStringLiteral("button"), static_cast<int>(event->button()) },
            { QStringLiteral("buttons"), event->buttons().toInt() },
            { QStringLiteral("modifiers"), event->modifiers().toInt() },
        };
    }

    QVariantMap hoverEventPayload(const QHoverEvent* event, const QString& type)
    {
        if (!event) {
            return { };
        }
        return {
            { QStringLiteral("type"), type },
            { QStringLiteral("point"), pointPayload(event->position()) },
            { QStringLiteral("oldPoint"), pointPayload(event->oldPosF()) },
            { QStringLiteral("modifiers"), event->modifiers().toInt() },
        };
    }

    QVariantMap wheelEventPayload(const QWheelEvent* event)
    {
        if (!event) {
            return { };
        }
        return {
            { QStringLiteral("type"), QStringLiteral("wheel") },
            { QStringLiteral("point"), pointPayload(event->position()) },
            { QStringLiteral("pixelDeltaX"), event->pixelDelta().x() },
            { QStringLiteral("pixelDeltaY"), event->pixelDelta().y() },
            { QStringLiteral("angleDeltaX"), event->angleDelta().x() },
            { QStringLiteral("angleDeltaY"), event->angleDelta().y() },
            { QStringLiteral("buttons"), event->buttons().toInt() },
            { QStringLiteral("modifiers"), event->modifiers().toInt() },
        };
    }

    QVariantMap touchEventPayload(const QTouchEvent* event, const QString& type, const QPointF& point)
    {
        if (!event) {
            return { };
        }
        return {
            { QStringLiteral("type"), type },
            { QStringLiteral("point"), pointPayload(point) },
            { QStringLiteral("pointCount"), event->points().size() },
            { QStringLiteral("modifiers"), event->modifiers().toInt() },
        };
    }

} // namespace

struct FinancialChartItem::LabelTexture {
    QSizeF logicalSize;
    QImage image;
    QSGTexture* texture = nullptr;
    QPointer<QQuickWindow> textureWindow;

    ~LabelTexture()
    {
        delete texture;
    }
};

class FinancialChartItem::TextTextureCache {
public:
    LabelTexture* get(const QString& text,
        const QFont& font,
        const QColor& foreground,
        const QColor& background,
        qreal dpr)
    {
        if (text.isEmpty()) {
            return nullptr;
        }

        const QString key = text + QLatin1Char('|') + font.toString() + QLatin1Char('|')
            + foreground.name(QColor::HexArgb) + QLatin1Char('|')
            + background.name(QColor::HexArgb) + QLatin1Char('|')
            + QString::number(dpr, 'f', 2);

        const auto existing = m_entries.constFind(key);
        if (existing != m_entries.constEnd()) {
            return existing.value().get();
        }

        constexpr qreal horizontalPadding = 6.0;
        constexpr qreal verticalPadding = 2.0;
        const QFontMetricsF metrics(font);
        const QStringList lines = text.split(QLatin1Char('\n'));
        qreal textWidth = 0.0;
        for (const QString& line : lines) {
            textWidth = std::max(textWidth, metrics.horizontalAdvance(line));
        }
        const qsizetype lineCount = std::max<qsizetype>(1, lines.size());
        const QSizeF logicalSize(std::ceil(textWidth + horizontalPadding * 2.0),
            std::ceil(metrics.height() * lineCount + verticalPadding * 2.0));
        QImage image((logicalSize * dpr).toSize(), QImage::Format_RGBA8888_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(font);
        if (background.alpha() > 0) {
            painter.fillRect(QRectF(QPointF(0, 0), logicalSize), background);
        }
        painter.setPen(foreground);
        painter.drawText(QRectF(horizontalPadding, 0.0,
                             logicalSize.width() - horizontalPadding * 2.0,
                             logicalSize.height()),
            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
            text);
        painter.end();

        auto entry = std::make_shared<LabelTexture>();
        entry->logicalSize = logicalSize;
        entry->image = std::move(image);
        LabelTexture* raw = entry.get();
        m_entries.insert(key, std::move(entry));
        return raw;
    }

    QSGTexture* textureFor(LabelTexture* label, QQuickWindow* window)
    {
        if (!label || !window || label->image.isNull()) {
            return nullptr;
        }
        if (label->texture && label->textureWindow == window) {
            return label->texture;
        }

        delete label->texture;
        label->texture = window->createTextureFromImage(label->image);
        label->textureWindow = window;
        if (label->texture) {
            label->texture->setFiltering(QSGTexture::Linear);
        }
        return label->texture;
    }

    void clear()
    {
        m_entries.clear();
    }

private:
    QHash<QString, std::shared_ptr<LabelTexture>> m_entries;
};

class FinancialChartItem::ChartRootNode final : public QSGNode {
public:
    TextTextureCache textCache;
};

FinancialChartItem::FinancialChartItem(QQuickItem* parent)
    : QQuickItem(parent)
    , m_ownedTheme(std::make_unique<ChartTheme>())
    , m_locale(QLocale())
{
    m_theme = m_ownedTheme.get();
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setAcceptTouchEvents(true);

    connect(m_theme, &ChartTheme::changed, this, [this] {
        m_textCacheDirty = true;
        scheduleVisualUpdate();
        Q_EMIT themeChanged();
    });
}

FinancialChartItem::~FinancialChartItem() = default;

QQmlListProperty<ChartSeries> FinancialChartItem::series()
{
    return QQmlListProperty<ChartSeries>(this,
        this,
        &FinancialChartItem::appendSeries,
        &FinancialChartItem::seriesCount,
        &FinancialChartItem::seriesAt,
        &FinancialChartItem::clearSeries);
}

void FinancialChartItem::setTheme(ChartTheme* theme)
{
    ChartTheme* nextTheme = theme ? theme : m_ownedTheme.get();
    if (m_theme == nextTheme) {
        return;
    }

    if (m_theme) {
        disconnect(m_theme, nullptr, this, nullptr);
    }
    m_theme = nextTheme;
    connect(m_theme, &ChartTheme::changed, this, [this] {
        m_textCacheDirty = true;
        scheduleVisualUpdate();
        Q_EMIT themeChanged();
    });
    if (nextTheme != m_ownedTheme.get()) {
        connect(nextTheme, &QObject::destroyed, this, [this] {
            setTheme(nullptr);
        });
    }

    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT themeChanged();
}

void FinancialChartItem::setPaneCount(int count)
{
    const int normalized = std::max(1, count);
    if (m_paneCount == normalized) {
        return;
    }
    m_paneCount = normalized;
    scheduleDataUpdate();
    Q_EMIT paneCountChanged();
}

void FinancialChartItem::setGridVisible(bool visible)
{
    if (m_gridVisible == visible) {
        return;
    }
    m_gridVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

void FinancialChartItem::setVerticalGridVisible(bool visible)
{
    if (m_verticalGridVisible == visible) {
        return;
    }
    m_verticalGridVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

void FinancialChartItem::setHorizontalGridVisible(bool visible)
{
    if (m_horizontalGridVisible == visible) {
        return;
    }
    m_horizontalGridVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

int FinancialChartItem::gridLineStyle() const noexcept
{
    return m_verticalGridLineStyle == m_horizontalGridLineStyle ? m_verticalGridLineStyle : -1;
}

void FinancialChartItem::setGridLineStyle(int style)
{
    const int normalized = std::clamp(style, static_cast<int>(LineStyle::Solid), static_cast<int>(LineStyle::SparseDotted));
    if (m_verticalGridLineStyle == normalized && m_horizontalGridLineStyle == normalized) {
        return;
    }
    m_verticalGridLineStyle = normalized;
    m_horizontalGridLineStyle = normalized;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

void FinancialChartItem::setVerticalGridLineStyle(int style)
{
    const int normalized = std::clamp(style, static_cast<int>(LineStyle::Solid), static_cast<int>(LineStyle::SparseDotted));
    if (m_verticalGridLineStyle == normalized) {
        return;
    }
    m_verticalGridLineStyle = normalized;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

void FinancialChartItem::setHorizontalGridLineStyle(int style)
{
    const int normalized = std::clamp(style, static_cast<int>(LineStyle::Solid), static_cast<int>(LineStyle::SparseDotted));
    if (m_horizontalGridLineStyle == normalized) {
        return;
    }
    m_horizontalGridLineStyle = normalized;
    scheduleVisualUpdate();
    Q_EMIT gridVisibleChanged();
}

void FinancialChartItem::setInteractive(bool interactive)
{
    if (m_interactive == interactive) {
        return;
    }
    m_interactive = interactive;
    Q_EMIT interactiveChanged();
}

void FinancialChartItem::setHandleScroll(bool enabled)
{
    if (m_handleScroll == enabled) {
        return;
    }
    m_handleScroll = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setHandleScale(bool enabled)
{
    if (m_handleScale == enabled) {
        return;
    }
    m_handleScale = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setMouseWheelScroll(bool enabled)
{
    if (m_mouseWheelScroll == enabled) {
        return;
    }
    m_mouseWheelScroll = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setMouseWheelScale(bool enabled)
{
    if (m_mouseWheelScale == enabled) {
        return;
    }
    m_mouseWheelScale = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setPressedMouseMoveScroll(bool enabled)
{
    if (m_pressedMouseMoveScroll == enabled) {
        return;
    }
    m_pressedMouseMoveScroll = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setHorizontalTouchDrag(bool enabled)
{
    if (m_horizontalTouchDrag == enabled) {
        return;
    }
    m_horizontalTouchDrag = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setVerticalTouchDrag(bool enabled)
{
    if (m_verticalTouchDrag == enabled) {
        return;
    }
    m_verticalTouchDrag = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setPinchScale(bool enabled)
{
    if (m_pinchScale == enabled) {
        return;
    }
    m_pinchScale = enabled;
    Q_EMIT interactionOptionsChanged();
}

void FinancialChartItem::setTimeScaleVisible(bool visible)
{
    if (m_timeScaleVisible == visible) {
        return;
    }
    m_timeScaleVisible = visible;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT timeScaleVisibleChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setPriceScaleVisible(bool visible)
{
    if (m_priceScaleVisible == visible) {
        return;
    }
    m_priceScaleVisible = visible;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setLeftPriceScaleVisible(bool visible)
{
    if (m_leftPriceScaleVisible == visible) {
        return;
    }
    m_leftPriceScaleVisible = visible;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setPriceAxisBorderVisible(bool visible)
{
    if (m_priceAxisBorderVisible == visible) {
        return;
    }
    m_priceAxisBorderVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setLeftPriceAxisBorderVisible(bool visible)
{
    if (m_leftPriceAxisBorderVisible == visible) {
        return;
    }
    m_leftPriceAxisBorderVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setTimeAxisBorderVisible(bool visible)
{
    if (m_timeAxisBorderVisible == visible) {
        return;
    }
    m_timeAxisBorderVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceAxisTickMarksVisible(bool visible)
{
    if (m_priceAxisTickMarksVisible == visible) {
        return;
    }
    m_priceAxisTickMarksVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setLeftPriceAxisTickMarksVisible(bool visible)
{
    if (m_leftPriceAxisTickMarksVisible == visible) {
        return;
    }
    m_leftPriceAxisTickMarksVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setTimeAxisTickMarksVisible(bool visible)
{
    if (m_timeAxisTickMarksVisible == visible) {
        return;
    }
    m_timeAxisTickMarksVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setTimeVisible(bool visible)
{
    if (m_timeVisible == visible) {
        return;
    }
    m_timeVisible = visible;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setSecondsVisible(bool visible)
{
    if (m_secondsVisible == visible) {
        return;
    }
    m_secondsVisible = visible;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setDateFormat(const QString& format)
{
    if (format.isEmpty() || m_dateFormat == format) {
        return;
    }
    m_dateFormat = format;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setTimeFormat(const QString& format)
{
    if (format.isEmpty() || m_timeFormat == format) {
        return;
    }
    m_timeFormat = format;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setTimeWithSecondsFormat(const QString& format)
{
    if (format.isEmpty() || m_timeWithSecondsFormat == format) {
        return;
    }
    m_timeWithSecondsFormat = format;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setTimeAxisMinimumHeight(qreal height)
{
    if (!std::isfinite(height)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(height, 0.0, 96.0);
    if (qFuzzyCompare(m_timeAxisMinimumHeight, normalized)) {
        return;
    }
    m_timeAxisMinimumHeight = normalized;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setTimeAxisMaxLabelWidth(int width)
{
    const int normalized = std::clamp(width, 24, 240);
    if (m_timeAxisMaxLabelWidth == normalized) {
        return;
    }
    m_timeAxisMaxLabelWidth = normalized;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setTimeAxisUniformDistribution(bool uniform)
{
    if (m_timeAxisUniformDistribution == uniform) {
        return;
    }
    m_timeAxisUniformDistribution = uniform;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setTimeAxisBoldLabels(bool bold)
{
    if (m_timeAxisBoldLabels == bold) {
        return;
    }
    m_timeAxisBoldLabels = bold;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceAxisMinimumWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 160.0);
    if (qFuzzyCompare(m_priceAxisMinimumWidth, normalized)) {
        return;
    }
    m_priceAxisMinimumWidth = normalized;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setLeftPriceAxisMinimumWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 160.0);
    if (qFuzzyCompare(m_leftPriceAxisMinimumWidth, normalized)) {
        return;
    }
    m_leftPriceAxisMinimumWidth = normalized;
    m_textCacheDirty = true;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    scheduleVisualUpdate();
    Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setPriceAxisTextColor(const QColor& color)
{
    if (m_priceAxisTextColor == color) {
        return;
    }
    m_priceAxisTextColor = color;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceAxisLabelAlignment(const QString& alignment)
{
    const QString normalized = normalizedPriceAxisLabelAlignment(alignment);
    if (m_priceAxisLabelAlignment == normalized) {
        return;
    }
    m_priceAxisLabelAlignment = normalized;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceScaleTopMargin(qreal margin)
{
    if (!std::isfinite(margin)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(margin, 0.0, 0.45);
    if (qFuzzyCompare(m_priceScaleTopMargin, normalized)) {
        return;
    }
    m_priceScaleTopMargin = normalized;
    if (m_priceScaleTopMargin + m_priceScaleBottomMargin > 0.9) {
        m_priceScaleBottomMargin = 0.9 - m_priceScaleTopMargin;
    }
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceScaleBottomMargin(qreal margin)
{
    if (!std::isfinite(margin)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(margin, 0.0, 0.45);
    if (qFuzzyCompare(m_priceScaleBottomMargin, normalized)) {
        return;
    }
    m_priceScaleBottomMargin = normalized;
    if (m_priceScaleTopMargin + m_priceScaleBottomMargin > 0.9) {
        m_priceScaleTopMargin = 0.9 - m_priceScaleBottomMargin;
    }
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceTickSpacing(qreal spacing)
{
    if (!std::isfinite(spacing)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(spacing, 24.0, 240.0);
    if (qFuzzyCompare(m_priceTickSpacing, normalized)) {
        return;
    }
    m_priceTickSpacing = normalized;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setAutoScale(bool enabled)
{
    if (m_autoScale == enabled) {
        if (enabled && !m_manualPriceRanges.isEmpty()) {
            m_manualPriceRanges.clear();
            scheduleVisualUpdate();
            Q_EMIT axisOptionsChanged();
        }
        return;
    }

    if (enabled) {
        m_manualPriceRanges.clear();
    } else {
        const ChartLayout layout = currentLayoutWithSyncedTimeScale();
        const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
        for (const ChartLayout::Pane& pane : layout.panes) {
            QSet<QString> scaleIds {
                QStringLiteral("right"),
                QStringLiteral("left"),
            };

            for (const ChartSeries* series : m_series) {
                if (series && series->pane() == pane.index) {
                    scaleIds.insert(normalizedPriceScaleId(series->priceScaleId()));
                }
            }

            for (const QString& scaleId : scaleIds) {
                const QString normalizedId = normalizedPriceScaleId(scaleId);
                const QString key = priceScaleKey(pane.index, normalizedId);
                const PriceRange current = resolvedPriceRange(pane.index, normalizedId, visibleFrom, visibleTo);
                if (current.isValid()) {
                    m_manualPriceRanges.insert(key, current);
                }
            }
        }
    }

    m_autoScale = enabled;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPriceScaleInverted(bool inverted)
{
    if (m_priceScaleInverted == inverted) {
        return;
    }
    m_priceScaleInverted = inverted;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

QString FinancialChartItem::priceScaleMode() const
{
    return priceScaleModeName(m_priceScaleMode);
}

void FinancialChartItem::setPriceScaleMode(const QString& mode)
{
    const PriceScaleMode normalized = priceScaleModeFromString(mode);
    if (m_priceScaleMode == normalized) {
        return;
    }
    m_priceScaleMode = normalized;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setPreserveEmptyPanes(bool preserve)
{
    if (m_preserveEmptyPanes == preserve) {
        return;
    }
    m_preserveEmptyPanes = preserve;
    scheduleDataUpdate();
    Q_EMIT paneCountChanged();
}

void FinancialChartItem::setPaneSeparatorsResizable(bool resizable)
{
    if (m_paneSeparatorsResizable == resizable) {
        return;
    }
    m_paneSeparatorsResizable = resizable;
    if (!m_paneSeparatorsResizable) {
        m_hoveredPaneSeparator = -1;
        m_draggedPaneSeparator = -1;
    }
    scheduleVisualUpdate();
    Q_EMIT paneSeparatorOptionsChanged();
}

void FinancialChartItem::setLocaleName(const QString& localeName)
{
    const QLocale nextLocale(localeName);
    if (m_locale == nextLocale) {
        return;
    }
    m_locale = nextLocale;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setPricePrecision(int precision)
{
    const int normalized = std::clamp(precision, 0, 8);
    if (m_pricePrecision == normalized) {
        return;
    }
    m_pricePrecision = normalized;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

QString FinancialChartItem::priceFormat() const
{
    return priceFormatName(m_priceFormat);
}

void FinancialChartItem::setPriceFormat(const QString& format)
{
    const PriceFormat nextFormat = priceFormatFromString(format);
    if (m_priceFormat == nextFormat) {
        return;
    }
    m_priceFormat = nextFormat;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setTimeFormatter(const QJSValue& formatter)
{
    m_timeFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setTimeTickMarkFormatter(const QJSValue& formatter)
{
    m_timeTickMarkFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setPriceFormatter(const QJSValue& formatter)
{
    m_priceFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setPercentageFormatter(const QJSValue& formatter)
{
    m_percentageFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setPriceTickMarkFormatter(const QJSValue& formatter)
{
    m_priceTickMarkFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setPercentageTickMarkFormatter(const QJSValue& formatter)
{
    m_percentageTickMarkFormatter = formatter;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT localizationChanged();
}

void FinancialChartItem::setBarSpacing(qreal spacing)
{
    if (!std::isfinite(spacing)) {
        return;
    }
    if (qFuzzyCompare(m_model.timeScale().barSpacing(), spacing)) {
        return;
    }
    m_model.timeScale().setBarSpacing(spacing);
    m_fitContentRequested = false;
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setRightOffset(qreal offset)
{
    if (!std::isfinite(offset)) {
        return;
    }
    if (qFuzzyCompare(m_model.timeScale().rightOffset(), offset)) {
        return;
    }
    m_model.timeScale().setRightOffset(offset);
    m_fitContentRequested = false;
    scheduleVisualUpdate();
    Q_EMIT rightOffsetChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setRightOffsetPixels(qreal offsetPixels)
{
    if (!std::isfinite(offsetPixels)) {
        return;
    }
    const qreal spacing = std::max<qreal>(0.1, m_model.timeScale().barSpacing());
    setRightOffset(offsetPixels / spacing);
}

void FinancialChartItem::setMinBarSpacing(qreal spacing)
{
    if (!std::isfinite(spacing)) {
        return;
    }
    m_model.timeScale().setSpacingLimits(spacing, m_model.timeScale().maxBarSpacing());
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setMaxBarSpacing(qreal spacing)
{
    if (!std::isfinite(spacing)) {
        return;
    }
    m_model.timeScale().setSpacingLimits(m_model.timeScale().minBarSpacing(), spacing);
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setFixLeftEdge(bool enabled)
{
    if (fixLeftEdge() == enabled) {
        return;
    }
    m_model.timeScale().setFixedEdges(enabled, fixRightEdge());
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    Q_EMIT rightOffsetChanged();
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setFixRightEdge(bool enabled)
{
    if (fixRightEdge() == enabled) {
        return;
    }
    m_model.timeScale().setFixedEdges(fixLeftEdge(), enabled);
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    Q_EMIT rightOffsetChanged();
    Q_EMIT axisOptionsChanged();
    emitVisibleRangeSignals();
}

void FinancialChartItem::setLockVisibleTimeRangeOnResize(bool enabled)
{
    if (lockVisibleTimeRangeOnResize() == enabled) {
        return;
    }
    m_model.timeScale().setLockVisibleRangeOnResize(enabled);
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setRightBarStaysOnScroll(bool enabled)
{
    if (rightBarStaysOnScroll() == enabled) {
        return;
    }
    m_model.timeScale().setRightBarStaysOnScroll(enabled);
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setShiftVisibleRangeOnNewBar(bool enabled)
{
    if (shiftVisibleRangeOnNewBar() == enabled) {
        return;
    }
    m_model.timeScale().setShiftVisibleRangeOnNewBar(enabled);
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::setCrosshairMode(int mode)
{
    const int normalized = std::clamp(mode,
        static_cast<int>(CrosshairMode::Normal),
        static_cast<int>(CrosshairMode::Hidden));
    if (m_crosshairMode == normalized) {
        return;
    }
    m_crosshairMode = normalized;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setVerticalCrosshairLineVisible(bool visible)
{
    if (m_verticalCrosshairLineVisible == visible) {
        return;
    }
    m_verticalCrosshairLineVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setHorizontalCrosshairLineVisible(bool visible)
{
    if (m_horizontalCrosshairLineVisible == visible) {
        return;
    }
    m_horizontalCrosshairLineVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setVerticalCrosshairLineColor(const QColor& color)
{
    if (m_verticalCrosshairLineColor == color) {
        return;
    }
    m_verticalCrosshairLineColor = color;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setHorizontalCrosshairLineColor(const QColor& color)
{
    if (m_horizontalCrosshairLineColor == color) {
        return;
    }
    m_horizontalCrosshairLineColor = color;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setVerticalCrosshairLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 16.0);
    if (qFuzzyCompare(m_verticalCrosshairLineWidth, normalized)) {
        return;
    }
    m_verticalCrosshairLineWidth = normalized;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setHorizontalCrosshairLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 16.0);
    if (qFuzzyCompare(m_horizontalCrosshairLineWidth, normalized)) {
        return;
    }
    m_horizontalCrosshairLineWidth = normalized;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setVerticalCrosshairLineStyle(int style)
{
    const int normalized = std::clamp(style,
        static_cast<int>(LineStyle::Solid),
        static_cast<int>(LineStyle::SparseDotted));
    if (m_verticalCrosshairLineStyle == normalized) {
        return;
    }
    m_verticalCrosshairLineStyle = normalized;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setHorizontalCrosshairLineStyle(int style)
{
    const int normalized = std::clamp(style,
        static_cast<int>(LineStyle::Solid),
        static_cast<int>(LineStyle::SparseDotted));
    if (m_horizontalCrosshairLineStyle == normalized) {
        return;
    }
    m_horizontalCrosshairLineStyle = normalized;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setTimeCrosshairLabelVisible(bool visible)
{
    if (m_timeCrosshairLabelVisible == visible) {
        return;
    }
    m_timeCrosshairLabelVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setPriceCrosshairLabelVisible(bool visible)
{
    if (m_priceCrosshairLabelVisible == visible) {
        return;
    }
    m_priceCrosshairLabelVisible = visible;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setTimeCrosshairLabelBackground(const QColor& color)
{
    if (m_timeCrosshairLabelBackground == color) {
        return;
    }
    m_timeCrosshairLabelBackground = color;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setPriceCrosshairLabelBackground(const QColor& color)
{
    if (m_priceCrosshairLabelBackground == color) {
        return;
    }
    m_priceCrosshairLabelBackground = color;
    m_textCacheDirty = true;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::setSnapCrosshairToHiddenSeries(bool snap)
{
    if (m_snapCrosshairToHiddenSeries == snap) {
        return;
    }
    m_snapCrosshairToHiddenSeries = snap;
    scheduleVisualUpdate();
    Q_EMIT crosshairOptionsChanged();
}

void FinancialChartItem::fitContent()
{
    m_fitContentRequested = true;
    const ChartLayout layout = currentLayout();
    const bool fitted = syncTimeScaleWithLayout(layout);
    scheduleVisualUpdate();
    if (fitted) {
        Q_EMIT barSpacingChanged();
        Q_EMIT rightOffsetChanged();
        emitVisibleRangeSignals();
    }
}

void FinancialChartItem::resetTimeScale()
{
    fitContent();
}

int FinancialChartItem::addPane()
{
    const int index = m_paneCount;
    setPaneCount(m_paneCount + 1);
    return index;
}

void FinancialChartItem::removePane(int index)
{
    if (m_paneCount <= 1 || index < 0 || index >= m_paneCount) {
        return;
    }

    const int fallbackPane = std::max(0, index - 1);
    for (ChartSeries* series : m_series) {
        if (!series) {
            continue;
        }
        if (series->pane() == index) {
            series->setPane(fallbackPane);
        } else if (series->pane() > index) {
            series->setPane(series->pane() - 1);
        }
    }
    m_model.removePaneSizing(index);
    setPaneCount(m_paneCount - 1);
    scheduleDataUpdate();
}

void FinancialChartItem::swapPanes(int first, int second)
{
    if (first == second || first < 0 || second < 0 || first >= m_paneCount || second >= m_paneCount) {
        return;
    }

    for (ChartSeries* series : m_series) {
        if (!series) {
            continue;
        }
        if (series->pane() == first) {
            series->setPane(second);
        } else if (series->pane() == second) {
            series->setPane(first);
        }
    }
    m_model.swapPaneSizing(first, second);
    scheduleDataUpdate();
}

void FinancialChartItem::removeSeries(ChartSeries* series)
{
    detachSeries(series);
    scheduleDataUpdate();
}

void FinancialChartItem::resizeChart(qreal width, qreal height)
{
    const qreal safeWidth = std::isfinite(width) ? std::max<qreal>(0.0, width) : 0.0;
    const qreal safeHeight = std::isfinite(height) ? std::max<qreal>(0.0, height) : 0.0;
    setSize(QSizeF(safeWidth, safeHeight));
}

qreal FinancialChartItem::chartWidth() const
{
    return width();
}

qreal FinancialChartItem::chartHeight() const
{
    return height();
}

QVariantMap FinancialChartItem::coordinateToData(const QPointF& point) const
{
    return crosshairPayload(point);
}

QVariantMap FinancialChartItem::visibleLogicalRange() const
{
    currentLayoutWithSyncedTimeScale();
    const LogicalRange range = m_model.timeScale().visibleLogicalRange();
    return {
        { QStringLiteral("from"), range.from },
        { QStringLiteral("to"), range.to },
    };
}

void FinancialChartItem::setVisibleLogicalRange(qreal from, qreal to)
{
    if (!std::isfinite(from) || !std::isfinite(to)) {
        return;
    }
    currentLayoutWithSyncedTimeScale();
    m_model.timeScale().setVisibleLogicalRange({ from, to });
    m_fitContentRequested = false;
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    Q_EMIT rightOffsetChanged();
    emitVisibleRangeSignals();
}

QVariantMap FinancialChartItem::visibleTimeRange() const
{
    if (m_model.timePoints().isEmpty()) {
        return { };
    }

    currentLayoutWithSyncedTimeScale();
    const auto [from, to] = m_model.timeScale().visibleIndexRange(0);
    if (from > to) {
        return { };
    }

    return {
        { QStringLiteral("from"), m_model.timeAt(from) },
        { QStringLiteral("to"), m_model.timeAt(to) },
    };
}

void FinancialChartItem::setVisibleTimeRange(qint64 from, qint64 to)
{
    const QVector<TimePoint>& times = m_model.timePoints();
    if (times.isEmpty() || from > to) {
        return;
    }

    const auto fromIt = std::lower_bound(times.cbegin(), times.cend(), from);
    const auto toIt = std::upper_bound(times.cbegin(), times.cend(), to);
    if (fromIt == times.cend() || toIt == times.cbegin() || fromIt >= toIt) {
        return;
    }

    const int fromIndex = static_cast<int>(std::distance(times.cbegin(), fromIt));
    const int toIndex = static_cast<int>(std::distance(times.cbegin(), std::prev(toIt)));
    setVisibleLogicalRange(fromIndex, toIndex);
}

qreal FinancialChartItem::logicalToCoordinate(qreal logicalIndex) const
{
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty()) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }
    return layout.panes.first().dataRect.left() + m_model.timeScale().indexToCoordinate(logicalIndex);
}

qreal FinancialChartItem::coordinateToLogical(qreal x) const
{
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty()) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }
    return m_model.timeScale().coordinateToLogical(x - layout.panes.first().dataRect.left());
}

qint64 FinancialChartItem::coordinateToTime(qreal x) const
{
    if (m_model.timePoints().isEmpty()) {
        return 0;
    }

    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty()) {
        return 0;
    }

    const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
    const int index = std::clamp(m_model.timeScale().coordinateToIndex(x - layout.panes.first().dataRect.left()), 0, lastIndex);
    return m_model.timeAt(index);
}

qreal FinancialChartItem::timeToCoordinate(qint64 time) const
{
    const int index = m_model.indexForTime(time);
    if (index < 0) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }
    return logicalToCoordinate(index);
}

int FinancialChartItem::timeToIndex(qint64 time) const
{
    return m_model.indexForTime(time);
}

qint64 FinancialChartItem::indexToTime(int index) const
{
    return timeAtIndex(index).value_or(0);
}

qreal FinancialChartItem::priceToCoordinate(int pane, double price) const
{
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    for (const ChartLayout::Pane& paneLayout : layout.panes) {
        if (paneLayout.index != pane) {
            continue;
        }
        const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
        PriceScale priceScale = m_priceScales.value(priceScaleKey(pane, QStringLiteral("right")));
        configurePriceScale(priceScale, pane, QStringLiteral("right"), visibleFrom, visibleTo);
        return priceScale.priceToCoordinate(price, paneLayout.dataRect);
    }
    return std::numeric_limits<qreal>::quiet_NaN();
}

void FinancialChartItem::setCrosshairPosition(qint64 time, double price, int paneIndex, const QString& priceScaleId)
{
    if (!std::isfinite(price) || m_model.timePoints().isEmpty()) {
        clearCrosshairPosition();
        return;
    }

    const int index = m_model.indexForTime(time);
    if (index < 0) {
        clearCrosshairPosition();
        return;
    }

    ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty()) {
        clearCrosshairPosition();
        return;
    }

    const ChartLayout::Pane* pane = nullptr;
    for (const ChartLayout::Pane& candidate : layout.panes) {
        if (candidate.index == paneIndex) {
            pane = &candidate;
            break;
        }
    }
    if (!pane) {
        clearCrosshairPosition();
        return;
    }

    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
    QString resolvedPriceScaleId = normalizedPriceScaleId(priceScaleId);
    if (priceScaleId.trimmed().isEmpty()) {
        QHash<QString, int> scaleUseCounts;
        QString matchedScaleId;
        for (const ChartSeries* series : m_series) {
            if (!series || !series->isVisible() || series->pane() != paneIndex) {
                continue;
            }

            const QString seriesScaleId = normalizedPriceScaleId(series->priceScaleId());
            ++scaleUseCounts[seriesScaleId];
            for (double candidatePrice : crosshairCandidatePrices(series, time, CrosshairMode::MagnetOHLC)) {
                if (std::isfinite(candidatePrice) && std::abs(candidatePrice - price) <= 1e-9) {
                    matchedScaleId = seriesScaleId;
                    break;
                }
            }
            if (!matchedScaleId.isEmpty()) {
                break;
            }
        }

        if (!matchedScaleId.isEmpty()) {
            resolvedPriceScaleId = matchedScaleId;
        } else if (scaleUseCounts.size() == 1) {
            resolvedPriceScaleId = scaleUseCounts.constBegin().key();
        } else {
            resolvedPriceScaleId = !m_priceScaleVisible && m_leftPriceScaleVisible
                ? QStringLiteral("left")
                : QStringLiteral("right");
        }
    }

    m_crosshairPriceScaleId = resolvedPriceScaleId;
    PriceScale& priceScale = m_priceScales[priceScaleKey(paneIndex, resolvedPriceScaleId)];
    configurePriceScale(priceScale, paneIndex, resolvedPriceScaleId, visibleFrom, visibleTo);

    const qreal y = priceScale.priceToCoordinate(price, pane->dataRect);
    if (!std::isfinite(y)) {
        clearCrosshairPosition();
        return;
    }

    m_crosshairPosition = QPointF(pane->dataRect.left() + m_model.timeScale().indexToCoordinate(index), y);
    m_crosshairVisible = true;
    scheduleVisualUpdate();
    Q_EMIT crosshairMoved(crosshairPayload(m_crosshairPosition, { }, m_crosshairPriceScaleId));
}

void FinancialChartItem::clearCrosshairPosition()
{
    if (!m_crosshairVisible) {
        return;
    }
    m_crosshairVisible = false;
    m_crosshairPriceScaleId = QStringLiteral("right");
    scheduleVisualUpdate();
    Q_EMIT crosshairMoved(QVariantMap());
}

qreal FinancialChartItem::scrollPosition() const
{
    return m_model.timeScale().rightOffset();
}

void FinancialChartItem::scrollToPosition(qreal position)
{
    setRightOffset(position);
}

void FinancialChartItem::scrollToRealTime()
{
    setRightOffset(0.0);
}

qreal FinancialChartItem::paneHeight(int pane) const
{
    return paneSize(pane).value(QStringLiteral("height")).toDouble();
}

void FinancialChartItem::setPaneHeight(int pane, qreal height)
{
    if (pane < 0) {
        return;
    }
    if (pane >= m_paneCount) {
        m_paneCount = pane + 1;
        Q_EMIT paneCountChanged();
    }
    m_model.setPaneHeight(pane, height);
    scheduleDataUpdate();
}

void FinancialChartItem::clearPaneHeight(int pane)
{
    setPaneHeight(pane, 0.0);
}

qreal FinancialChartItem::paneStretchFactor(int pane) const
{
    return m_model.paneStretchFactor(pane);
}

void FinancialChartItem::setPaneStretchFactor(int pane, qreal factor)
{
    if (pane < 0) {
        return;
    }
    if (pane >= m_paneCount) {
        m_paneCount = pane + 1;
        Q_EMIT paneCountChanged();
    }
    m_model.setPaneStretchFactor(pane, factor);
    scheduleDataUpdate();
}

QVariantMap FinancialChartItem::paneSize(int pane) const
{
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    for (const ChartLayout::Pane& paneLayout : layout.panes) {
        if (paneLayout.index != pane) {
            continue;
        }
        return {
            { QStringLiteral("pane"), paneLayout.index },
            { QStringLiteral("x"), paneLayout.dataRect.x() },
            { QStringLiteral("y"), paneLayout.dataRect.y() },
            { QStringLiteral("width"), paneLayout.dataRect.width() },
            { QStringLiteral("height"), paneLayout.dataRect.height() },
            { QStringLiteral("dataWidth"), paneLayout.dataRect.width() },
            { QStringLiteral("dataHeight"), paneLayout.dataRect.height() },
            { QStringLiteral("leftPriceAxisWidth"), paneLayout.leftPriceAxisRect.width() },
            { QStringLiteral("leftPriceAxisHeight"), paneLayout.leftPriceAxisRect.height() },
            { QStringLiteral("rightPriceAxisWidth"), paneLayout.rightPriceAxisRect.width() },
            { QStringLiteral("rightPriceAxisHeight"), paneLayout.rightPriceAxisRect.height() },
            { QStringLiteral("priceAxisWidth"), paneLayout.priceAxisRect.width() },
            { QStringLiteral("priceAxisHeight"), paneLayout.priceAxisRect.height() },
        };
    }
    return { };
}

QVariantList FinancialChartItem::paneSizes() const
{
    QVariantList sizes;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    sizes.reserve(layout.panes.size());
    for (const ChartLayout::Pane& pane : layout.panes) {
        sizes.push_back(paneSize(pane.index));
    }
    return sizes;
}

QVariantList FinancialChartItem::panes() const
{
    QVariantList result;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    result.reserve(layout.panes.size());
    for (const ChartLayout::Pane& pane : layout.panes) {
        QVariantMap paneInfo = paneSize(pane.index);
        paneInfo.insert(QStringLiteral("seriesCount"), seriesInPane(pane.index).size());
        paneInfo.insert(QStringLiteral("stretchFactor"), paneStretchFactor(pane.index));
        paneInfo.insert(QStringLiteral("configuredHeight"), m_model.paneHeight(pane.index));
        result.push_back(paneInfo);
    }
    return result;
}

QVariantList FinancialChartItem::seriesInPane(int pane) const
{
    QVariantList result;
    if (pane < 0) {
        return result;
    }

    int visualIndex = 0;
    for (qsizetype i = 0; i < m_series.size(); ++i) {
        ChartSeries* series = m_series.at(i);
        if (!series || series->pane() != pane) {
            continue;
        }

        QString name = series->name();
        if (name.isEmpty()) {
            name = series->objectName();
        }
        if (name.isEmpty()) {
            name = QStringLiteral("series%1").arg(i);
        }

        result.push_back(QVariantMap {
            { QStringLiteral("series"), QVariant::fromValue(series) },
            { QStringLiteral("seriesName"), name },
            { QStringLiteral("title"), series->title() },
            { QStringLiteral("pane"), pane },
            { QStringLiteral("seriesIndex"), static_cast<int>(i) },
            { QStringLiteral("visualIndex"), visualIndex++ },
        });
    }
    return result;
}

void FinancialChartItem::moveSeriesToPane(ChartSeries* series, int pane, int visualIndex)
{
    if (!series || !m_series.contains(series) || pane < 0) {
        return;
    }

    if (pane >= m_paneCount) {
        m_paneCount = pane + 1;
        Q_EMIT paneCountChanged();
    }

    series->setPane(pane);
    if (visualIndex >= 0) {
        moveSeriesInPane(series, visualIndex);
    } else {
        scheduleDataUpdate();
    }
}

void FinancialChartItem::moveSeriesInPane(ChartSeries* series, int visualIndex)
{
    if (!series || !m_series.contains(series)) {
        return;
    }

    const int pane = series->pane();
    QVector<ChartSeries*> paneSeries;
    paneSeries.reserve(m_series.size());
    for (ChartSeries* candidate : m_series) {
        if (candidate && candidate->pane() == pane && candidate != series) {
            paneSeries.push_back(candidate);
        }
    }

    const int normalizedIndex = std::clamp(visualIndex, 0, static_cast<int>(paneSeries.size()));
    paneSeries.insert(normalizedIndex, series);

    int nextPaneSeries = 0;
    for (ChartSeries*& candidate : m_series) {
        if (candidate && candidate->pane() == pane) {
            candidate = paneSeries.at(nextPaneSeries++);
        }
    }
    scheduleDataUpdate();
}

int FinancialChartItem::seriesOrderInPane(ChartSeries* series) const
{
    if (!series) {
        return -1;
    }

    int visualIndex = 0;
    for (ChartSeries* candidate : m_series) {
        if (!candidate || candidate->pane() != series->pane()) {
            continue;
        }
        if (candidate == series) {
            return visualIndex;
        }
        ++visualIndex;
    }
    return -1;
}

QVariantMap FinancialChartItem::visiblePriceRange(int pane) const
{
    return visiblePriceRangeForScale(QStringLiteral("right"), pane);
}

QVariantMap FinancialChartItem::visiblePriceRangeForScale(const QString& priceScaleId, int pane) const
{
    if (pane < 0) {
        return { };
    }

    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty() || !layoutContainsPane(layout, pane)) {
        return { };
    }

    const QString normalizedId = normalizedPriceScaleId(priceScaleId);
    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
    bool manualRange = false;
    const PriceRange range = resolvedPriceRange(pane, normalizedId, visibleFrom, visibleTo, &manualRange);

    if (!range.isValid()) {
        return { };
    }
    return {
        { QStringLiteral("from"), range.min },
        { QStringLiteral("to"), range.max },
        { QStringLiteral("min"), range.min },
        { QStringLiteral("max"), range.max },
        { QStringLiteral("autoScale"), !manualRange },
        { QStringLiteral("pane"), pane },
        { QStringLiteral("priceScaleId"), normalizedId },
    };
}

void FinancialChartItem::setVisiblePriceRange(int pane, double minimum, double maximum)
{
    setVisiblePriceRangeForScale(QStringLiteral("right"), pane, minimum, maximum);
}

void FinancialChartItem::setVisiblePriceRangeForScale(const QString& priceScaleId, int pane, double minimum, double maximum)
{
    if (pane < 0) {
        return;
    }

    PriceRange range { minimum, maximum };
    if (!range.isValid()) {
        return;
    }
    m_manualPriceRanges.insert(priceScaleKey(pane, normalizedPriceScaleId(priceScaleId)), range);
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::clearVisiblePriceRange(int pane)
{
    clearVisiblePriceRangeForScale(QString(), pane);
}

void FinancialChartItem::clearVisiblePriceRangeForScale(const QString& priceScaleId, int pane)
{
    if (pane < 0) {
        if (priceScaleId.trimmed().isEmpty()) {
            m_manualPriceRanges.clear();
        } else {
            const QString normalizedId = normalizedPriceScaleId(priceScaleId);
            for (auto it = m_manualPriceRanges.begin(); it != m_manualPriceRanges.end();) {
                const QString id = it.key().section(QLatin1Char(':'), 1, -1);
                if (id == normalizedId) {
                    it = m_manualPriceRanges.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        if (priceScaleId.trimmed().isEmpty()) {
            const QString prefix = QString::number(pane) + QLatin1Char(':');
            for (auto it = m_manualPriceRanges.begin(); it != m_manualPriceRanges.end();) {
                if (it.key().startsWith(prefix)) {
                    it = m_manualPriceRanges.erase(it);
                } else {
                    ++it;
                }
            }
        } else {
            m_manualPriceRanges.remove(priceScaleKey(pane, normalizedPriceScaleId(priceScaleId)));
        }
    }
    if (m_manualPriceRanges.isEmpty()) {
        m_autoScale = true;
    }
    scheduleVisualUpdate();
    Q_EMIT axisOptionsChanged();
}

void FinancialChartItem::applyOptions(const QVariantMap& options)
{
    auto applyBool = [](const QVariantMap& map, const QString& key, const std::function<void(bool)>& setter) {
        if (map.contains(key)) {
            setter(map.value(key).toBool());
        }
    };
    auto applyInt = [](const QVariantMap& map, const QString& key, const std::function<void(int)>& setter) {
        if (map.contains(key)) {
            setter(map.value(key).toInt());
        }
    };
    auto applyReal = [](const QVariantMap& map, const QString& key, const std::function<void(qreal)>& setter) {
        if (map.contains(key)) {
            setter(map.value(key).toDouble());
        }
    };
    auto applyString = [](const QVariantMap& map, const QString& key, const std::function<void(const QString&)>& setter) {
        if (map.contains(key)) {
            setter(map.value(key).toString());
        }
    };
    auto applyColor = [](const QVariantMap& map, const QString& key, const std::function<void(const QColor&)>& setter) {
        if (map.contains(key)) {
            setter(colorFromOption(map.value(key)));
        }
    };
    auto applyFormatter = [](const QVariantMap& map, const QString& key, const std::function<void(const QJSValue&)>& setter) {
        if (!map.contains(key)) {
            return;
        }
        const QVariant value = map.value(key);
        if (value.canConvert<QJSValue>()) {
            setter(value.value<QJSValue>());
        }
    };

    applyInt(options, QStringLiteral("paneCount"), [this](int value) { setPaneCount(value); });
    applyBool(options, QStringLiteral("interactive"), [this](bool value) { setInteractive(value); });
    applyBool(options, QStringLiteral("gridVisible"), [this](bool value) { setGridVisible(value); });
    applyBool(options, QStringLiteral("verticalGridVisible"), [this](bool value) { setVerticalGridVisible(value); });
    applyBool(options, QStringLiteral("horizontalGridVisible"), [this](bool value) { setHorizontalGridVisible(value); });
    applyInt(options, QStringLiteral("gridLineStyle"), [this](int value) { setGridLineStyle(value); });
    applyInt(options, QStringLiteral("verticalGridLineStyle"), [this](int value) { setVerticalGridLineStyle(value); });
    applyInt(options, QStringLiteral("horizontalGridLineStyle"), [this](int value) { setHorizontalGridLineStyle(value); });
    applyBool(options, QStringLiteral("timeScaleVisible"), [this](bool value) { setTimeScaleVisible(value); });
    applyBool(options, QStringLiteral("priceScaleVisible"), [this](bool value) { setPriceScaleVisible(value); });
    applyBool(options, QStringLiteral("leftPriceScaleVisible"), [this](bool value) { setLeftPriceScaleVisible(value); });
    applyBool(options, QStringLiteral("priceAxisBorderVisible"), [this](bool value) { setPriceAxisBorderVisible(value); });
    applyBool(options, QStringLiteral("leftPriceAxisBorderVisible"), [this](bool value) { setLeftPriceAxisBorderVisible(value); });
    applyBool(options, QStringLiteral("timeAxisBorderVisible"), [this](bool value) { setTimeAxisBorderVisible(value); });
    applyBool(options, QStringLiteral("priceAxisTickMarksVisible"), [this](bool value) { setPriceAxisTickMarksVisible(value); });
    applyBool(options, QStringLiteral("leftPriceAxisTickMarksVisible"), [this](bool value) { setLeftPriceAxisTickMarksVisible(value); });
    applyBool(options, QStringLiteral("timeAxisTickMarksVisible"), [this](bool value) { setTimeAxisTickMarksVisible(value); });
    applyBool(options, QStringLiteral("timeVisible"), [this](bool value) { setTimeVisible(value); });
    applyBool(options, QStringLiteral("secondsVisible"), [this](bool value) { setSecondsVisible(value); });
    applyString(options, QStringLiteral("dateFormat"), [this](const QString& value) { setDateFormat(value); });
    applyString(options, QStringLiteral("timeFormat"), [this](const QString& value) { setTimeFormat(value); });
    applyString(options, QStringLiteral("timeWithSecondsFormat"), [this](const QString& value) { setTimeWithSecondsFormat(value); });
    applyReal(options, QStringLiteral("timeAxisMinimumHeight"), [this](qreal value) { setTimeAxisMinimumHeight(value); });
    applyInt(options, QStringLiteral("timeAxisMaxLabelWidth"), [this](int value) { setTimeAxisMaxLabelWidth(value); });
    applyBool(options, QStringLiteral("timeAxisUniformDistribution"), [this](bool value) { setTimeAxisUniformDistribution(value); });
    applyBool(options, QStringLiteral("timeAxisBoldLabels"), [this](bool value) { setTimeAxisBoldLabels(value); });
    applyReal(options, QStringLiteral("priceAxisMinimumWidth"), [this](qreal value) { setPriceAxisMinimumWidth(value); });
    applyReal(options, QStringLiteral("leftPriceAxisMinimumWidth"), [this](qreal value) { setLeftPriceAxisMinimumWidth(value); });
    applyColor(options, QStringLiteral("priceAxisTextColor"), [this](const QColor& value) { setPriceAxisTextColor(value); });
    applyString(options, QStringLiteral("priceAxisLabelAlignment"), [this](const QString& value) { setPriceAxisLabelAlignment(value); });
    applyReal(options, QStringLiteral("priceScaleTopMargin"), [this](qreal value) { setPriceScaleTopMargin(value); });
    applyReal(options, QStringLiteral("priceScaleBottomMargin"), [this](qreal value) { setPriceScaleBottomMargin(value); });
    applyReal(options, QStringLiteral("priceTickSpacing"), [this](qreal value) { setPriceTickSpacing(value); });
    applyBool(options, QStringLiteral("autoScale"), [this](bool value) { setAutoScale(value); });
    applyBool(options, QStringLiteral("priceScaleInverted"), [this](bool value) { setPriceScaleInverted(value); });
    applyString(options, QStringLiteral("priceScaleMode"), [this](const QString& value) { setPriceScaleMode(value); });
    applyBool(options, QStringLiteral("preserveEmptyPanes"), [this](bool value) { setPreserveEmptyPanes(value); });
    applyBool(options, QStringLiteral("paneSeparatorsResizable"), [this](bool value) { setPaneSeparatorsResizable(value); });
    applyString(options, QStringLiteral("localeName"), [this](const QString& value) { setLocaleName(value); });
    applyInt(options, QStringLiteral("pricePrecision"), [this](int value) { setPricePrecision(value); });
    applyString(options, QStringLiteral("priceFormat"), [this](const QString& value) { setPriceFormat(value); });
    applyFormatter(options, QStringLiteral("timeFormatter"), [this](const QJSValue& value) { setTimeFormatter(value); });
    applyFormatter(options, QStringLiteral("timeTickMarkFormatter"), [this](const QJSValue& value) { setTimeTickMarkFormatter(value); });
    applyFormatter(options, QStringLiteral("priceFormatter"), [this](const QJSValue& value) { setPriceFormatter(value); });
    applyFormatter(options, QStringLiteral("percentageFormatter"), [this](const QJSValue& value) { setPercentageFormatter(value); });
    applyFormatter(options, QStringLiteral("priceTickMarkFormatter"), [this](const QJSValue& value) { setPriceTickMarkFormatter(value); });
    applyFormatter(options, QStringLiteral("percentageTickMarkFormatter"), [this](const QJSValue& value) { setPercentageTickMarkFormatter(value); });
    applyReal(options, QStringLiteral("barSpacing"), [this](qreal value) { setBarSpacing(value); });
    applyReal(options, QStringLiteral("rightOffset"), [this](qreal value) { setRightOffset(value); });
    applyReal(options, QStringLiteral("rightOffsetPixels"), [this](qreal value) { setRightOffsetPixels(value); });
    applyReal(options, QStringLiteral("minBarSpacing"), [this](qreal value) { setMinBarSpacing(value); });
    applyReal(options, QStringLiteral("maxBarSpacing"), [this](qreal value) { setMaxBarSpacing(value); });
    applyBool(options, QStringLiteral("fixLeftEdge"), [this](bool value) { setFixLeftEdge(value); });
    applyBool(options, QStringLiteral("fixRightEdge"), [this](bool value) { setFixRightEdge(value); });
    applyBool(options, QStringLiteral("lockVisibleTimeRangeOnResize"), [this](bool value) { setLockVisibleTimeRangeOnResize(value); });
    applyBool(options, QStringLiteral("rightBarStaysOnScroll"), [this](bool value) { setRightBarStaysOnScroll(value); });
    applyBool(options, QStringLiteral("shiftVisibleRangeOnNewBar"), [this](bool value) { setShiftVisibleRangeOnNewBar(value); });
    applyBool(options, QStringLiteral("horizontalTouchDrag"), [this](bool value) { setHorizontalTouchDrag(value); });
    applyBool(options, QStringLiteral("verticalTouchDrag"), [this](bool value) { setVerticalTouchDrag(value); });
    if (options.contains(QStringLiteral("crosshairMode"))) {
        setCrosshairMode(crosshairModeFromVariant(options.value(QStringLiteral("crosshairMode"))));
    }
    applyBool(options, QStringLiteral("verticalCrosshairLineVisible"), [this](bool value) { setVerticalCrosshairLineVisible(value); });
    applyBool(options, QStringLiteral("horizontalCrosshairLineVisible"), [this](bool value) { setHorizontalCrosshairLineVisible(value); });
    applyColor(options, QStringLiteral("verticalCrosshairLineColor"), [this](const QColor& value) { setVerticalCrosshairLineColor(value); });
    applyColor(options, QStringLiteral("horizontalCrosshairLineColor"), [this](const QColor& value) { setHorizontalCrosshairLineColor(value); });
    applyReal(options, QStringLiteral("verticalCrosshairLineWidth"), [this](qreal value) { setVerticalCrosshairLineWidth(value); });
    applyReal(options, QStringLiteral("horizontalCrosshairLineWidth"), [this](qreal value) { setHorizontalCrosshairLineWidth(value); });
    applyInt(options, QStringLiteral("verticalCrosshairLineStyle"), [this](int value) { setVerticalCrosshairLineStyle(value); });
    applyInt(options, QStringLiteral("horizontalCrosshairLineStyle"), [this](int value) { setHorizontalCrosshairLineStyle(value); });
    applyBool(options, QStringLiteral("timeCrosshairLabelVisible"), [this](bool value) { setTimeCrosshairLabelVisible(value); });
    applyBool(options, QStringLiteral("priceCrosshairLabelVisible"), [this](bool value) { setPriceCrosshairLabelVisible(value); });
    applyColor(options, QStringLiteral("timeCrosshairLabelBackground"), [this](const QColor& value) { setTimeCrosshairLabelBackground(value); });
    applyColor(options, QStringLiteral("priceCrosshairLabelBackground"), [this](const QColor& value) { setPriceCrosshairLabelBackground(value); });
    applyBool(options, QStringLiteral("snapCrosshairToHiddenSeries"), [this](bool value) { setSnapCrosshairToHiddenSeries(value); });

    const QVariant handleScrollValue = options.value(QStringLiteral("handleScroll"));
    if (handleScrollValue.isValid()) {
        const QVariantMap map = handleScrollValue.toMap();
        if (map.isEmpty() && handleScrollValue.canConvert<bool>()) {
            setHandleScroll(handleScrollValue.toBool());
        } else {
            applyBool(map, QStringLiteral("enabled"), [this](bool value) { setHandleScroll(value); });
            applyBool(map, QStringLiteral("mouseWheel"), [this](bool value) { setMouseWheelScroll(value); });
            applyBool(map, QStringLiteral("pressedMouseMove"), [this](bool value) { setPressedMouseMoveScroll(value); });
            applyBool(map, QStringLiteral("horzTouchDrag"), [this](bool value) { setHorizontalTouchDrag(value); });
            applyBool(map, QStringLiteral("horizontalTouchDrag"), [this](bool value) { setHorizontalTouchDrag(value); });
            applyBool(map, QStringLiteral("vertTouchDrag"), [this](bool value) { setVerticalTouchDrag(value); });
            applyBool(map, QStringLiteral("verticalTouchDrag"), [this](bool value) { setVerticalTouchDrag(value); });
        }
    }

    const QVariant handleScaleValue = options.value(QStringLiteral("handleScale"));
    if (handleScaleValue.isValid()) {
        const QVariantMap map = handleScaleValue.toMap();
        if (map.isEmpty() && handleScaleValue.canConvert<bool>()) {
            setHandleScale(handleScaleValue.toBool());
        } else {
            applyBool(map, QStringLiteral("enabled"), [this](bool value) { setHandleScale(value); });
            applyBool(map, QStringLiteral("mouseWheel"), [this](bool value) { setMouseWheelScale(value); });
            applyBool(map, QStringLiteral("pinch"), [this](bool value) { setPinchScale(value); });
        }
    }

    const QVariantMap grid = options.value(QStringLiteral("grid")).toMap();
    if (!grid.isEmpty()) {
        applyBool(grid, QStringLiteral("visible"), [this](bool value) { setGridVisible(value); });
        applyBool(grid, QStringLiteral("verticalVisible"), [this](bool value) { setVerticalGridVisible(value); });
        applyBool(grid, QStringLiteral("horizontalVisible"), [this](bool value) { setHorizontalGridVisible(value); });
        applyInt(grid, QStringLiteral("lineStyle"), [this](int value) { setGridLineStyle(value); });
        applyInt(grid, QStringLiteral("verticalLineStyle"), [this](int value) { setVerticalGridLineStyle(value); });
        applyInt(grid, QStringLiteral("horizontalLineStyle"), [this](int value) { setHorizontalGridLineStyle(value); });
    }

    const QVariantMap timeScale = options.value(QStringLiteral("timeScale")).toMap();
    if (!timeScale.isEmpty()) {
        applyBool(timeScale, QStringLiteral("visible"), [this](bool value) { setTimeScaleVisible(value); });
        applyBool(timeScale, QStringLiteral("borderVisible"), [this](bool value) { setTimeAxisBorderVisible(value); });
        applyBool(timeScale, QStringLiteral("tickMarksVisible"), [this](bool value) { setTimeAxisTickMarksVisible(value); });
        applyReal(timeScale, QStringLiteral("barSpacing"), [this](qreal value) { setBarSpacing(value); });
        applyReal(timeScale, QStringLiteral("rightOffset"), [this](qreal value) { setRightOffset(value); });
        applyReal(timeScale, QStringLiteral("rightOffsetPixels"), [this](qreal value) { setRightOffsetPixels(value); });
        applyReal(timeScale, QStringLiteral("minBarSpacing"), [this](qreal value) { setMinBarSpacing(value); });
        applyReal(timeScale, QStringLiteral("maxBarSpacing"), [this](qreal value) { setMaxBarSpacing(value); });
        applyBool(timeScale, QStringLiteral("fixLeftEdge"), [this](bool value) { setFixLeftEdge(value); });
        applyBool(timeScale, QStringLiteral("fixRightEdge"), [this](bool value) { setFixRightEdge(value); });
        applyBool(timeScale, QStringLiteral("lockVisibleTimeRangeOnResize"), [this](bool value) { setLockVisibleTimeRangeOnResize(value); });
        applyBool(timeScale, QStringLiteral("rightBarStaysOnScroll"), [this](bool value) { setRightBarStaysOnScroll(value); });
        applyBool(timeScale, QStringLiteral("shiftVisibleRangeOnNewBar"), [this](bool value) { setShiftVisibleRangeOnNewBar(value); });
        applyReal(timeScale, QStringLiteral("minimumHeight"), [this](qreal value) { setTimeAxisMinimumHeight(value); });
        applyInt(timeScale, QStringLiteral("maxLabelWidth"), [this](int value) { setTimeAxisMaxLabelWidth(value); });
        applyBool(timeScale, QStringLiteral("uniformDistribution"), [this](bool value) { setTimeAxisUniformDistribution(value); });
        applyBool(timeScale, QStringLiteral("boldLabels"), [this](bool value) { setTimeAxisBoldLabels(value); });
        applyFormatter(timeScale, QStringLiteral("tickMarkFormatter"), [this](const QJSValue& value) { setTimeTickMarkFormatter(value); });
    }

    const QVariantMap priceScale = options.value(QStringLiteral("rightPriceScale")).toMap();
    if (!priceScale.isEmpty()) {
        applyBool(priceScale, QStringLiteral("visible"), [this](bool value) { setPriceScaleVisible(value); });
        applyBool(priceScale, QStringLiteral("borderVisible"), [this](bool value) { setPriceAxisBorderVisible(value); });
        applyBool(priceScale, QStringLiteral("tickMarksVisible"), [this](bool value) { setPriceAxisTickMarksVisible(value); });
        applyReal(priceScale, QStringLiteral("minimumWidth"), [this](qreal value) { setPriceAxisMinimumWidth(value); });
        applyColor(priceScale, QStringLiteral("textColor"), [this](const QColor& value) { setPriceAxisTextColor(value); });
        applyString(priceScale, QStringLiteral("labelAlignment"), [this](const QString& value) { setPriceAxisLabelAlignment(value); });
        applyReal(priceScale, QStringLiteral("topMargin"), [this](qreal value) { setPriceScaleTopMargin(value); });
        applyReal(priceScale, QStringLiteral("bottomMargin"), [this](qreal value) { setPriceScaleBottomMargin(value); });
        applyReal(priceScale, QStringLiteral("tickSpacing"), [this](qreal value) { setPriceTickSpacing(value); });
        applyBool(priceScale, QStringLiteral("autoScale"), [this](bool value) {
            setAutoScaleForScale(QStringLiteral("right"), value);
        });
        applyBool(priceScale, QStringLiteral("invertScale"), [this](bool value) { setPriceScaleInverted(value); });
        applyBool(priceScale, QStringLiteral("inverted"), [this](bool value) { setPriceScaleInverted(value); });
        applyString(priceScale, QStringLiteral("mode"), [this](const QString& value) { setPriceScaleMode(value); });
        applyString(priceScale, QStringLiteral("scaleMode"), [this](const QString& value) { setPriceScaleMode(value); });
    }

    const QVariantMap leftPriceScale = options.value(QStringLiteral("leftPriceScale")).toMap();
    if (!leftPriceScale.isEmpty()) {
        applyBool(leftPriceScale, QStringLiteral("visible"), [this](bool value) { setLeftPriceScaleVisible(value); });
        applyBool(leftPriceScale, QStringLiteral("borderVisible"), [this](bool value) { setLeftPriceAxisBorderVisible(value); });
        applyBool(leftPriceScale, QStringLiteral("tickMarksVisible"), [this](bool value) { setLeftPriceAxisTickMarksVisible(value); });
        applyReal(leftPriceScale, QStringLiteral("minimumWidth"), [this](qreal value) { setLeftPriceAxisMinimumWidth(value); });
        applyColor(leftPriceScale, QStringLiteral("textColor"), [this](const QColor& value) { setPriceAxisTextColor(value); });
        applyString(leftPriceScale, QStringLiteral("labelAlignment"), [this](const QString& value) { setPriceAxisLabelAlignment(value); });
        applyReal(leftPriceScale, QStringLiteral("topMargin"), [this](qreal value) { setPriceScaleTopMargin(value); });
        applyReal(leftPriceScale, QStringLiteral("bottomMargin"), [this](qreal value) { setPriceScaleBottomMargin(value); });
        applyReal(leftPriceScale, QStringLiteral("tickSpacing"), [this](qreal value) { setPriceTickSpacing(value); });
        applyBool(leftPriceScale, QStringLiteral("autoScale"), [this](bool value) {
            setAutoScaleForScale(QStringLiteral("left"), value);
        });
        applyBool(leftPriceScale, QStringLiteral("invertScale"), [this](bool value) { setPriceScaleInverted(value); });
        applyBool(leftPriceScale, QStringLiteral("inverted"), [this](bool value) { setPriceScaleInverted(value); });
        applyString(leftPriceScale, QStringLiteral("mode"), [this](const QString& value) { setPriceScaleMode(value); });
        applyString(leftPriceScale, QStringLiteral("scaleMode"), [this](const QString& value) { setPriceScaleMode(value); });
    }

    const QVariantMap panes = options.value(QStringLiteral("panes")).toMap();
    if (!panes.isEmpty()) {
        applyBool(panes, QStringLiteral("preserveEmptyPanes"), [this](bool value) { setPreserveEmptyPanes(value); });
        applyBool(panes, QStringLiteral("separatorResizable"), [this](bool value) { setPaneSeparatorsResizable(value); });
        applyBool(panes, QStringLiteral("separatorsResizable"), [this](bool value) { setPaneSeparatorsResizable(value); });
    }

    const QVariantMap crosshair = options.value(QStringLiteral("crosshair")).toMap();
    if (!crosshair.isEmpty()) {
        if (crosshair.contains(QStringLiteral("mode"))) {
            setCrosshairMode(crosshairModeFromVariant(crosshair.value(QStringLiteral("mode"))));
        }

        const QVariantMap vertLine = crosshair.value(QStringLiteral("vertLine")).toMap();
        if (!vertLine.isEmpty()) {
            applyBool(vertLine, QStringLiteral("visible"), [this](bool value) { setVerticalCrosshairLineVisible(value); });
            applyColor(vertLine, QStringLiteral("color"), [this](const QColor& value) { setVerticalCrosshairLineColor(value); });
            applyReal(vertLine, QStringLiteral("width"), [this](qreal value) { setVerticalCrosshairLineWidth(value); });
            applyInt(vertLine, QStringLiteral("style"), [this](int value) { setVerticalCrosshairLineStyle(value); });
            applyBool(vertLine, QStringLiteral("labelVisible"), [this](bool value) { setTimeCrosshairLabelVisible(value); });
            applyColor(vertLine, QStringLiteral("labelBackgroundColor"), [this](const QColor& value) { setTimeCrosshairLabelBackground(value); });
        }

        const QVariantMap horzLine = crosshair.value(QStringLiteral("horzLine")).toMap();
        if (!horzLine.isEmpty()) {
            applyBool(horzLine, QStringLiteral("visible"), [this](bool value) { setHorizontalCrosshairLineVisible(value); });
            applyColor(horzLine, QStringLiteral("color"), [this](const QColor& value) { setHorizontalCrosshairLineColor(value); });
            applyReal(horzLine, QStringLiteral("width"), [this](qreal value) { setHorizontalCrosshairLineWidth(value); });
            applyInt(horzLine, QStringLiteral("style"), [this](int value) { setHorizontalCrosshairLineStyle(value); });
            applyBool(horzLine, QStringLiteral("labelVisible"), [this](bool value) { setPriceCrosshairLabelVisible(value); });
            applyColor(horzLine, QStringLiteral("labelBackgroundColor"), [this](const QColor& value) { setPriceCrosshairLabelBackground(value); });
        }
        applyBool(crosshair, QStringLiteral("snapToHiddenSeries"), [this](bool value) { setSnapCrosshairToHiddenSeries(value); });
        applyBool(crosshair, QStringLiteral("snapCrosshairToHiddenSeries"), [this](bool value) { setSnapCrosshairToHiddenSeries(value); });
    }

    const QVariantMap localization = options.value(QStringLiteral("localization")).toMap();
    if (!localization.isEmpty()) {
        applyString(localization, QStringLiteral("localeName"), [this](const QString& value) { setLocaleName(value); });
        applyInt(localization, QStringLiteral("pricePrecision"), [this](int value) { setPricePrecision(value); });
        applyString(localization, QStringLiteral("priceFormat"), [this](const QString& value) { setPriceFormat(value); });
        applyBool(localization, QStringLiteral("timeVisible"), [this](bool value) { setTimeVisible(value); });
        applyBool(localization, QStringLiteral("secondsVisible"), [this](bool value) { setSecondsVisible(value); });
        applyString(localization, QStringLiteral("dateFormat"), [this](const QString& value) { setDateFormat(value); });
        applyString(localization, QStringLiteral("timeFormat"), [this](const QString& value) { setTimeFormat(value); });
        applyString(localization, QStringLiteral("timeWithSecondsFormat"), [this](const QString& value) { setTimeWithSecondsFormat(value); });
        applyFormatter(localization, QStringLiteral("timeFormatter"), [this](const QJSValue& value) { setTimeFormatter(value); });
        applyFormatter(localization, QStringLiteral("priceFormatter"), [this](const QJSValue& value) { setPriceFormatter(value); });
        applyFormatter(localization, QStringLiteral("percentageFormatter"), [this](const QJSValue& value) { setPercentageFormatter(value); });
        applyFormatter(localization, QStringLiteral("priceTickMarkFormatter"), [this](const QJSValue& value) { setPriceTickMarkFormatter(value); });
        applyFormatter(localization, QStringLiteral("percentageTickMarkFormatter"), [this](const QJSValue& value) { setPercentageTickMarkFormatter(value); });
    }
}

QVariantMap FinancialChartItem::options() const
{
    const bool rightAutoScale = m_autoScale && !hasManualPriceRangeForScale(QStringLiteral("right"));
    const bool leftAutoScale = m_autoScale && !hasManualPriceRangeForScale(QStringLiteral("left"));

    return {
        { QStringLiteral("paneCount"), m_paneCount },
        { QStringLiteral("interactive"), m_interactive },
        { QStringLiteral("gridVisible"), m_gridVisible },
        { QStringLiteral("verticalGridVisible"), m_verticalGridVisible },
        { QStringLiteral("horizontalGridVisible"), m_horizontalGridVisible },
        { QStringLiteral("gridLineStyle"), gridLineStyle() },
        { QStringLiteral("verticalGridLineStyle"), m_verticalGridLineStyle },
        { QStringLiteral("horizontalGridLineStyle"), m_horizontalGridLineStyle },
        { QStringLiteral("timeScaleVisible"), m_timeScaleVisible },
        { QStringLiteral("priceScaleVisible"), m_priceScaleVisible },
        { QStringLiteral("leftPriceScaleVisible"), m_leftPriceScaleVisible },
        { QStringLiteral("priceAxisBorderVisible"), m_priceAxisBorderVisible },
        { QStringLiteral("leftPriceAxisBorderVisible"), m_leftPriceAxisBorderVisible },
        { QStringLiteral("timeAxisBorderVisible"), m_timeAxisBorderVisible },
        { QStringLiteral("priceAxisTickMarksVisible"), m_priceAxisTickMarksVisible },
        { QStringLiteral("leftPriceAxisTickMarksVisible"), m_leftPriceAxisTickMarksVisible },
        { QStringLiteral("timeAxisTickMarksVisible"), m_timeAxisTickMarksVisible },
        { QStringLiteral("timeVisible"), m_timeVisible },
        { QStringLiteral("secondsVisible"), m_secondsVisible },
        { QStringLiteral("dateFormat"), m_dateFormat },
        { QStringLiteral("timeFormat"), m_timeFormat },
        { QStringLiteral("timeWithSecondsFormat"), m_timeWithSecondsFormat },
        { QStringLiteral("timeAxisMinimumHeight"), m_timeAxisMinimumHeight },
        { QStringLiteral("timeAxisMaxLabelWidth"), m_timeAxisMaxLabelWidth },
        { QStringLiteral("timeAxisUniformDistribution"), m_timeAxisUniformDistribution },
        { QStringLiteral("timeAxisBoldLabels"), m_timeAxisBoldLabels },
        { QStringLiteral("priceAxisMinimumWidth"), m_priceAxisMinimumWidth },
        { QStringLiteral("leftPriceAxisMinimumWidth"), m_leftPriceAxisMinimumWidth },
        { QStringLiteral("priceAxisTextColor"), m_priceAxisTextColor },
        { QStringLiteral("priceAxisLabelAlignment"), m_priceAxisLabelAlignment },
        { QStringLiteral("priceScaleTopMargin"), m_priceScaleTopMargin },
        { QStringLiteral("priceScaleBottomMargin"), m_priceScaleBottomMargin },
        { QStringLiteral("priceTickSpacing"), m_priceTickSpacing },
        { QStringLiteral("autoScale"), m_autoScale },
        { QStringLiteral("priceScaleInverted"), m_priceScaleInverted },
        { QStringLiteral("priceScaleMode"), priceScaleMode() },
        { QStringLiteral("preserveEmptyPanes"), m_preserveEmptyPanes },
        { QStringLiteral("paneSeparatorsResizable"), m_paneSeparatorsResizable },
        { QStringLiteral("localeName"), localeName() },
        { QStringLiteral("pricePrecision"), m_pricePrecision },
        { QStringLiteral("priceFormat"), priceFormat() },
        { QStringLiteral("timeFormatter"), QVariant::fromValue(m_timeFormatter) },
        { QStringLiteral("timeTickMarkFormatter"), QVariant::fromValue(m_timeTickMarkFormatter) },
        { QStringLiteral("priceFormatter"), QVariant::fromValue(m_priceFormatter) },
        { QStringLiteral("percentageFormatter"), QVariant::fromValue(m_percentageFormatter) },
        { QStringLiteral("priceTickMarkFormatter"), QVariant::fromValue(m_priceTickMarkFormatter) },
        { QStringLiteral("percentageTickMarkFormatter"), QVariant::fromValue(m_percentageTickMarkFormatter) },
        { QStringLiteral("barSpacing"), barSpacing() },
        { QStringLiteral("rightOffset"), rightOffset() },
        { QStringLiteral("rightOffsetPixels"), rightOffsetPixels() },
        { QStringLiteral("minBarSpacing"), minBarSpacing() },
        { QStringLiteral("maxBarSpacing"), maxBarSpacing() },
        { QStringLiteral("fixLeftEdge"), fixLeftEdge() },
        { QStringLiteral("fixRightEdge"), fixRightEdge() },
        { QStringLiteral("lockVisibleTimeRangeOnResize"), lockVisibleTimeRangeOnResize() },
        { QStringLiteral("rightBarStaysOnScroll"), rightBarStaysOnScroll() },
        { QStringLiteral("shiftVisibleRangeOnNewBar"), shiftVisibleRangeOnNewBar() },
        { QStringLiteral("horizontalTouchDrag"), m_horizontalTouchDrag },
        { QStringLiteral("verticalTouchDrag"), m_verticalTouchDrag },
        { QStringLiteral("crosshairMode"), m_crosshairMode },
        { QStringLiteral("verticalCrosshairLineVisible"), m_verticalCrosshairLineVisible },
        { QStringLiteral("horizontalCrosshairLineVisible"), m_horizontalCrosshairLineVisible },
        { QStringLiteral("verticalCrosshairLineColor"), m_verticalCrosshairLineColor },
        { QStringLiteral("horizontalCrosshairLineColor"), m_horizontalCrosshairLineColor },
        { QStringLiteral("verticalCrosshairLineWidth"), m_verticalCrosshairLineWidth },
        { QStringLiteral("horizontalCrosshairLineWidth"), m_horizontalCrosshairLineWidth },
        { QStringLiteral("verticalCrosshairLineStyle"), m_verticalCrosshairLineStyle },
        { QStringLiteral("horizontalCrosshairLineStyle"), m_horizontalCrosshairLineStyle },
        { QStringLiteral("timeCrosshairLabelVisible"), m_timeCrosshairLabelVisible },
        { QStringLiteral("priceCrosshairLabelVisible"), m_priceCrosshairLabelVisible },
        { QStringLiteral("timeCrosshairLabelBackground"), m_timeCrosshairLabelBackground },
        { QStringLiteral("priceCrosshairLabelBackground"), m_priceCrosshairLabelBackground },
        { QStringLiteral("snapCrosshairToHiddenSeries"), m_snapCrosshairToHiddenSeries },
        { QStringLiteral("handleScroll"), QVariantMap {
                                              { QStringLiteral("enabled"), m_handleScroll },
                                              { QStringLiteral("mouseWheel"), m_mouseWheelScroll },
                                              { QStringLiteral("pressedMouseMove"), m_pressedMouseMoveScroll },
                                              { QStringLiteral("horzTouchDrag"), m_horizontalTouchDrag },
                                              { QStringLiteral("vertTouchDrag"), m_verticalTouchDrag },
                                          } },
        { QStringLiteral("handleScale"), QVariantMap {
                                             { QStringLiteral("enabled"), m_handleScale },
                                             { QStringLiteral("mouseWheel"), m_mouseWheelScale },
                                             { QStringLiteral("pinch"), m_pinchScale },
                                         } },
        { QStringLiteral("grid"), QVariantMap {
                                      { QStringLiteral("visible"), m_gridVisible },
                                      { QStringLiteral("verticalVisible"), m_verticalGridVisible },
                                      { QStringLiteral("horizontalVisible"), m_horizontalGridVisible },
                                      { QStringLiteral("lineStyle"), gridLineStyle() },
                                      { QStringLiteral("verticalLineStyle"), m_verticalGridLineStyle },
                                      { QStringLiteral("horizontalLineStyle"), m_horizontalGridLineStyle },
                                  } },
        { QStringLiteral("timeScale"), QVariantMap {
                                           { QStringLiteral("visible"), m_timeScaleVisible },
                                           { QStringLiteral("borderVisible"), m_timeAxisBorderVisible },
                                           { QStringLiteral("tickMarksVisible"), m_timeAxisTickMarksVisible },
                                           { QStringLiteral("barSpacing"), barSpacing() },
                                           { QStringLiteral("rightOffset"), rightOffset() },
                                           { QStringLiteral("rightOffsetPixels"), rightOffsetPixels() },
                                           { QStringLiteral("minBarSpacing"), minBarSpacing() },
                                           { QStringLiteral("maxBarSpacing"), maxBarSpacing() },
                                           { QStringLiteral("fixLeftEdge"), fixLeftEdge() },
                                           { QStringLiteral("fixRightEdge"), fixRightEdge() },
                                           { QStringLiteral("lockVisibleTimeRangeOnResize"), lockVisibleTimeRangeOnResize() },
                                           { QStringLiteral("rightBarStaysOnScroll"), rightBarStaysOnScroll() },
                                           { QStringLiteral("shiftVisibleRangeOnNewBar"), shiftVisibleRangeOnNewBar() },
                                           { QStringLiteral("minimumHeight"), m_timeAxisMinimumHeight },
                                           { QStringLiteral("maxLabelWidth"), m_timeAxisMaxLabelWidth },
                                           { QStringLiteral("uniformDistribution"), m_timeAxisUniformDistribution },
                                           { QStringLiteral("boldLabels"), m_timeAxisBoldLabels },
                                           { QStringLiteral("tickMarkFormatter"), QVariant::fromValue(m_timeTickMarkFormatter) },
                                       } },
        { QStringLiteral("rightPriceScale"), QVariantMap {
                                                 { QStringLiteral("visible"), m_priceScaleVisible },
                                                 { QStringLiteral("borderVisible"), m_priceAxisBorderVisible },
                                                 { QStringLiteral("tickMarksVisible"), m_priceAxisTickMarksVisible },
                                                 { QStringLiteral("minimumWidth"), m_priceAxisMinimumWidth },
                                                 { QStringLiteral("textColor"), m_priceAxisTextColor },
                                                 { QStringLiteral("labelAlignment"), m_priceAxisLabelAlignment },
                                                 { QStringLiteral("topMargin"), m_priceScaleTopMargin },
                                                 { QStringLiteral("bottomMargin"), m_priceScaleBottomMargin },
                                                 { QStringLiteral("tickSpacing"), m_priceTickSpacing },
                                                 { QStringLiteral("autoScale"), rightAutoScale },
                                                 { QStringLiteral("invertScale"), m_priceScaleInverted },
                                                 { QStringLiteral("mode"), priceScaleMode() },
                                             } },
        { QStringLiteral("leftPriceScale"), QVariantMap {
                                                { QStringLiteral("visible"), m_leftPriceScaleVisible },
                                                { QStringLiteral("borderVisible"), m_leftPriceAxisBorderVisible },
                                                { QStringLiteral("tickMarksVisible"), m_leftPriceAxisTickMarksVisible },
                                                { QStringLiteral("minimumWidth"), m_leftPriceAxisMinimumWidth },
                                                { QStringLiteral("textColor"), m_priceAxisTextColor },
                                                { QStringLiteral("labelAlignment"), m_priceAxisLabelAlignment },
                                                { QStringLiteral("topMargin"), m_priceScaleTopMargin },
                                                { QStringLiteral("bottomMargin"), m_priceScaleBottomMargin },
                                                { QStringLiteral("tickSpacing"), m_priceTickSpacing },
                                                { QStringLiteral("autoScale"), leftAutoScale },
                                                { QStringLiteral("invertScale"), m_priceScaleInverted },
                                                { QStringLiteral("mode"), priceScaleMode() },
                                            } },
        { QStringLiteral("panes"), QVariantMap {
                                       { QStringLiteral("preserveEmptyPanes"), m_preserveEmptyPanes },
                                       { QStringLiteral("separatorsResizable"), m_paneSeparatorsResizable },
                                   } },
        { QStringLiteral("crosshair"), QVariantMap {
                                           { QStringLiteral("mode"), m_crosshairMode },
                                           { QStringLiteral("snapToHiddenSeries"), m_snapCrosshairToHiddenSeries },
                                           { QStringLiteral("vertLine"), QVariantMap {
                                                                             { QStringLiteral("visible"), m_verticalCrosshairLineVisible },
                                                                             { QStringLiteral("color"), m_verticalCrosshairLineColor },
                                                                             { QStringLiteral("width"), m_verticalCrosshairLineWidth },
                                                                             { QStringLiteral("style"), m_verticalCrosshairLineStyle },
                                                                             { QStringLiteral("labelVisible"), m_timeCrosshairLabelVisible },
                                                                             { QStringLiteral("labelBackgroundColor"), m_timeCrosshairLabelBackground },
                                                                         } },
                                           { QStringLiteral("horzLine"), QVariantMap {
                                                                             { QStringLiteral("visible"), m_horizontalCrosshairLineVisible },
                                                                             { QStringLiteral("color"), m_horizontalCrosshairLineColor },
                                                                             { QStringLiteral("width"), m_horizontalCrosshairLineWidth },
                                                                             { QStringLiteral("style"), m_horizontalCrosshairLineStyle },
                                                                             { QStringLiteral("labelVisible"), m_priceCrosshairLabelVisible },
                                                                             { QStringLiteral("labelBackgroundColor"), m_priceCrosshairLabelBackground },
                                                                         } },
                                       } },
        { QStringLiteral("localization"), QVariantMap {
                                              { QStringLiteral("localeName"), localeName() },
                                              { QStringLiteral("pricePrecision"), m_pricePrecision },
                                              { QStringLiteral("priceFormat"), priceFormat() },
                                              { QStringLiteral("timeVisible"), m_timeVisible },
                                              { QStringLiteral("secondsVisible"), m_secondsVisible },
                                              { QStringLiteral("dateFormat"), m_dateFormat },
                                              { QStringLiteral("timeFormat"), m_timeFormat },
                                              { QStringLiteral("timeWithSecondsFormat"), m_timeWithSecondsFormat },
                                              { QStringLiteral("timeFormatter"), QVariant::fromValue(m_timeFormatter) },
                                              { QStringLiteral("priceFormatter"), QVariant::fromValue(m_priceFormatter) },
                                              { QStringLiteral("percentageFormatter"), QVariant::fromValue(m_percentageFormatter) },
                                              { QStringLiteral("priceTickMarkFormatter"), QVariant::fromValue(m_priceTickMarkFormatter) },
                                              { QStringLiteral("percentageTickMarkFormatter"), QVariant::fromValue(m_percentageTickMarkFormatter) },
                                          } },
    };
}

bool FinancialChartItem::saveScreenshot(const QString& fileName, const QSize& targetSize)
{
    if (m_removed || fileName.trimmed().isEmpty() || !window() || width() <= 0.0 || height() <= 0.0) {
        return false;
    }

    const QSharedPointer<QQuickItemGrabResult> grab = grabToImage(targetSize);
    if (grab.isNull()) {
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    bool ready = false;
    timeout.setSingleShot(true);
    QObject::connect(grab.get(), &QQuickItemGrabResult::ready, &loop, [&] {
        ready = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    if (!ready || grab->image().isNull()) {
        return false;
    }

    const QUrl url(fileName);
    const QString path = url.isLocalFile() ? url.toLocalFile() : fileName;
    const QFileInfo fileInfo(path);
    if (!fileInfo.absolutePath().isEmpty()) {
        QDir().mkpath(fileInfo.absolutePath());
    }
    return grab->image().save(path);
}

void FinancialChartItem::remove()
{
    if (m_removed) {
        return;
    }

    const QVector<ChartSeries*> ownedSeries = m_series;
    for (ChartSeries* series : ownedSeries) {
        detachSeries(series);
        if (series && series->parent() == this) {
            series->deleteLater();
        }
    }

    m_priceScales.clear();
    m_manualPriceRanges.clear();
    m_model.setSeries({ });
    m_model.rebuildTimeline();
    if (m_theme) {
        disconnect(m_theme, nullptr, this, nullptr);
    }
    m_crosshairVisible = false;
    m_crosshairPriceScaleId = QStringLiteral("right");
    m_hoveredPaneSeparator = -1;
    m_draggedPaneSeparator = -1;
    m_axisDragMode = AxisDragMode::None;
    m_axisDragPane = -1;
    m_axisDragPriceScaleId = QStringLiteral("right");
    m_dragging = false;
    m_dragMoved = false;
    m_touchDragging = false;
    m_touchDragPane = -1;
    m_removed = true;

    unsetCursor();
    setEnabled(false);
    setVisible(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setAcceptTouchEvents(false);
    update();

    Q_EMIT crosshairMoved(QVariantMap());
    Q_EMIT removedChanged();
}

QObject* FinancialChartItem::chartElement(const QString& name) const
{
    QString key = name.trimmed().toLower();
    key.remove(QLatin1Char('-'));
    key.remove(QLatin1Char('_'));
    key.remove(QLatin1Char(' '));

    if (key.isEmpty()
        || key == QStringLiteral("chart")
        || key == QStringLiteral("item")
        || key == QStringLiteral("nativeitem")
        || key == QStringLiteral("root")) {
        return const_cast<FinancialChartItem*>(this);
    }
    if (key == QStringLiteral("theme")) {
        return theme();
    }
    return nullptr;
}

void FinancialChartItem::scheduleDataUpdate()
{
    if (m_removed) {
        return;
    }
    rebuildModel();
    m_fitContentRequested = m_fitContentRequested || m_model.timePoints().size() <= 2;
    scheduleVisualUpdate();
    emitVisibleRangeSignals();
}

std::optional<TimePoint> FinancialChartItem::timeAtIndex(int index) const
{
    if (index < 0 || index >= m_model.timePoints().size()) {
        return std::nullopt;
    }
    return m_model.timeAt(index);
}

void FinancialChartItem::appendSeries(QQmlListProperty<ChartSeries>* property, ChartSeries* series)
{
    auto* chart = qobject_cast<FinancialChartItem*>(property->object);
    if (!chart || !series) {
        return;
    }
    chart->attachSeries(series);
}

qsizetype FinancialChartItem::seriesCount(QQmlListProperty<ChartSeries>* property)
{
    auto* chart = qobject_cast<FinancialChartItem*>(property->object);
    return chart ? chart->m_series.size() : 0;
}

ChartSeries* FinancialChartItem::seriesAt(QQmlListProperty<ChartSeries>* property, qsizetype index)
{
    auto* chart = qobject_cast<FinancialChartItem*>(property->object);
    if (!chart || index < 0 || index >= chart->m_series.size()) {
        return nullptr;
    }
    return chart->m_series.at(index);
}

void FinancialChartItem::clearSeries(QQmlListProperty<ChartSeries>* property)
{
    auto* chart = qobject_cast<FinancialChartItem*>(property->object);
    if (!chart) {
        return;
    }
    const QVector<ChartSeries*> copy = chart->m_series;
    for (ChartSeries* series : copy) {
        chart->detachSeries(series);
    }
    chart->scheduleDataUpdate();
}

QString FinancialChartItem::normalizedPriceScaleId(const QString& priceScaleId)
{
    const QString normalized = priceScaleId.trimmed();
    return normalized.isEmpty() ? QStringLiteral("right") : normalized;
}

QString FinancialChartItem::priceScaleKey(int pane, const QString& priceScaleId)
{
    return QString::number(std::max(0, pane)) + QLatin1Char(':') + normalizedPriceScaleId(priceScaleId);
}

void FinancialChartItem::attachSeries(ChartSeries* series)
{
    if (m_removed || !series || m_series.contains(series)) {
        return;
    }
    m_series.push_back(series);
    m_seriesPreviousParents.insert(series, series->parent());
    series->setParent(this);
    series->setChart(this);
    connect(series, &ChartSeries::dataChangedDetailed, this, &FinancialChartItem::handleSeriesDataChanged);
    connect(series, &ChartSeries::optionsChanged, this, &FinancialChartItem::handleSeriesOptionsChanged);
    scheduleDataUpdate();
}

void FinancialChartItem::detachSeries(ChartSeries* series)
{
    if (!series) {
        return;
    }
    m_series.removeAll(series);
    const QPointer<QObject> previousParent = m_seriesPreviousParents.take(series);
    disconnect(series, nullptr, this, nullptr);
    series->setChart(nullptr);
    if (series->parent() == this) {
        series->setParent(previousParent.data());
    }
}

void FinancialChartItem::handleSeriesDataChanged(const QVariantMap& payload)
{
    const QString scope = payload.value(QStringLiteral("scope")).toString();
    const bool timelineChanged = scope == QStringLiteral("full")
        || scope == QStringLiteral("pop")
        || (scope == QStringLiteral("update") && payload.value(QStringLiteral("inserted")).toBool());

    if (timelineChanged) {
        scheduleDataUpdate();
        return;
    }

    scheduleVisualUpdate();
}

void FinancialChartItem::handleSeriesOptionsChanged()
{
    if (seriesAttachmentsMatchModel()) {
        scheduleVisualUpdate();
        return;
    }

    scheduleDataUpdate();
}

bool FinancialChartItem::seriesAttachmentsMatchModel() const
{
    const QVector<SeriesAttachment>& attachments = m_model.series();
    if (attachments.size() != m_series.size()) {
        return false;
    }

    for (qsizetype i = 0; i < m_series.size(); ++i) {
        const ChartSeries* series = m_series.at(i);
        if (!series) {
            return false;
        }

        const SeriesAttachment& attachment = attachments.at(i);
        if (attachment.model != &series->model()
            || attachment.pane != series->pane()
            || attachment.visible != series->isVisible()
            || attachment.priceScaleId != normalizedPriceScaleId(series->priceScaleId())) {
            return false;
        }
    }

    return true;
}

void FinancialChartItem::rebuildModel()
{
    QVector<SeriesAttachment> attachments;
    attachments.reserve(m_series.size());
    QSet<QString> usedPriceScaleKeys;
    for (ChartSeries* series : m_series) {
        if (!series) {
            continue;
        }
        const QString priceScaleId = normalizedPriceScaleId(series->priceScaleId());
        usedPriceScaleKeys.insert(priceScaleKey(series->pane(), priceScaleId));
        attachments.push_back({ &series->model(), series->pane(), series->isVisible(), priceScaleId });
    }
    m_model.setPreserveEmptyPanes(m_preserveEmptyPanes);
    m_model.setPaneCount(m_paneCount);
    m_model.setSeries(std::move(attachments));

    for (auto it = m_priceScales.begin(); it != m_priceScales.end();) {
        const QString priceScaleId = it.key().section(QLatin1Char(':'), 1, -1);
        if (priceScaleId != QStringLiteral("right") && priceScaleId != QStringLiteral("left") && !usedPriceScaleKeys.contains(it.key())) {
            it = m_priceScales.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_manualPriceRanges.begin(); it != m_manualPriceRanges.end();) {
        const QString priceScaleId = it.key().section(QLatin1Char(':'), 1, -1);
        if (priceScaleId != QStringLiteral("right") && priceScaleId != QStringLiteral("left") && !usedPriceScaleKeys.contains(it.key())) {
            it = m_manualPriceRanges.erase(it);
        } else {
            ++it;
        }
    }
}

void FinancialChartItem::scheduleVisualUpdate()
{
    if (m_removed) {
        return;
    }
    polish();
    update();
}

void FinancialChartItem::updatePolish()
{
    QQuickItem::updatePolish();
    prepareRenderState();
}

QString FinancialChartItem::priceLabelCacheKey(double value, PriceFormat format)
{
    return QString::number(static_cast<int>(format)) + QLatin1Char(':') + QString::number(value, 'g', 17);
}

void FinancialChartItem::cacheRenderPriceLabel(double value, PriceFormat format)
{
    if (!std::isfinite(value)) {
        return;
    }
    const QString key = priceLabelCacheKey(value, format);
    if (!m_renderState.priceLabels.contains(key)) {
        m_renderState.priceLabels.insert(key, formatPriceValue(value, format));
    }
}

QString FinancialChartItem::renderPriceLabel(double value, PriceFormat format) const
{
    const QString key = priceLabelCacheKey(value, format);
    const auto it = m_renderState.priceLabels.constFind(key);
    if (it != m_renderState.priceLabels.constEnd()) {
        return it.value();
    }
    return PriceScale::formatValue(value, m_locale, m_pricePrecision, format);
}

bool FinancialChartItem::layoutContainsPane(const ChartLayout& layout, int pane)
{
    return std::any_of(layout.panes.cbegin(), layout.panes.cend(), [pane](const ChartLayout::Pane& candidate) {
        return candidate.index == pane;
    });
}

PriceScale FinancialChartItem::renderPriceScale(int pane, const QString& priceScaleId) const
{
    const auto it = m_renderState.priceScales.constFind(priceScaleKey(pane, priceScaleId));
    return it == m_renderState.priceScales.constEnd() ? PriceScale() : it.value();
}

bool FinancialChartItem::syncTimeScaleWithLayout(const ChartLayout& layout)
{
    if (layout.panes.isEmpty()) {
        return false;
    }

    m_model.timeScale().setWidth(layout.panes.first().dataRect.width());
    if (!m_fitContentRequested
        || m_model.timeScale().width() <= 0.0
        || m_model.timeScale().pointCount() <= 0) {
        return false;
    }

    m_model.timeScale().fitContent();
    m_fitContentRequested = false;
    return true;
}

QString FinancialChartItem::touchPriceScaleIdAt(const QPointF& point, const ChartLayout& layout) const
{
    const int paneIndex = paneAtY(point.y(), layout);
    for (const ChartLayout::Pane& pane : layout.panes) {
        if (m_priceScaleVisible && pane.rightPriceAxisRect.contains(point)) {
            return QStringLiteral("right");
        }
        if (m_leftPriceScaleVisible && pane.leftPriceAxisRect.contains(point)) {
            return QStringLiteral("left");
        }
    }

    bool hasRightSeries = false;
    bool hasLeftSeries = false;
    for (const ChartSeries* series : m_series) {
        if (!series || !series->isVisible() || series->pane() != paneIndex) {
            continue;
        }

        const QString scaleId = normalizedPriceScaleId(series->priceScaleId());
        if (scaleId == QStringLiteral("left")) {
            hasLeftSeries = true;
        } else if (scaleId == QStringLiteral("right")) {
            hasRightSeries = true;
        }
    }
    if (hasLeftSeries != hasRightSeries) {
        return hasLeftSeries ? QStringLiteral("left") : QStringLiteral("right");
    }

    return m_priceScaleVisible || !m_leftPriceScaleVisible
        ? QStringLiteral("right")
        : QStringLiteral("left");
}

QString FinancialChartItem::defaultCrosshairPriceScaleId(const QPointF& point, const ChartLayout& layout, int paneIndex) const
{
    Q_UNUSED(paneIndex);
    if (layout.panes.isEmpty()) {
        return QStringLiteral("right");
    }
    return normalizedPriceScaleId(touchPriceScaleIdAt(point, layout));
}

void FinancialChartItem::prepareRenderState()
{
    m_renderState = { };
    m_renderState.layout = currentLayout();
    if (m_renderState.layout.panes.isEmpty()) {
        return;
    }

    (void)syncTimeScaleWithLayout(m_renderState.layout);

    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
    m_renderState.visibleFrom = visibleFrom;
    m_renderState.visibleTo = visibleTo;
    m_renderState.barSpacing = m_model.timeScale().barSpacing();

    m_renderState.timeTicks = m_model.buildTimeTicks(m_renderState.layout.panes.first().dataRect,
        m_locale,
        m_timeVisible,
        m_secondsVisible,
        m_timeAxisMaxLabelWidth,
        activeTimeLabelFormat(),
        m_timeAxisUniformDistribution);
    for (TimeTick& tick : m_renderState.timeTicks) {
        tick.label = formatTimeTickValue(tick);
    }

    auto configureAndStoreScale = [this, visibleFrom, visibleTo](int pane, const QString& priceScaleId) -> PriceScale {
        const QString key = priceScaleKey(pane, priceScaleId);
        PriceScale& liveScale = m_priceScales[key];
        configurePriceScale(liveScale, pane, priceScaleId, visibleFrom, visibleTo);
        m_renderState.priceScales.insert(key, liveScale);
        return liveScale;
    };

    m_renderState.ticksByPane.resize(m_renderState.layout.panes.size());
    for (qsizetype panePosition = 0; panePosition < m_renderState.layout.panes.size(); ++panePosition) {
        const ChartLayout::Pane& pane = m_renderState.layout.panes.at(panePosition);

        const PriceScale rightScale = configureAndStoreScale(pane.index, QStringLiteral("right"));
        QVector<PriceTick> rightPriceTicks = rightScale.buildTicks(pane.dataRect,
            m_locale,
            m_pricePrecision,
            m_priceTickSpacing,
            displayPriceFormat());
        for (PriceTick& tick : rightPriceTicks) {
            tick.label = formatPriceTickValue(tick.price);
        }
        m_renderState.ticksByPane[panePosition].right = rightPriceTicks;

        const PriceScale leftScale = configureAndStoreScale(pane.index, QStringLiteral("left"));
        QVector<PriceTick> leftPriceTicks = leftScale.buildTicks(pane.dataRect,
            m_locale,
            m_pricePrecision,
            m_priceTickSpacing,
            displayPriceFormat());
        for (PriceTick& tick : leftPriceTicks) {
            tick.label = formatPriceTickValue(tick.price);
        }
        m_renderState.ticksByPane[panePosition].left = leftPriceTicks;
    }

    auto acceptsIndex = [this, visibleFrom, visibleTo](TimePoint time, PriceLineSource source) {
        if (source == PriceLineSource::LastBar) {
            return true;
        }
        const int index = m_model.indexForTime(time);
        return index >= visibleFrom && index <= visibleTo;
    };

    auto lastSeriesPrice = [&](const ChartSeries* series, PriceLineSource source) -> std::optional<double> {
        if (!series) {
            return std::nullopt;
        }

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            const QVector<CandlestickDataPoint>& points = series->model().candlesticks();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time, source)) {
                    return it->close;
                }
            }
            return std::nullopt;
        }

        if (qobject_cast<const LineSeries*>(series) || qobject_cast<const AreaSeries*>(series) || qobject_cast<const BaselineSeries*>(series)) {
            const QVector<LineDataPoint>& points = series->model().linePoints();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time, source)) {
                    return it->value;
                }
            }
            return std::nullopt;
        }

        if (qobject_cast<const HistogramSeries*>(series)) {
            const QVector<HistogramDataPoint>& points = series->model().histogramPoints();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time, source)) {
                    return it->value;
                }
            }
        }
        return std::nullopt;
    };

    auto priceAxisVisibleForScale = [this](const QString& priceScaleId) {
        const QString normalizedId = normalizedPriceScaleId(priceScaleId);
        if (normalizedId.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0) {
            return m_leftPriceScaleVisible;
        }
        if (normalizedId.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0) {
            return m_priceScaleVisible;
        }
        return false;
    };

    const PriceFormat renderFormat = displayPriceFormat();
    for (const ChartSeries* series : m_series) {
        if (!series || !series->isVisible()) {
            continue;
        }

        const PriceScale seriesScale = configureAndStoreScale(series->pane(), series->priceScaleId());
        if (priceAxisVisibleForScale(series->priceScaleId())) {
            for (const SeriesPriceLine& line : series->customPriceLines()) {
                if (line.axisLabelVisible && std::isfinite(line.price)) {
                    cacheRenderPriceLabel(seriesScale.priceToDisplayValue(line.price), renderFormat);
                }
            }
            const std::optional<double> lastPrice = lastSeriesPrice(series, static_cast<PriceLineSource>(series->priceLineSource()));
            if (series->lastValueLabelVisible() && lastPrice.has_value()) {
                cacheRenderPriceLabel(seriesScale.priceToDisplayValue(lastPrice.value()), renderFormat);
            }
        }
    }

    if (crosshairDrawable() && contains(m_crosshairPosition) && !m_model.timePoints().isEmpty()) {
        const int paneIndex = paneAtY(m_crosshairPosition.y(), m_renderState.layout);
        if (layoutContainsPane(m_renderState.layout, paneIndex)) {
            const ChartLayout::Pane& pane = *std::find_if(m_renderState.layout.panes.cbegin(),
                m_renderState.layout.panes.cend(),
                [paneIndex](const ChartLayout::Pane& candidate) {
                    return candidate.index == paneIndex;
                });
            const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
            const int logicalIndex = std::clamp(m_model.timeScale().coordinateToIndex(m_crosshairPosition.x() - pane.dataRect.left()), 0, lastIndex);
            const TimePoint time = m_model.timeAt(logicalIndex);
            const QString crosshairPriceScaleId = normalizedPriceScaleId(m_crosshairPriceScaleId);
            const PriceScale crosshairScale = configureAndStoreScale(paneIndex, crosshairPriceScaleId);
            const double price = crosshairScale.coordinateToPrice(m_crosshairPosition.y(), pane.dataRect);
            m_renderState.crosshairPane = paneIndex;
            m_renderState.crosshairPriceScaleId = crosshairPriceScaleId;
            m_renderState.crosshairTimeLabel = formatTimeValue(time);
            m_renderState.crosshairPriceLabel = formatPriceValue(crosshairScale.priceToDisplayValue(price));
            cacheRenderPriceLabel(crosshairScale.priceToDisplayValue(price), renderFormat);
        }
    }

    m_renderState.valid = true;
}

void FinancialChartItem::emitVisibleRangeSignals()
{
    Q_EMIT visibleLogicalRangeChanged(visibleLogicalRange());
    Q_EMIT visibleTimeRangeChanged(visibleTimeRange());
}

ChartLayout FinancialChartItem::currentLayoutWithSyncedTimeScale() const
{
    ChartLayout layout = currentLayout();
    (void)const_cast<FinancialChartItem*>(this)->syncTimeScaleWithLayout(layout);
    return layout;
}

void FinancialChartItem::updateCrosshair(const QPointF& point, const QVariantMap& sourceEvent)
{
    const CrosshairResolution resolution = resolvedCrosshair(point);
    m_crosshairPosition = resolution.point;
    m_crosshairPriceScaleId = normalizedPriceScaleId(resolution.priceScaleId);
    m_crosshairVisible = contains(point);
    scheduleVisualUpdate();
    if (m_crosshairVisible) {
        Q_EMIT crosshairMoved(crosshairPayload(m_crosshairPosition, sourceEvent, m_crosshairPriceScaleId));
    }
}

bool FinancialChartItem::zoomAtItemPosition(qreal itemX, qreal factor)
{
    if (!m_interactive || !m_handleScale || width() <= 0.0 || height() <= 0.0) {
        return false;
    }

    factor = boundedZoomFactor(factor);
    if (qFuzzyCompare(factor, 1.0)) {
        return false;
    }

    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty() || layout.panes.first().dataRect.width() <= 1.0) {
        return false;
    }

    const QRectF dataRect = layout.panes.first().dataRect;
    const qreal localX = std::clamp(itemX - dataRect.left(), 0.0, dataRect.width());
    m_model.timeScale().setWidth(dataRect.width());
    m_model.timeScale().zoomAt(localX, factor);
    m_fitContentRequested = false;
    scheduleVisualUpdate();
    Q_EMIT barSpacingChanged();
    Q_EMIT rightOffsetChanged();
    emitVisibleRangeSignals();
    return true;
}

int FinancialChartItem::paneAtY(qreal y, const ChartLayout& layout) const
{
    for (const ChartLayout::Pane& pane : layout.panes) {
        if (pane.dataRect.top() <= y && y <= pane.dataRect.bottom()) {
            return pane.index;
        }
    }
    return layout.panes.isEmpty() ? 0 : layout.panes.first().index;
}

int FinancialChartItem::paneSeparatorAt(const QPointF& point, const ChartLayout& layout) const
{
    if (!m_paneSeparatorsResizable || layout.panes.size() < 2) {
        return -1;
    }

    constexpr qreal hitHeight = 8.0;
    for (qsizetype i = 1; i < layout.panes.size(); ++i) {
        const ChartLayout::Pane& upper = layout.panes.at(i - 1);
        const ChartLayout::Pane& lower = layout.panes.at(i);
        const qreal separatorY = (upper.dataRect.bottom() + lower.dataRect.top()) * 0.5;
        const QRectF hitRect(0.0,
            separatorY - hitHeight * 0.5,
            width(),
            hitHeight);
        if (hitRect.contains(point)) {
            return upper.index;
        }
    }
    return -1;
}

bool FinancialChartItem::crosshairDrawable() const noexcept
{
    return m_crosshairVisible && m_crosshairMode != static_cast<int>(CrosshairMode::Hidden);
}

FinancialChartItem::CrosshairResolution FinancialChartItem::resolvedCrosshair(const QPointF& point) const
{
    CrosshairResolution resolution;
    resolution.point = point;

    const ChartLayout layout = currentLayout();
    const int paneIndex = paneAtY(point.y(), layout);
    resolution.priceScaleId = defaultCrosshairPriceScaleId(point, layout, paneIndex);

    const CrosshairMode mode = static_cast<CrosshairMode>(m_crosshairMode);
    if (mode == CrosshairMode::Normal || mode == CrosshairMode::Hidden) {
        return resolution;
    }

    if (layout.panes.isEmpty() || m_model.timePoints().isEmpty()) {
        return resolution;
    }

    const ChartLayout::Pane* pane = nullptr;
    for (const ChartLayout::Pane& candidate : layout.panes) {
        if (candidate.index == paneIndex) {
            pane = &candidate;
            break;
        }
    }
    if (!pane || pane->dataRect.width() <= 0.0 || pane->dataRect.height() <= 0.0) {
        return resolution;
    }

    const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
    const int logicalIndex = std::clamp(m_model.timeScale().coordinateToIndex(point.x() - pane->dataRect.left()), 0, lastIndex);
    const TimePoint time = m_model.timeAt(logicalIndex);
    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);

    qreal bestY = std::numeric_limits<qreal>::quiet_NaN();
    qreal bestDistance = std::numeric_limits<qreal>::max();
    QString bestPriceScaleId;
    for (const ChartSeries* series : m_series) {
        if (!series || series->pane() != paneIndex || (!series->isVisible() && !m_snapCrosshairToHiddenSeries)) {
            continue;
        }

        const QString seriesPriceScaleId = normalizedPriceScaleId(series->priceScaleId());
        PriceScale priceScale = m_priceScales.value(priceScaleKey(paneIndex, seriesPriceScaleId));
        configurePriceScale(priceScale, paneIndex, seriesPriceScaleId, visibleFrom, visibleTo);
        for (double candidatePrice : crosshairCandidatePrices(series, time, mode)) {
            if (!std::isfinite(candidatePrice)) {
                continue;
            }
            const qreal y = priceScale.priceToCoordinate(candidatePrice, pane->dataRect);
            if (!std::isfinite(y)) {
                continue;
            }
            const qreal distance = std::abs(y - point.y());
            if (!std::isfinite(bestY) || distance < bestDistance) {
                bestY = y;
                bestDistance = distance;
                bestPriceScaleId = seriesPriceScaleId;
            }
        }
    }

    if (!std::isfinite(bestY)) {
        resolution.point = QPointF(pane->dataRect.left() + m_model.timeScale().indexToCoordinate(logicalIndex),
            point.y());
        return resolution;
    }

    resolution.point = QPointF(pane->dataRect.left() + m_model.timeScale().indexToCoordinate(logicalIndex),
        bestY);
    if (!bestPriceScaleId.isEmpty()) {
        resolution.priceScaleId = bestPriceScaleId;
    }
    return resolution;
}

QVariantMap FinancialChartItem::hitTestPayload(const QPointF& point) const
{
    struct Candidate {
        const ChartSeries* series = nullptr;
        int seriesIndex = -1;
        QString type;
        QString objectKind;
        QString objectId;
        qreal distance = std::numeric_limits<qreal>::max();
        int priority = 0;
        int cursorShape = -1;
    };

    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    if (layout.panes.isEmpty() || m_model.timePoints().isEmpty()) {
        return { };
    }

    const int paneIndex = paneAtY(point.y(), layout);
    const ChartLayout::Pane* pane = nullptr;
    for (const ChartLayout::Pane& candidate : layout.panes) {
        if (candidate.index == paneIndex) {
            pane = &candidate;
            break;
        }
    }
    if (!pane || point.y() < pane->dataRect.top() || point.y() > pane->dataRect.bottom()) {
        return { };
    }

    const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
    const int logicalIndex = std::clamp(m_model.timeScale().coordinateToIndex(point.x() - pane->dataRect.left()), 0, lastIndex);
    const TimePoint time = m_model.timeAt(logicalIndex);
    const qreal barX = pane->dataRect.left() + m_model.timeScale().indexToCoordinate(logicalIndex);
    const qreal barSpacing = std::max<qreal>(1.0, m_model.timeScale().barSpacing());
    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);

    std::optional<Candidate> best;
    auto consider = [&best](const Candidate& candidate) {
        if (!candidate.series || !std::isfinite(candidate.distance)) {
            return;
        }
        if (!best.has_value()
            || candidate.priority > best->priority
            || (candidate.priority == best->priority && candidate.distance < best->distance)) {
            best = candidate;
        }
    };

    for (int seriesIndex = static_cast<int>(m_series.size()) - 1; seriesIndex >= 0; --seriesIndex) {
        const ChartSeries* series = m_series.at(seriesIndex);
        if (!series || !series->isVisible() || series->pane() != paneIndex) {
            continue;
        }

        PriceScale priceScale = m_priceScales.value(priceScaleKey(paneIndex, series->priceScaleId()));
        configurePriceScale(priceScale, paneIndex, series->priceScaleId(), visibleFrom, visibleTo);
        const qreal tolerance = std::max<qreal>(0.0, series->hitTestTolerance());

        for (const SeriesMarker& marker : series->model().markers()) {
            const int markerIndex = m_model.indexForTime(marker.time);
            if (markerIndex < visibleFrom || markerIndex > visibleTo) {
                continue;
            }

            const std::optional<double> price = isAtPricePosition(marker.position) && std::isfinite(marker.price)
                ? std::optional<double>(marker.price)
                : series->model().priceAt(marker.time, marker.position);
            if (!price.has_value()) {
                continue;
            }

            const qreal x = pane->dataRect.left() + m_model.timeScale().indexToCoordinate(markerIndex);
            qreal y = priceScale.priceToCoordinate(price.value(), pane->dataRect);
            if (!std::isfinite(y)) {
                continue;
            }
            const qreal radius = 4.5 * marker.size;
            if (marker.position == MarkerPosition::AboveBar) {
                y -= 12.0;
            } else if (marker.position == MarkerPosition::BelowBar) {
                y += 12.0;
            } else if (marker.position == MarkerPosition::AtPriceTop) {
                y += radius;
            } else if (marker.position == MarkerPosition::AtPriceBottom) {
                y -= radius;
            }

            const qreal dx = std::abs(point.x() - x);
            const qreal dy = std::abs(point.y() - y);
            const qreal distance = marker.shape == MarkerShape::Square
                ? std::max(dx, dy)
                : std::hypot(dx, dy);
            if (distance <= radius + tolerance) {
                consider({ series, seriesIndex, QStringLiteral("marker"), QStringLiteral("series-marker"), marker.id, distance, 40, marker.cursorShape });
            }
        }

        if (point.x() >= pane->dataRect.left() && point.x() <= pane->dataRect.right()) {
            for (const SeriesPriceLine& line : series->customPriceLines()) {
                if (!line.lineVisible || !std::isfinite(line.price)) {
                    continue;
                }
                const qreal y = priceScale.priceToCoordinate(line.price, pane->dataRect);
                if (!std::isfinite(y)) {
                    continue;
                }
                const qreal distance = std::abs(point.y() - y);
                if (distance <= tolerance + std::max<qreal>(0.5, line.lineWidth * 0.5)) {
                    consider({ series, seriesIndex, QStringLiteral("price-line"), QStringLiteral("custom-price-line"), line.id, distance, 30, line.cursorShape });
                }
            }
        }

        if (std::abs(point.x() - barX) > barSpacing * 0.5 + tolerance) {
            continue;
        }

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            if (const CandlestickDataPoint* dataPoint = findPointByTime(series->model().candlesticks(), time)) {
                const qreal highY = priceScale.priceToCoordinate(dataPoint->high, pane->dataRect);
                const qreal lowY = priceScale.priceToCoordinate(dataPoint->low, pane->dataRect);
                if (!std::isfinite(highY) || !std::isfinite(lowY)) {
                    continue;
                }
                const qreal top = std::min(highY, lowY);
                const qreal bottom = std::max(highY, lowY);
                if (point.y() >= top - tolerance && point.y() <= bottom + tolerance) {
                    const qreal distance = point.y() < top ? top - point.y() : (point.y() > bottom ? point.y() - bottom : 0.0);
                    consider({ series, seriesIndex, QStringLiteral("series-range"), QStringLiteral("series"), QString(), distance, 10 });
                }
            }
            continue;
        }

        if (const auto* histogramSeries = qobject_cast<const HistogramSeries*>(series)) {
            if (const HistogramDataPoint* dataPoint = findPointByTime(series->model().histogramPoints(), time)) {
                const qreal valueY = priceScale.priceToCoordinate(dataPoint->value, pane->dataRect);
                const qreal baseY = priceScale.priceToCoordinate(histogramSeries->baseValue(), pane->dataRect);
                if (!std::isfinite(valueY) || !std::isfinite(baseY)) {
                    continue;
                }
                const qreal top = std::min(valueY, baseY);
                const qreal bottom = std::max(valueY, baseY);
                if (point.y() >= top - tolerance && point.y() <= bottom + tolerance) {
                    const qreal distance = point.y() < top ? top - point.y() : (point.y() > bottom ? point.y() - bottom : 0.0);
                    consider({ series, seriesIndex, QStringLiteral("series-range"), QStringLiteral("series"), QString(), distance, 10 });
                }
            }
            continue;
        }

        if (const LineDataPoint* dataPoint = findPointByTime(series->model().linePoints(), time)) {
            qreal lineWidth = 1.0;
            if (const auto* lineSeries = qobject_cast<const LineSeries*>(series)) {
                lineWidth = lineSeries->lineWidth();
            } else if (const auto* areaSeries = qobject_cast<const AreaSeries*>(series)) {
                lineWidth = areaSeries->lineWidth();
            } else if (const auto* baselineSeries = qobject_cast<const BaselineSeries*>(series)) {
                lineWidth = baselineSeries->lineWidth();
            }

            const qreal y = priceScale.priceToCoordinate(dataPoint->value, pane->dataRect);
            if (!std::isfinite(y)) {
                continue;
            }
            const qreal distance = std::hypot(point.x() - barX, point.y() - y);
            if (distance <= tolerance + std::max<qreal>(1.0, lineWidth * 0.5)) {
                consider({ series, seriesIndex, QStringLiteral("series-point"), QStringLiteral("series"), QString(), distance, 20 });
            }
        }
    }

    if (!best.has_value()) {
        return { };
    }

    QString seriesName = best->series->name();
    if (seriesName.isEmpty()) {
        seriesName = best->series->objectName();
    }
    if (seriesName.isEmpty()) {
        seriesName = QStringLiteral("series%1").arg(best->seriesIndex);
    }

    QVariantMap hoveredInfo {
        { QStringLiteral("type"), best->type },
        { QStringLiteral("sourceKind"), QStringLiteral("series") },
        { QStringLiteral("objectKind"), best->objectKind },
        { QStringLiteral("paneIndex"), paneIndex },
        { QStringLiteral("series"), QVariant::fromValue(const_cast<ChartSeries*>(best->series)) },
        { QStringLiteral("seriesName"), seriesName },
        { QStringLiteral("seriesIndex"), best->seriesIndex },
    };
    if (!best->objectId.isEmpty()) {
        hoveredInfo.insert(QStringLiteral("objectId"), best->objectId);
    }
    if (best->cursorShape >= 0) {
        hoveredInfo.insert(QStringLiteral("cursorShape"), best->cursorShape);
    }

    QVariantMap payload {
        { QStringLiteral("hoveredSeries"), QVariant::fromValue(const_cast<ChartSeries*>(best->series)) },
        { QStringLiteral("hoveredSeriesName"), seriesName },
        { QStringLiteral("hoveredSeriesIndex"), best->seriesIndex },
        { QStringLiteral("hoveredInfo"), hoveredInfo },
    };
    if (!best->objectId.isEmpty()) {
        payload.insert(QStringLiteral("hoveredObjectId"), best->objectId);
    }
    if (best->cursorShape >= 0) {
        payload.insert(QStringLiteral("cursorShape"), best->cursorShape);
    }
    return payload;
}

QVariantMap FinancialChartItem::crosshairPayload(const QPointF& point,
    const QVariantMap& sourceEvent,
    const QString& priceScaleId) const
{
    QVariantMap payload;
    const ChartLayout layout = currentLayout();
    if (layout.panes.isEmpty() || m_model.timePoints().isEmpty()) {
        return payload;
    }

    const int paneIndex = paneAtY(point.y(), layout);
    const ChartLayout::Pane* pane = nullptr;
    for (const ChartLayout::Pane& candidate : layout.panes) {
        if (candidate.index == paneIndex) {
            pane = &candidate;
            break;
        }
    }
    if (!pane) {
        return payload;
    }

    const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
    const int logicalIndex = std::clamp(m_model.timeScale().coordinateToIndex(point.x() - pane->dataRect.left()), 0, lastIndex);
    const TimePoint time = m_model.timeAt(logicalIndex);
    const QString payloadPriceScaleId = priceScaleId.trimmed().isEmpty()
        ? defaultCrosshairPriceScaleId(point, layout, paneIndex)
        : normalizedPriceScaleId(priceScaleId);
    PriceScale priceScale = m_priceScales.value(priceScaleKey(paneIndex, payloadPriceScaleId));
    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
    configurePriceScale(priceScale, paneIndex, payloadPriceScaleId, visibleFrom, visibleTo);
    const double price = priceScale.coordinateToPrice(point.y(), pane->dataRect);

    payload.insert(QStringLiteral("pane"), paneIndex);
    payload.insert(QStringLiteral("paneIndex"), paneIndex);
    payload.insert(QStringLiteral("point"), pointPayload(point));
    payload.insert(QStringLiteral("logicalIndex"), logicalIndex);
    payload.insert(QStringLiteral("time"), time);
    payload.insert(QStringLiteral("timeLabel"), formatTimeValue(time));
    payload.insert(QStringLiteral("price"), price);
    payload.insert(QStringLiteral("priceLabel"), formatPriceValue(priceScale.priceToDisplayValue(price)));
    payload.insert(QStringLiteral("priceScaleId"), payloadPriceScaleId);

    QVariantMap seriesData;
    for (qsizetype i = 0; i < m_series.size(); ++i) {
        const ChartSeries* series = m_series.at(i);
        if (!series || !series->isVisible()) {
            continue;
        }

        QVariantMap row = series->dataByIndex(logicalIndex);
        if (row.isEmpty()) {
            row = seriesDataAt(series, time);
        }
        if (row.isEmpty()) {
            continue;
        }

        QString key = series->name();
        if (key.isEmpty()) {
            key = series->objectName();
        }
        if (key.isEmpty()) {
            key = QStringLiteral("series%1").arg(i);
        }

        const QString baseKey = key;
        int suffix = 2;
        while (seriesData.contains(key)) {
            key = QStringLiteral("%1#%2").arg(baseKey).arg(suffix++);
        }
        seriesData.insert(key, row);
    }
    payload.insert(QStringLiteral("seriesData"), seriesData);
    const QVariantMap hitTest = hitTestPayload(point);
    for (auto it = hitTest.cbegin(); it != hitTest.cend(); ++it) {
        payload.insert(it.key(), it.value());
    }
    if (!sourceEvent.isEmpty()) {
        payload.insert(QStringLiteral("sourceEvent"), sourceEvent);
    }
    return payload;
}

std::optional<PriceRange> FinancialChartItem::autoscaleProviderRange(const ChartSeries* series,
    const PriceRange& defaultRange,
    int pane,
    int visibleFrom,
    int visibleTo) const
{
    if (!series) {
        return std::nullopt;
    }

    QJSValue provider = series->autoscaleInfoProvider();
    if (!provider.isCallable()) {
        return std::nullopt;
    }

    QJSValue result = provider.call({
        QJSValue(defaultRange.isValid() ? defaultRange.min : std::numeric_limits<double>::quiet_NaN()),
        QJSValue(defaultRange.isValid() ? defaultRange.max : std::numeric_limits<double>::quiet_NaN()),
        QJSValue(visibleFrom),
        QJSValue(visibleTo),
        QJSValue(pane),
    });
    if (result.isError()) {
        qWarning() << "QtFinCharts: autoscaleInfoProvider failed" << result.toString();
        return std::nullopt;
    }
    if (result.isNull() || result.isUndefined()) {
        return std::nullopt;
    }

    QJSValue rangeValue = result.property(QStringLiteral("priceRange"));
    if (!rangeValue.isObject()) {
        rangeValue = result;
    }

    auto readNumber = [&rangeValue](const QString& primary, const QString& fallback) -> std::optional<double> {
        QJSValue value = rangeValue.property(primary);
        if (!value.isNumber()) {
            value = rangeValue.property(fallback);
        }
        if (!value.isNumber()) {
            return std::nullopt;
        }
        const double number = value.toNumber();
        return std::isfinite(number) ? std::optional<double>(number) : std::nullopt;
    };

    const std::optional<double> min = readNumber(QStringLiteral("min"), QStringLiteral("from"));
    const std::optional<double> max = readNumber(QStringLiteral("max"), QStringLiteral("to"));
    if (!min.has_value() || !max.has_value()) {
        qWarning() << "QtFinCharts: autoscaleInfoProvider returned no finite price range";
        return std::nullopt;
    }

    PriceRange range { min.value(), max.value() };
    if (range.max < range.min) {
        std::swap(range.min, range.max);
    }
    return range.isValid() ? std::optional<PriceRange>(range) : std::nullopt;
}

PriceRange FinancialChartItem::resolvedPriceRange(int pane,
    const QString& priceScaleId,
    int visibleFrom,
    int visibleTo,
    bool* manualRange) const
{
    if (manualRange) {
        *manualRange = false;
    }

    const QString normalizedId = normalizedPriceScaleId(priceScaleId);
    const auto manual = m_manualPriceRanges.constFind(priceScaleKey(pane, normalizedId));
    if (manual != m_manualPriceRanges.constEnd() && manual.value().isValid()) {
        if (manualRange) {
            *manualRange = true;
        }
        return manual.value();
    }

    PriceRange range;
    const QHash<TimePoint, int>& indexByTime = m_model.indexByTime();
    for (const ChartSeries* series : m_series) {
        if (!series || !series->isVisible() || series->pane() != pane) {
            continue;
        }
        if (normalizedPriceScaleId(series->priceScaleId()) != normalizedId) {
            continue;
        }

        const PriceRange defaultRange = series->model().priceRangeForLogicalRange(indexByTime,
            visibleFrom,
            visibleTo,
            m_priceScaleMode);
        const std::optional<PriceRange> providerRange = autoscaleProviderRange(series, defaultRange, pane, visibleFrom, visibleTo);
        range.include(providerRange.value_or(defaultRange));
    }
    return range.padded(0.08);
}

void FinancialChartItem::setAutoScaleForScale(const QString& priceScaleId, bool enabled)
{
    const QString normalizedId = normalizedPriceScaleId(priceScaleId);
    if (enabled) {
        clearVisiblePriceRangeForScale(normalizedId);
        return;
    }

    bool changed = false;
    const ChartLayout layout = currentLayoutWithSyncedTimeScale();
    const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
    for (const ChartLayout::Pane& pane : layout.panes) {
        const PriceRange current = resolvedPriceRange(pane.index, normalizedId, visibleFrom, visibleTo);
        if (!current.isValid()) {
            continue;
        }
        m_manualPriceRanges.insert(priceScaleKey(pane.index, normalizedId), current);
        changed = true;
    }

    if (changed) {
        scheduleVisualUpdate();
        Q_EMIT axisOptionsChanged();
    }
}

void FinancialChartItem::configurePriceScale(PriceScale& priceScale,
    int pane,
    const QString& priceScaleId,
    int visibleFrom,
    int visibleTo) const
{
    bool manualRange = false;
    const PriceRange range = resolvedPriceRange(pane, priceScaleId, visibleFrom, visibleTo, &manualRange);
    priceScale.setMargins(m_priceScaleTopMargin, m_priceScaleBottomMargin);
    priceScale.setInverted(m_priceScaleInverted);
    priceScale.setMode(m_priceScaleMode);
    const auto [baseVisibleFrom, baseVisibleTo] = m_model.timeScale().visibleIndexRange(0);
    const std::optional<double> baseValue = firstVisiblePrice(pane, priceScaleId, baseVisibleFrom, baseVisibleTo);
    priceScale.setBaseValue(baseValue.value_or(range.isValid() ? range.min : 1.0));
    priceScale.setRange(range, false);
}

std::optional<double> FinancialChartItem::firstVisiblePrice(int pane,
    const QString& priceScaleId,
    int visibleFrom,
    int visibleTo) const
{
    const QString normalizedId = normalizedPriceScaleId(priceScaleId);
    int bestIndex = std::numeric_limits<int>::max();
    std::optional<double> bestPrice;

    auto consider = [&](TimePoint time, double price) {
        if (!std::isfinite(price)) {
            return;
        }
        if ((m_priceScaleMode == PriceScaleMode::Percentage || m_priceScaleMode == PriceScaleMode::IndexedTo100)
            && std::abs(price) <= 1e-12) {
            return;
        }
        const int index = m_model.indexForTime(time);
        if (index < visibleFrom || index > visibleTo || index >= bestIndex) {
            return;
        }
        bestIndex = index;
        bestPrice = price;
    };

    for (const ChartSeries* series : m_series) {
        if (!series || !series->isVisible() || series->pane() != pane) {
            continue;
        }
        if (normalizedPriceScaleId(series->priceScaleId()) != normalizedId) {
            continue;
        }

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            for (const CandlestickDataPoint& point : series->model().candlesticks()) {
                consider(point.time, point.close);
            }
            continue;
        }

        if (qobject_cast<const LineSeries*>(series) || qobject_cast<const AreaSeries*>(series) || qobject_cast<const BaselineSeries*>(series)) {
            for (const LineDataPoint& point : series->model().linePoints()) {
                consider(point.time, point.value);
            }
            continue;
        }

        if (qobject_cast<const HistogramSeries*>(series)) {
            for (const HistogramDataPoint& point : series->model().histogramPoints()) {
                consider(point.time, point.value);
            }
        }
    }

    return bestPrice;
}

bool FinancialChartItem::hasManualPriceRangeForScale(const QString& priceScaleId, int pane) const
{
    const QString normalizedId = normalizedPriceScaleId(priceScaleId);
    if (pane >= 0) {
        const auto manual = m_manualPriceRanges.constFind(priceScaleKey(pane, normalizedId));
        return manual != m_manualPriceRanges.constEnd() && manual.value().isValid();
    }

    for (auto it = m_manualPriceRanges.constBegin(); it != m_manualPriceRanges.constEnd(); ++it) {
        if (!it.value().isValid()) {
            continue;
        }
        const QString id = it.key().section(QLatin1Char(':'), 1, -1);
        if (id == normalizedId) {
            return true;
        }
    }
    return false;
}

PriceFormat FinancialChartItem::displayPriceFormat() const noexcept
{
    if (m_priceScaleMode == PriceScaleMode::Percentage) {
        return PriceFormat::Percent;
    }
    return m_priceFormat;
}

QString FinancialChartItem::formatPriceValue(double value) const
{
    return formatPriceValue(value, displayPriceFormat());
}

QString FinancialChartItem::formatPriceValue(double value, PriceFormat format) const
{
    const double displayValue = std::abs(value) <= 1e-12 ? 0.0 : value;
    const QJSValue& formatter = format == PriceFormat::Percent ? m_percentageFormatter : m_priceFormatter;
    return callFormatter(formatter,
        { QJSValue(displayValue) },
        PriceScale::formatValue(displayValue, m_locale, m_pricePrecision, format));
}

QString FinancialChartItem::formatPriceTickValue(double value) const
{
    return formatPriceTickValue(value, displayPriceFormat());
}

QString FinancialChartItem::formatPriceTickValue(double value, PriceFormat format) const
{
    const QJSValue& formatter = format == PriceFormat::Percent
        ? m_percentageTickMarkFormatter
        : m_priceTickMarkFormatter;
    return callFormatter(formatter, { QJSValue(value) }, formatPriceValue(value, format));
}

QString FinancialChartItem::formatTimeValue(TimePoint time) const
{
    const QString fallback = m_locale.toString(QDateTime::fromSecsSinceEpoch(time, QTimeZone::UTC),
        activeTimeLabelFormat());
    return callFormatter(m_timeFormatter, { QJSValue(static_cast<double>(time)) }, fallback);
}

QString FinancialChartItem::formatTimeTickValue(const TimeTick& tick) const
{
    return callFormatter(m_timeTickMarkFormatter,
        { QJSValue(static_cast<double>(tick.time)), QJSValue(static_cast<double>(tick.index)) },
        tick.label);
}

QString FinancialChartItem::callFormatter(const QJSValue& formatter,
    const QJSValueList& args,
    const QString& fallback) const
{
    if (!formatter.isCallable()) {
        return fallback;
    }

    QJSValue callable(formatter);
    const QJSValue result = callable.call(args);
    if (result.isError()) {
        qWarning() << "QtFinCharts: formatter callback failed" << result.toString();
        return fallback;
    }

    const QString text = result.toString();
    return text.isNull() ? fallback : text;
}

QString FinancialChartItem::activeTimeLabelFormat() const
{
    if (!m_timeVisible) {
        return m_dateFormat;
    }
    return m_secondsVisible ? m_timeWithSecondsFormat : m_timeFormat;
}

ChartLayout FinancialChartItem::currentLayout() const
{
    return m_model.layoutForSize(QSizeF(width(), height()),
        m_timeScaleVisible,
        m_priceScaleVisible,
        m_timeAxisMinimumHeight,
        m_priceAxisMinimumWidth,
        m_leftPriceScaleVisible,
        m_leftPriceAxisMinimumWidth);
}

QSGNode* FinancialChartItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* root = oldNode ? static_cast<ChartRootNode*>(oldNode) : new ChartRootNode;
    clearChildren(root);

    QQuickWindow* quickWindow = window();
    if (m_removed || !quickWindow || width() <= 0.0 || height() <= 0.0) {
        return root;
    }

    if (m_textCacheDirty) {
        root->textCache.clear();
        m_textCacheDirty = false;
    }

    const qreal dpr = devicePixelRatioFor(quickWindow);
    const qreal px = oneDevicePixel(dpr);
    const QSizeF itemSize(width(), height());

    const QRectF itemRect(QPointF(0, 0), itemSize);
    if (m_theme->backgroundType() == QStringLiteral("verticalGradient")) {
        Vertices backgroundVertices;
        appendRect(backgroundVertices, itemRect);
        if (QSGGeometryNode* backgroundNode = gradientTriangles(backgroundVertices, [&](qreal y) {
                return colorForY(y, itemRect.top(), itemRect.bottom(), m_theme->backgroundTopColor(), m_theme->backgroundBottomColor());
            })) {
            root->appendChildNode(backgroundNode);
        }
    } else {
        root->appendChildNode(new QSGSimpleRectNode(itemRect, m_theme->backgroundColor()));
    }

    if (!m_renderState.valid || m_renderState.layout.panes.isEmpty()) {
        return root;
    }
    const ChartLayout layout = m_renderState.layout;
    const int visibleFrom = m_renderState.visibleFrom;
    const int visibleTo = m_renderState.visibleTo;
    const QHash<TimePoint, int>& renderIndexByTime = m_model.indexByTime();
    const qreal renderBarSpacing = m_renderState.barSpacing;
    const QVector<TimeTick>& timeTicks = m_renderState.timeTicks;
    bool hasCrosshairDataPoint = false;
    int crosshairIndex = -1;
    TimePoint crosshairTime = 0;
    if (crosshairDrawable() && contains(m_crosshairPosition) && !m_model.timePoints().isEmpty()) {
        const int lastIndex = static_cast<int>(m_model.timePoints().size() - 1);
        crosshairIndex = std::clamp(m_model.timeScale().coordinateToIndex(m_crosshairPosition.x() - layout.panes.first().dataRect.left()), 0, lastIndex);
        crosshairTime = m_model.timeAt(crosshairIndex);
        hasCrosshairDataPoint = true;
    }

    QFont labelFont;
    if (!m_theme->fontFamily().isEmpty()) {
        labelFont.setFamily(m_theme->fontFamily());
    }
    labelFont.setPixelSize(m_theme->fontPixelSize());
    QFont timeLabelFont = labelFont;
    timeLabelFont.setBold(m_timeAxisBoldLabels);

    auto addTextWithFont = [root, quickWindow, dpr](QSGNode* parent,
                               const QString& text,
                               const QFont& font,
                               const QColor& foreground,
                               const QColor& background,
                               QPointF topLeft,
                               bool centerX,
                               bool centerY) -> QSizeF {
        if (!quickWindow) {
            return { };
        }

        LabelTexture* label = root->textCache.get(text, font, foreground, background, dpr);
        if (!label || label->image.isNull()) {
            return { };
        }

        QSGTexture* texture = root->textCache.textureFor(label, quickWindow);
        if (!texture) {
            return { };
        }

        if (centerX) {
            topLeft.rx() -= label->logicalSize.width() * 0.5;
        }
        if (centerY) {
            topLeft.ry() -= label->logicalSize.height() * 0.5;
        }

        auto* node = new QSGSimpleTextureNode;
        node->setTexture(texture);
        node->setOwnsTexture(false);
        node->setFiltering(QSGTexture::Linear);
        node->setRect(QRectF(topLeft, label->logicalSize));
        parent->appendChildNode(node);
        return label->logicalSize;
    };

    auto addText = [&addTextWithFont, labelFont](QSGNode* parent,
                       const QString& text,
                       const QColor& foreground,
                       const QColor& background,
                       QPointF topLeft,
                       bool centerX = false,
                       bool centerY = false) -> QSizeF {
        return addTextWithFont(parent, text, labelFont, foreground, background, topLeft, centerX, centerY);
    };

    auto measureText = [root, dpr, labelFont](const QString& text,
                           const QColor& foreground,
                           const QColor& background) -> QSizeF {
        LabelTexture* label = root->textCache.get(text, labelFont, foreground, background, dpr);
        return label ? label->logicalSize : QSizeF();
    };

    const QColor priceAxisTextColor = colorOr(m_priceAxisTextColor, m_theme->textColor());
    auto isLeftPriceScale = [](const QString& priceScaleId) {
        return priceScaleId.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0;
    };
    auto isRightPriceScale = [](const QString& priceScaleId) {
        return priceScaleId.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0;
    };
    auto priceAxisRectForScale = [&](const ChartLayout::Pane& pane, const QString& priceScaleId) -> QRectF {
        const QString normalizedId = normalizedPriceScaleId(priceScaleId);
        if (isLeftPriceScale(normalizedId)) {
            return pane.leftPriceAxisRect;
        }
        if (isRightPriceScale(normalizedId)) {
            return pane.rightPriceAxisRect;
        }
        return { };
    };
    auto priceAxisVisibleForScale = [&](const QString& priceScaleId) {
        const QString normalizedId = normalizedPriceScaleId(priceScaleId);
        if (isLeftPriceScale(normalizedId)) {
            return m_leftPriceScaleVisible;
        }
        if (isRightPriceScale(normalizedId)) {
            return m_priceScaleVisible;
        }
        return false;
    };
    auto priceAxisLabelTopLeft = [&](const QRectF& axisRect, const QSizeF& labelSize, qreal labelTop, bool leftAxis) {
        QString alignment = m_priceAxisLabelAlignment;
        if (alignment == QStringLiteral("auto")) {
            alignment = leftAxis ? QStringLiteral("right") : QStringLiteral("left");
        }

        qreal x = axisRect.left() + 5.0;
        if (alignment == QStringLiteral("center")) {
            x = axisRect.center().x() - labelSize.width() * 0.5;
        } else if (alignment == QStringLiteral("right")) {
            x = axisRect.right() - labelSize.width() - 5.0;
        }

        const qreal minX = axisRect.left() + 2.0;
        const qreal maxX = std::max(minX, axisRect.right() - labelSize.width() - 2.0);
        return QPointF(std::clamp(x, minX, maxX), labelTop);
    };

    auto appendPriceAxis = [&](const ChartLayout::Pane& pane, const QVector<PriceTick>& priceTicks, bool leftAxis) {
        const QRectF axisRect = leftAxis ? pane.leftPriceAxisRect : pane.rightPriceAxisRect;
        const bool axisVisible = leftAxis ? m_leftPriceScaleVisible : m_priceScaleVisible;
        if (!axisVisible || axisRect.width() <= 0.0) {
            return;
        }

        Vertices axisLines;
        const qreal borderX = leftAxis ? pane.dataRect.left() : pane.dataRect.right();
        const bool borderVisible = leftAxis ? m_leftPriceAxisBorderVisible : m_priceAxisBorderVisible;
        if (borderVisible) {
            appendVerticalLine(axisLines, borderX, pane.dataRect.top(), pane.dataRect.bottom(), px, dpr);
        }
        const bool tickMarksVisible = leftAxis ? m_leftPriceAxisTickMarksVisible : m_priceAxisTickMarksVisible;
        if (tickMarksVisible) {
            for (const PriceTick& tick : priceTicks) {
                if (leftAxis) {
                    appendHorizontalLine(axisLines, pane.dataRect.left() - 4.0, pane.dataRect.left(), tick.y, px, dpr);
                } else {
                    appendHorizontalLine(axisLines, pane.dataRect.right(), pane.dataRect.right() + 4.0, tick.y, px, dpr);
                }
            }
        }
        if (QSGGeometryNode* node = solidTriangles(axisLines, m_theme->priceAxisBorderColor())) {
            root->appendChildNode(node);
        }

        QVector<PriceTick> sortedTicks = priceTicks;
        std::sort(sortedTicks.begin(), sortedTicks.end(), [](const PriceTick& lhs, const PriceTick& rhs) {
            return lhs.y < rhs.y;
        });

        qreal nextAvailableTop = pane.dataRect.top();
        constexpr qreal labelGap = 2.0;
        for (const PriceTick& tick : sortedTicks) {
            if (!std::isfinite(tick.y) || tick.label.isEmpty()) {
                continue;
            }

            const QSizeF labelSize = measureText(tick.label, priceAxisTextColor, transparent());
            if (labelSize.isEmpty()) {
                continue;
            }

            const qreal maxTop = std::max(pane.dataRect.top(), pane.dataRect.bottom() - labelSize.height());
            const qreal labelTop = std::clamp(tick.y - labelSize.height() * 0.5,
                pane.dataRect.top(),
                maxTop);
            if (labelTop < nextAvailableTop) {
                continue;
            }

            addText(root,
                tick.label,
                priceAxisTextColor,
                transparent(),
                priceAxisLabelTopLeft(axisRect, labelSize, labelTop, leftAxis),
                false,
                false);
            nextAvailableTop = labelTop + labelSize.height() + labelGap;
        }
    };

    auto appendTimeAxis = [&] {
        Vertices axisLines;
        if (m_timeAxisBorderVisible) {
            appendHorizontalLine(axisLines,
                layout.timeAxisRect.left(),
                layout.timeAxisRect.right(),
                layout.timeAxisRect.top(),
                px,
                dpr);
        }
        if (m_timeAxisTickMarksVisible) {
            for (const TimeTick& tick : timeTicks) {
                appendVerticalLine(axisLines, tick.x, layout.timeAxisRect.top(), layout.timeAxisRect.top() + 4.0, px, dpr);
            }
        }
        if (QSGGeometryNode* node = solidTriangles(axisLines, m_theme->timeAxisBorderColor())) {
            root->appendChildNode(node);
        }

        for (const TimeTick& tick : timeTicks) {
            QPointF topLeft(tick.x, layout.timeAxisRect.top() + 5.0);
            addTextWithFont(root, tick.label, timeLabelFont, m_theme->textColor(), transparent(), topLeft, true, false);
        }
    };

    auto appendCandlesticks = [&](QSGNode* parent, const CandlestickSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        QHash<QRgb, Vertices> wickVertices;
        QHash<QRgb, Vertices> bodyVertices;
        QHash<QRgb, Vertices> borderVertices;
        const qreal bodyWidth = candleBodyWidth(m_model.timeScale().barSpacing(), dpr);
        const qreal wickWidth = std::max(px, std::min(bodyWidth, px));
        const QVector<CandlestickDataPoint> renderPoints = series->model().conflatedOhlcPoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        for (const CandlestickDataPoint& point : renderPoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const qreal x = pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index);
            if (x < pane.dataRect.left() - bodyWidth || x > pane.dataRect.right() + bodyWidth) {
                continue;
            }

            const bool up = point.close >= point.open;
            const QColor bodyColor = colorOr(point.color, up ? series->upColor() : series->downColor());
            const QColor borderColor = colorOr(point.borderColor, up ? series->borderUpColor() : series->borderDownColor());
            const QColor wickColor = colorOr(point.wickColor, up ? series->wickUpColor() : series->wickDownColor());
            const qreal openY = priceScale.priceToCoordinate(point.open, pane.dataRect);
            const qreal closeY = priceScale.priceToCoordinate(point.close, pane.dataRect);
            const qreal highY = priceScale.priceToCoordinate(point.high, pane.dataRect);
            const qreal lowY = priceScale.priceToCoordinate(point.low, pane.dataRect);
            if (!std::isfinite(openY) || !std::isfinite(closeY) || !std::isfinite(highY) || !std::isfinite(lowY)) {
                continue;
            }

            if (series->wickVisible()) {
                appendVerticalLine(wickVertices[wickColor.rgba()], x, highY, lowY, wickWidth, dpr);
            }

            const qreal bodyTop = std::min(openY, closeY);
            const qreal bodyHeight = std::max(px, std::abs(closeY - openY));
            QRectF bodyRect(alignToDevice(x - bodyWidth * 0.5, dpr),
                alignToDevice(bodyTop, dpr),
                bodyWidth,
                bodyHeight);

            if (series->borderVisible() && bodyRect.width() > px * 2.0 && bodyRect.height() > px * 2.0) {
                appendRect(borderVertices[borderColor.rgba()], bodyRect);
                bodyRect.adjust(px, px, -px, -px);
            }
            appendRect(bodyVertices[bodyColor.rgba()], bodyRect);
        }

        for (auto it = wickVertices.cbegin(); it != wickVertices.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
        for (auto it = borderVertices.cbegin(); it != borderVertices.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
        for (auto it = bodyVertices.cbegin(); it != bodyVertices.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
    };

    auto appendBarSeries = [&](QSGNode* parent, const BarSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        QHash<QRgb, Vertices> verticesByColor;
        const qreal tickWidth = series->thinBars()
            ? std::max(px, m_model.timeScale().barSpacing() * 0.28)
            : std::max(px, m_model.timeScale().barSpacing() * 0.45);
        const qreal lineWidth = series->thinBars() ? px : std::max(px, px * 1.5);
        const QVector<CandlestickDataPoint> renderPoints = series->model().conflatedOhlcPoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        for (const CandlestickDataPoint& point : renderPoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const qreal x = pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index);
            if (x < pane.dataRect.left() - tickWidth || x > pane.dataRect.right() + tickWidth) {
                continue;
            }

            const bool up = point.close >= point.open;
            const QColor color = colorOr(point.color, up ? series->upColor() : series->downColor());
            Vertices& vertices = verticesByColor[color.rgba()];
            const qreal openY = priceScale.priceToCoordinate(point.open, pane.dataRect);
            const qreal closeY = priceScale.priceToCoordinate(point.close, pane.dataRect);
            const qreal highY = priceScale.priceToCoordinate(point.high, pane.dataRect);
            const qreal lowY = priceScale.priceToCoordinate(point.low, pane.dataRect);
            if (!std::isfinite(openY) || !std::isfinite(closeY) || !std::isfinite(highY) || !std::isfinite(lowY)) {
                continue;
            }

            appendVerticalLine(vertices, x, highY, lowY, lineWidth, dpr);
            if (series->openVisible()) {
                appendHorizontalLine(vertices, x - tickWidth, x, openY, lineWidth, dpr);
            }
            appendHorizontalLine(vertices, x, x + tickWidth, closeY, lineWidth, dpr);
        }

        for (auto it = verticesByColor.cbegin(); it != verticesByColor.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
    };

    auto appendLinePointMarkers = [&](QSGNode* parent,
                                      const QVector<LineDataPoint>& points,
                                      bool visible,
                                      qreal configuredRadius,
                                      qreal lineWidth,
                                      const ChartLayout::Pane& pane,
                                      const PriceScale& priceScale,
                                      auto colorForPoint) {
        if (!visible) {
            return;
        }

        QHash<QRgb, Vertices> verticesByColor;
        const qreal radius = resolvedPointMarkerRadius(configuredRadius, lineWidth);
        for (const LineDataPoint& point : points) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const QPointF center(pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index),
                priceScale.priceToCoordinate(point.value, pane.dataRect));
            if (!isFinitePoint(center)) {
                continue;
            }
            if (center.x() < pane.dataRect.left() - radius || center.x() > pane.dataRect.right() + radius) {
                continue;
            }

            const QColor color = colorForPoint(point);
            if (color.isValid() && color.alpha() > 0) {
                appendCircle(verticesByColor[color.rgba()], center, radius);
            }
        }

        for (auto it = verticesByColor.cbegin(); it != verticesByColor.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
    };

    auto appendCrosshairMarkerCircle = [&](QSGNode* parent,
                                           const QPointF& center,
                                           qreal radius,
                                           const QColor& seriesColor,
                                           const QColor& borderColor,
                                           qreal borderWidth,
                                           const QColor& backgroundColor) {
        if (radius <= 0.0) {
            return;
        }

        const QColor resolvedBackground = backgroundColor.isValid() ? backgroundColor : seriesColor;
        const QColor resolvedBorder = borderColor.isValid() ? borderColor : seriesColor;
        const qreal resolvedBorderWidth = std::clamp<qreal>(borderWidth, 0.0, radius);

        if (resolvedBorderWidth > 0.0 && resolvedBorder.isValid() && resolvedBorder.alpha() > 0) {
            Vertices borderVertices;
            appendCircle(borderVertices, center, radius, 24);
            if (QSGGeometryNode* node = solidTriangles(borderVertices, resolvedBorder)) {
                parent->appendChildNode(node);
            }
        }

        if (resolvedBackground.isValid() && resolvedBackground.alpha() > 0) {
            Vertices fillVertices;
            appendCircle(fillVertices, center, std::max<qreal>(0.0, radius - resolvedBorderWidth), 24);
            if (QSGGeometryNode* node = solidTriangles(fillVertices, resolvedBackground)) {
                parent->appendChildNode(node);
            }
        }
    };

    auto appendLineLikeCrosshairMarker = [&](QSGNode* parent,
                                             const QVector<LineDataPoint>& points,
                                             const ChartLayout::Pane& pane,
                                             const PriceScale& priceScale,
                                             bool visible,
                                             qreal radius,
                                             const QColor& borderColor,
                                             qreal borderWidth,
                                             const QColor& backgroundColor,
                                             auto colorForPoint) {
        if (!hasCrosshairDataPoint || !visible) {
            return;
        }

        const LineDataPoint* point = findPointByTime(points, crosshairTime);
        if (!point) {
            return;
        }

        const QPointF center(pane.dataRect.left() + m_model.timeScale().indexToCoordinate(crosshairIndex),
            priceScale.priceToCoordinate(point->value, pane.dataRect));
        if (!isFinitePoint(center)) {
            return;
        }
        const QColor seriesColor = colorForPoint(*point);
        appendCrosshairMarkerCircle(parent, center, radius, seriesColor, borderColor, borderWidth, backgroundColor);
    };

    auto appendAreaSeries = [&](QSGNode* parent, const AreaSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        struct StyledPoint {
            LineDataPoint point;
            QPointF coordinate;
        };

        QHash<QString, Vertices> fillVerticesByColor;
        QHash<QString, QVector<QColor>> fillColorsByKey;
        QVector<StrokeRun> rawLineRuns;
        QVector<StyledPoint> points;
        const qreal lineWidth = std::max(px, series->lineWidth());
        const LineType lineType = lineTypeFromInt(series->lineType());
        const LineStyle lineStyle = lineStyleFromInt(series->lineStyle());
        const qreal fillBoundaryY = series->invertFilledArea() ? pane.dataRect.top() : pane.dataRect.bottom();
        qreal gradientTop = pane.dataRect.top();
        qreal gradientBottom = pane.dataRect.bottom();
        const QVector<LineDataPoint> sourcePoints = series->model().conflatedLinePoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        auto fillKey = [](const QColor& topColor, const QColor& bottomColor) {
            return topColor.name(QColor::HexArgb) + QLatin1Char('|') + bottomColor.name(QColor::HexArgb);
        };

        if (series->relativeGradient()) {
            gradientTop = fillBoundaryY;
            gradientBottom = fillBoundaryY;
            for (const LineDataPoint& point : sourcePoints) {
                const int index = m_model.indexForTime(point.time);
                if (index < visibleFrom || index > visibleTo) {
                    continue;
                }
                const qreal y = priceScale.priceToCoordinate(point.value, pane.dataRect);
                if (!std::isfinite(y)) {
                    continue;
                }
                gradientTop = std::min(gradientTop, y);
                gradientBottom = std::max(gradientBottom, y);
            }
        }

        for (const LineDataPoint& point : sourcePoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const QPointF coordinate(pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index),
                priceScale.priceToCoordinate(point.value, pane.dataRect));
            if (isFinitePoint(coordinate)) {
                points.push_back({ point, coordinate });
            }
        }

        for (qsizetype i = 1; i < points.size(); ++i) {
            if (hasWhitespaceBetween(series->model().whitespaceTimes(), points.at(i - 1).point.time, points.at(i).point.time)) {
                continue;
            }
            Vertices segmentEndpoints;
            segmentEndpoints << points.at(i - 1).coordinate << points.at(i).coordinate;
            const Vertices path = linePathPoints(segmentEndpoints, lineType);
            const QColor topColor = colorOr(points.at(i).point.topColor, series->topColor());
            const QColor bottomColor = colorOr(points.at(i).point.bottomColor, series->bottomColor());
            const QString key = fillKey(topColor, bottomColor);
            if (!fillColorsByKey.contains(key)) {
                fillColorsByKey.insert(key, { topColor, bottomColor });
            }
            appendFillToBoundary(fillVerticesByColor[key], path, fillBoundaryY);

            if (series->lineVisible()) {
                const QColor lineColor = colorOr(points.at(i).point.lineColor, series->lineColor());
                appendStrokeRun(rawLineRuns, points.at(i - 1).coordinate, points.at(i).coordinate, lineColor);
            }
        }

        for (auto it = fillVerticesByColor.cbegin(); it != fillVerticesByColor.cend(); ++it) {
            const QVector<QColor> colors = fillColorsByKey.value(it.key());
            if (colors.size() != 2) {
                continue;
            }
            if (QSGGeometryNode* fillNode = gradientTriangles(it.value(), [&](qreal y) {
                    return colorForY(y, gradientTop, gradientBottom, colors.at(0), colors.at(1));
                })) {
                parent->appendChildNode(fillNode);
            }
        }
        if (series->lineVisible()) {
            const QVector<StrokeRun> lineRuns = linePathStrokeRuns(rawLineRuns, lineType);
            if (QSGSimpleTextureNode* lineNode = strokedLinesTextureNode(quickWindow, pane.dataRect, lineRuns, lineStyle, lineWidth, dpr)) {
                parent->appendChildNode(lineNode);
            }
        }
        appendLinePointMarkers(parent,
            sourcePoints,
            series->pointMarkersVisible(),
            series->pointMarkersRadius(),
            lineWidth,
            pane,
            priceScale,
            [series](const LineDataPoint& point) { return colorOr(point.lineColor, series->lineColor()); });
    };

    auto appendBaselineSeries = [&](QSGNode* parent, const BaselineSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        struct StyledPoint {
            LineDataPoint point;
            QPointF coordinate;
        };

        QHash<QString, Vertices> topFillVerticesByColor;
        QHash<QString, Vertices> bottomFillVerticesByColor;
        QHash<QString, QVector<QColor>> fillColorsByKey;
        QVector<StrokeRun> topLineRuns;
        QVector<StrokeRun> bottomLineRuns;
        QVector<StyledPoint> points;
        const qreal lineWidth = std::max(px, series->lineWidth());
        const LineType lineType = lineTypeFromInt(series->lineType());
        const LineStyle lineStyle = lineStyleFromInt(series->lineStyle());
        const double baseValue = series->baseValue();
        const qreal baseY = priceScale.priceToCoordinate(baseValue, pane.dataRect);
        if (!std::isfinite(baseY)) {
            return;
        }
        const QVector<LineDataPoint> sourcePoints = series->model().conflatedLinePoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        qreal topGradientTop = pane.dataRect.top();
        qreal topGradientBottom = baseY;
        qreal bottomGradientTop = baseY;
        qreal bottomGradientBottom = pane.dataRect.bottom();

        auto fillKey = [](const QColor& first, const QColor& second) {
            return first.name(QColor::HexArgb) + QLatin1Char('|') + second.name(QColor::HexArgb);
        };

        if (series->relativeGradient()) {
            topGradientTop = baseY;
            topGradientBottom = baseY;
            bottomGradientTop = baseY;
            bottomGradientBottom = baseY;
            for (const LineDataPoint& point : sourcePoints) {
                const int index = m_model.indexForTime(point.time);
                if (index < visibleFrom || index > visibleTo) {
                    continue;
                }
                const qreal y = priceScale.priceToCoordinate(point.value, pane.dataRect);
                if (!std::isfinite(y)) {
                    continue;
                }
                if (point.value >= baseValue) {
                    topGradientTop = std::min(topGradientTop, y);
                } else {
                    bottomGradientBottom = std::max(bottomGradientBottom, y);
                }
            }
        }

        auto appendBaselineFill = [&](Vertices& vertices, const QPointF& first, const QPointF& second) {
            vertices << first << QPointF(first.x(), baseY) << second;
            vertices << second << QPointF(first.x(), baseY) << QPointF(second.x(), baseY);
        };

        auto appendBaselinePart = [&](const QPointF& first, const QPointF& second, bool aboveBase, const LineDataPoint& stylePoint) {
            if (aboveBase) {
                const QColor fillColor1 = colorOr(stylePoint.topFillColor1, series->topFillColor1());
                const QColor fillColor2 = colorOr(stylePoint.topFillColor2, series->topFillColor2());
                const QString key = fillKey(fillColor1, fillColor2);
                if (!fillColorsByKey.contains(key)) {
                    fillColorsByKey.insert(key, { fillColor1, fillColor2 });
                }
                appendBaselineFill(topFillVerticesByColor[key], first, second);
            } else {
                const QColor fillColor1 = colorOr(stylePoint.bottomFillColor1, series->bottomFillColor1());
                const QColor fillColor2 = colorOr(stylePoint.bottomFillColor2, series->bottomFillColor2());
                const QString key = fillKey(fillColor1, fillColor2);
                if (!fillColorsByKey.contains(key)) {
                    fillColorsByKey.insert(key, { fillColor1, fillColor2 });
                }
                appendBaselineFill(bottomFillVerticesByColor[key], first, second);
            }

            if (!series->lineVisible()) {
                return;
            }

            const QColor lineColor = aboveBase
                ? colorOr(stylePoint.topLineColor, series->topLineColor())
                : colorOr(stylePoint.bottomLineColor, series->bottomLineColor());
            QVector<StrokeRun>& lineRuns = aboveBase ? topLineRuns : bottomLineRuns;
            appendStrokeRun(lineRuns, first, second, lineColor);
        };

        for (const LineDataPoint& point : sourcePoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const QPointF coordinate(pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index),
                priceScale.priceToCoordinate(point.value, pane.dataRect));
            if (isFinitePoint(coordinate)) {
                points.push_back({ point, coordinate });
            }
        }

        for (qsizetype pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
            if (hasWhitespaceBetween(series->model().whitespaceTimes(),
                    points.at(pointIndex - 1).point.time,
                    points.at(pointIndex).point.time)) {
                continue;
            }
            Vertices segmentEndpoints;
            segmentEndpoints << points.at(pointIndex - 1).coordinate << points.at(pointIndex).coordinate;
            const Vertices path = linePathPoints(segmentEndpoints, lineType);
            const LineDataPoint& stylePoint = points.at(pointIndex).point;
            for (qsizetype i = 1; i < path.size(); ++i) {
                const QPointF previous = path.at(i - 1);
                const QPointF current = path.at(i);
                const bool previousAbove = previous.y() <= baseY;
                const bool currentAbove = current.y() <= baseY;
                if (previousAbove == currentAbove || std::abs(previous.y() - current.y()) <= 0.0001) {
                    appendBaselinePart(previous, current, previousAbove, stylePoint);
                } else {
                    const qreal ratio = std::clamp((baseY - previous.y()) / (current.y() - previous.y()), 0.0, 1.0);
                    const QPointF crossing = previous + (current - previous) * ratio;
                    appendBaselinePart(previous, crossing, previousAbove, stylePoint);
                    appendBaselinePart(crossing, current, currentAbove, stylePoint);
                }
            }
        }

        for (auto it = topFillVerticesByColor.cbegin(); it != topFillVerticesByColor.cend(); ++it) {
            const QVector<QColor> colors = fillColorsByKey.value(it.key());
            if (colors.size() != 2) {
                continue;
            }
            if (QSGGeometryNode* fillNode = gradientTriangles(it.value(), [&](qreal y) {
                    return colorForY(y, topGradientTop, topGradientBottom, colors.at(0), colors.at(1));
                })) {
                parent->appendChildNode(fillNode);
            }
        }
        for (auto it = bottomFillVerticesByColor.cbegin(); it != bottomFillVerticesByColor.cend(); ++it) {
            const QVector<QColor> colors = fillColorsByKey.value(it.key());
            if (colors.size() != 2) {
                continue;
            }
            if (QSGGeometryNode* fillNode = gradientTriangles(it.value(), [&](qreal y) {
                    return colorForY(y, bottomGradientTop, bottomGradientBottom, colors.at(0), colors.at(1));
                })) {
                parent->appendChildNode(fillNode);
            }
        }
        if (series->lineVisible()) {
            QVector<StrokeRun> lineRuns = topLineRuns;
            lineRuns += bottomLineRuns;
            if (QSGSimpleTextureNode* lineNode = strokedLinesTextureNode(quickWindow, pane.dataRect, lineRuns, lineStyle, lineWidth, dpr)) {
                parent->appendChildNode(lineNode);
            }
        }
        appendLinePointMarkers(parent,
            sourcePoints,
            series->pointMarkersVisible(),
            series->pointMarkersRadius(),
            lineWidth,
            pane,
            priceScale,
            [series](const LineDataPoint& point) {
                return point.value >= series->baseValue()
                    ? colorOr(point.topLineColor, series->topLineColor())
                    : colorOr(point.bottomLineColor, series->bottomLineColor());
            });
    };

    auto appendHistogramSeries = [&](QSGNode* parent, const HistogramSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        QHash<QRgb, Vertices> verticesByColor;
        const qreal barWidth = std::max(px, alignToDevice(std::max<qreal>(px, m_model.timeScale().barSpacing() * 0.8), dpr));
        qreal baseY = priceScale.priceToCoordinate(series->baseValue(), pane.dataRect);
        if (!std::isfinite(baseY)) {
            baseY = pane.dataRect.bottom();
        }
        const QVector<HistogramDataPoint> renderPoints = series->model().conflatedHistogramPoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        for (const HistogramDataPoint& point : renderPoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const qreal x = pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index);
            if (x < pane.dataRect.left() - barWidth || x > pane.dataRect.right() + barWidth) {
                continue;
            }

            const qreal valueY = priceScale.priceToCoordinate(point.value, pane.dataRect);
            if (!std::isfinite(valueY)) {
                continue;
            }
            const qreal top = std::min(valueY, baseY);
            const qreal height = std::max(px, std::abs(valueY - baseY));
            const QColor color = point.color.isValid() ? point.color : series->color();
            appendRect(verticesByColor[color.rgba()],
                QRectF(alignToDevice(x - barWidth * 0.5, dpr),
                    alignToDevice(top, dpr),
                    barWidth,
                    height));
        }

        for (auto it = verticesByColor.cbegin(); it != verticesByColor.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
    };

    auto appendLineSeries = [&](QSGNode* parent, const LineSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        struct StyledPoint {
            LineDataPoint point;
            QPointF coordinate;
        };

        QVector<StrokeRun> rawLineRuns;
        QVector<StyledPoint> points;
        const qreal lineWidth = std::max(px, series->lineWidth());
        const LineType lineType = lineTypeFromInt(series->lineType());
        const LineStyle lineStyle = lineStyleFromInt(series->lineStyle());
        const QVector<LineDataPoint> sourcePoints = series->model().conflatedLinePoints(renderIndexByTime,
            visibleFrom,
            visibleTo,
            renderBarSpacing,
            series->dataConflationThreshold());

        for (const LineDataPoint& point : sourcePoints) {
            const int index = m_model.indexForTime(point.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const QPointF coordinate(pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index),
                priceScale.priceToCoordinate(point.value, pane.dataRect));
            if (isFinitePoint(coordinate)) {
                points.push_back({ point, coordinate });
            }
        }

        if (series->lineVisible()) {
            for (qsizetype i = 1; i < points.size(); ++i) {
                if (hasWhitespaceBetween(series->model().whitespaceTimes(), points.at(i - 1).point.time, points.at(i).point.time)) {
                    continue;
                }
                const QColor color = colorOr(points.at(i).point.color, series->color());
                appendStrokeRun(rawLineRuns, points.at(i - 1).coordinate, points.at(i).coordinate, color);
            }
            const QVector<StrokeRun> lineRuns = linePathStrokeRuns(rawLineRuns, lineType);
            if (QSGSimpleTextureNode* node = strokedLinesTextureNode(quickWindow, pane.dataRect, lineRuns, lineStyle, lineWidth, dpr)) {
                parent->appendChildNode(node);
            }
        }
        appendLinePointMarkers(parent,
            sourcePoints,
            series->pointMarkersVisible(),
            series->pointMarkersRadius(),
            lineWidth,
            pane,
            priceScale,
            [series](const LineDataPoint& point) { return colorOr(point.color, series->color()); });
    };

    auto seriesDefaultColor = [this](const ChartSeries* series) -> QColor {
        if (const auto* candleSeries = qobject_cast<const CandlestickSeries*>(series)) {
            return candleSeries->upColor();
        }
        if (const auto* barSeries = qobject_cast<const BarSeries*>(series)) {
            return barSeries->upColor();
        }
        if (const auto* areaSeries = qobject_cast<const AreaSeries*>(series)) {
            return areaSeries->lineColor();
        }
        if (const auto* baselineSeries = qobject_cast<const BaselineSeries*>(series)) {
            return baselineSeries->topLineColor();
        }
        if (const auto* histogramSeries = qobject_cast<const HistogramSeries*>(series)) {
            return histogramSeries->color();
        }
        if (const auto* lineSeries = qobject_cast<const LineSeries*>(series)) {
            return lineSeries->color();
        }
        return m_theme->textColor();
    };

    auto lastSeriesPrice = [&](const ChartSeries* series, PriceLineSource source) -> std::optional<double> {
        if (!series) {
            return std::nullopt;
        }

        auto acceptsIndex = [&](TimePoint time) {
            if (source == PriceLineSource::LastBar) {
                return true;
            }
            const int index = m_model.indexForTime(time);
            return index >= visibleFrom && index <= visibleTo;
        };

        if (qobject_cast<const CandlestickSeries*>(series) || qobject_cast<const BarSeries*>(series)) {
            const QVector<CandlestickDataPoint>& points = series->model().candlesticks();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time)) {
                    return it->close;
                }
            }
            return std::nullopt;
        }

        if (qobject_cast<const LineSeries*>(series) || qobject_cast<const AreaSeries*>(series) || qobject_cast<const BaselineSeries*>(series)) {
            const QVector<LineDataPoint>& points = series->model().linePoints();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time)) {
                    return it->value;
                }
            }
            return std::nullopt;
        }

        if (qobject_cast<const HistogramSeries*>(series)) {
            const QVector<HistogramDataPoint>& points = series->model().histogramPoints();
            for (auto it = points.crbegin(); it != points.crend(); ++it) {
                if (acceptsIndex(it->time)) {
                    return it->value;
                }
            }
        }
        return std::nullopt;
    };

    auto baseLinePrice = [](const ChartSeries* series) {
        if (const auto* baselineSeries = qobject_cast<const BaselineSeries*>(series)) {
            return baselineSeries->baseValue();
        }
        if (const auto* histogramSeries = qobject_cast<const HistogramSeries*>(series)) {
            return histogramSeries->baseValue();
        }
        return 0.0;
    };

    auto appendHorizontalPriceLine = [&](QSGNode* parent,
                                         const ChartSeries* series,
                                         const ChartLayout::Pane& pane,
                                         const PriceScale& priceScale,
                                         double price,
                                         const QColor& requestedColor,
                                         qreal requestedWidth,
                                         int requestedStyle,
                                         bool lineVisible,
                                         bool axisLabelVisible,
                                         const QString& title,
                                         const QColor& labelTextColor,
                                         const QColor& labelBackgroundColor) {
        if (!std::isfinite(price)) {
            return;
        }

        const qreal y = priceScale.priceToCoordinate(price, pane.dataRect);
        if (!std::isfinite(y)) {
            return;
        }
        if (y < pane.dataRect.top() - 24.0 || y > pane.dataRect.bottom() + 24.0) {
            return;
        }

        const QColor color = colorOr(requestedColor, seriesDefaultColor(series));
        if (lineVisible && requestedWidth > 0.0) {
            Vertices vertices;
            appendStyledHorizontalLine(vertices,
                pane.dataRect.left(),
                pane.dataRect.right(),
                y,
                std::max(px, requestedWidth),
                dpr,
                lineStyleFromInt(requestedStyle));
            if (QSGGeometryNode* node = solidTriangles(vertices, color)) {
                parent->appendChildNode(node);
            }
        }

        if (!title.isEmpty()) {
            addText(parent,
                title,
                color,
                transparent(),
                QPointF(pane.dataRect.left() + 6.0, y - 16.0),
                false,
                false);
        }

        const QString axisScaleId = series ? series->priceScaleId() : QStringLiteral("right");
        const QRectF axisRect = priceAxisRectForScale(pane, axisScaleId);
        if (axisLabelVisible && priceAxisVisibleForScale(axisScaleId) && axisRect.width() > 0.0) {
            const bool leftAxis = isLeftPriceScale(normalizedPriceScaleId(axisScaleId));
            const QString label = renderPriceLabel(priceScale.priceToDisplayValue(price), displayPriceFormat());
            const QColor labelText = colorOr(labelTextColor, m_theme->crosshairLabelText());
            const QColor labelBackground = colorOr(labelBackgroundColor, color);
            const QSizeF labelSize = measureText(label, labelText, labelBackground);
            const QPointF labelTopLeft = priceAxisLabelTopLeft(axisRect, labelSize, y, leftAxis);
            addText(root,
                label,
                labelText,
                labelBackground,
                QPointF(labelTopLeft.x(), y),
                false,
                true);
        }
    };

    auto appendSeriesPriceLines = [&](QSGNode* parent, const ChartSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        if (!series) {
            return;
        }

        for (const SeriesPriceLine& line : series->customPriceLines()) {
            appendHorizontalPriceLine(parent,
                series,
                pane,
                priceScale,
                line.price,
                line.color,
                line.lineWidth,
                static_cast<int>(line.lineStyle),
                line.lineVisible,
                line.axisLabelVisible,
                line.title,
                line.axisLabelTextColor,
                line.axisLabelBackgroundColor);
        }

        if (series->baseLineVisible()) {
            appendHorizontalPriceLine(parent,
                series,
                pane,
                priceScale,
                baseLinePrice(series),
                series->baseLineColor(),
                series->baseLineWidth(),
                series->baseLineStyle(),
                true,
                false,
                QString(),
                QColor(),
                QColor());
        }

        const std::optional<double> lastPrice = lastSeriesPrice(series, static_cast<PriceLineSource>(series->priceLineSource()));
        if (!lastPrice.has_value()) {
            return;
        }

        appendHorizontalPriceLine(parent,
            series,
            pane,
            priceScale,
            lastPrice.value(),
            series->priceLineColor(),
            series->priceLineWidth(),
            series->priceLineStyle(),
            series->priceLineVisible(),
            series->lastValueLabelVisible(),
            QString(),
            QColor(),
            QColor());
    };

    QHash<int, QVector<QRectF>> markerTextRectsByPane;
    auto appendMarkers = [&](QSGNode* parent, const ChartSeries* series, const ChartLayout::Pane& pane, const PriceScale& priceScale) {
        QHash<QRgb, Vertices> markerVertices;
        QVector<QRectF>& occupiedTextRects = markerTextRectsByPane[pane.index];
        for (const SeriesMarker& marker : series->model().markers()) {
            const int index = m_model.indexForTime(marker.time);
            if (index < visibleFrom || index > visibleTo) {
                continue;
            }

            const std::optional<double> price = isAtPricePosition(marker.position) && std::isfinite(marker.price)
                ? std::optional<double>(marker.price)
                : series->model().priceAt(marker.time, marker.position);
            if (!price.has_value()) {
                continue;
            }

            const qreal x = pane.dataRect.left() + m_model.timeScale().indexToCoordinate(index);
            qreal y = priceScale.priceToCoordinate(price.value(), pane.dataRect);
            if (!std::isfinite(y)) {
                continue;
            }
            const qreal radius = 4.5 * marker.size;
            if (marker.position == MarkerPosition::AboveBar) {
                y -= 12.0;
            } else if (marker.position == MarkerPosition::BelowBar) {
                y += 12.0;
            } else if (marker.position == MarkerPosition::AtPriceTop) {
                y += radius;
            } else if (marker.position == MarkerPosition::AtPriceBottom) {
                y -= radius;
            }

            Vertices& vertices = markerVertices[marker.color.rgba()];
            switch (marker.shape) {
            case MarkerShape::ArrowUp:
                appendTriangle(vertices, QPointF(x, y - radius), QPointF(x - radius, y + radius), QPointF(x + radius, y + radius));
                break;
            case MarkerShape::ArrowDown:
                appendTriangle(vertices, QPointF(x, y + radius), QPointF(x - radius, y - radius), QPointF(x + radius, y - radius));
                break;
            case MarkerShape::Square:
                appendRect(vertices, QRectF(x - radius, y - radius, radius * 2.0, radius * 2.0));
                break;
            case MarkerShape::Circle:
                appendCircle(vertices, QPointF(x, y), radius);
                break;
            }

            if (!marker.text.isEmpty()) {
                const QSizeF labelSize = measureText(marker.text, marker.color, transparent());
                if (!labelSize.isEmpty()) {
                    const bool belowMarker = marker.position == MarkerPosition::BelowBar
                        || marker.position == MarkerPosition::AtPriceTop;
                    const qreal maxLeft = std::max(pane.dataRect.left(), pane.dataRect.right() - labelSize.width());
                    const qreal maxTop = std::max(pane.dataRect.top(), pane.dataRect.bottom() - labelSize.height());
                    const qreal labelLeft = std::clamp(x - labelSize.width() * 0.5,
                        pane.dataRect.left(),
                        maxLeft);
                    const qreal desiredTop = belowMarker
                        ? y + radius + 2.5
                        : y - radius - labelSize.height() - 2.5;
                    const qreal labelTop = std::clamp(desiredTop, pane.dataRect.top(), maxTop);
                    const QRectF labelRect(QPointF(labelLeft, labelTop), labelSize);
                    const QRectF collisionRect = labelRect.adjusted(-2.0, -2.0, 2.0, 2.0);
                    const bool collides = std::any_of(occupiedTextRects.cbegin(), occupiedTextRects.cend(), [&collisionRect](const QRectF& occupied) {
                        return occupied.intersects(collisionRect);
                    });
                    if (!collides) {
                        addText(parent, marker.text, marker.color, transparent(), labelRect.topLeft(), false, false);
                        occupiedTextRects.push_back(collisionRect);
                    }
                }
            }
        }

        for (auto it = markerVertices.cbegin(); it != markerVertices.cend(); ++it) {
            if (QSGGeometryNode* node = solidTriangles(it.value(), QColor::fromRgba(it.key()))) {
                parent->appendChildNode(node);
            }
        }
    };

    for (qsizetype panePosition = 0; panePosition < layout.panes.size(); ++panePosition) {
        const ChartLayout::Pane& pane = layout.panes.at(panePosition);
        const PaneAxisTicks paneTicks = m_renderState.ticksByPane.value(panePosition);
        const QVector<PriceTick>& gridPriceTicks = m_priceScaleVisible ? paneTicks.right : paneTicks.left;
        const PriceScale gridPriceScale = renderPriceScale(pane.index, m_priceScaleVisible ? QStringLiteral("right") : QStringLiteral("left"));

        if (panePosition > 0) {
            const int separatorIndex = layout.panes.at(panePosition - 1).index;
            const bool activeSeparator = separatorIndex == m_hoveredPaneSeparator
                || separatorIndex == m_draggedPaneSeparator;
            const QColor separatorColor = activeSeparator && m_theme->paneSeparatorHoverColor().isValid()
                ? m_theme->paneSeparatorHoverColor()
                : m_theme->paneSeparatorColor();
            root->appendChildNode(new QSGSimpleRectNode(QRectF(0.0, pane.dataRect.top() - px,
                                                            itemSize.width(), px),
                separatorColor));
        }

        auto* clipNode = new QSGClipNode;
        clipNode->setClipRect(pane.dataRect);
        clipNode->setIsRectangular(true);
        root->appendChildNode(clipNode);

        if (m_gridVisible) {
            Vertices gridVertices;
            if (m_verticalGridVisible) {
                const LineStyle verticalStyle = lineStyleFromInt(m_verticalGridLineStyle);
                for (const TimeTick& tick : timeTicks) {
                    appendStyledVerticalLine(gridVertices, tick.x, pane.dataRect.top(), pane.dataRect.bottom(), px, dpr, verticalStyle);
                }
            }
            if (m_horizontalGridVisible) {
                const LineStyle horizontalStyle = lineStyleFromInt(m_horizontalGridLineStyle);
                for (const PriceTick& tick : gridPriceTicks) {
                    appendStyledHorizontalLine(gridVertices, pane.dataRect.left(), pane.dataRect.right(), tick.y, px, dpr, horizontalStyle);
                }
            }
            if (QSGGeometryNode* node = solidTriangles(gridVertices, m_theme->gridColor())) {
                clipNode->appendChildNode(node);
            }
        }

        if (gridPriceScale.hasTransformedBaseLine()) {
            const qreal y = gridPriceScale.priceToCoordinate(gridPriceScale.baseValue(), pane.dataRect);
            if (std::isfinite(y) && y >= pane.dataRect.top() - 1.0 && y <= pane.dataRect.bottom() + 1.0) {
                Vertices baseLineVertices;
                appendStyledHorizontalLine(baseLineVertices,
                    pane.dataRect.left(),
                    pane.dataRect.right(),
                    y,
                    px,
                    dpr,
                    LineStyle::Dashed);
                if (QSGGeometryNode* node = solidTriangles(baseLineVertices, m_theme->priceAxisBorderColor())) {
                    clipNode->appendChildNode(node);
                }
            }
        }

        for (ChartSeries* chartSeries : m_series) {
            if (!chartSeries || !chartSeries->isVisible() || chartSeries->pane() != pane.index) {
                continue;
            }

            const PriceScale seriesPriceScale = renderPriceScale(pane.index, chartSeries->priceScaleId());
            if (const auto* candleSeries = qobject_cast<const CandlestickSeries*>(chartSeries)) {
                appendCandlesticks(clipNode, candleSeries, pane, seriesPriceScale);
            } else if (const auto* barSeries = qobject_cast<const BarSeries*>(chartSeries)) {
                appendBarSeries(clipNode, barSeries, pane, seriesPriceScale);
            } else if (const auto* areaSeries = qobject_cast<const AreaSeries*>(chartSeries)) {
                appendAreaSeries(clipNode, areaSeries, pane, seriesPriceScale);
            } else if (const auto* baselineSeries = qobject_cast<const BaselineSeries*>(chartSeries)) {
                appendBaselineSeries(clipNode, baselineSeries, pane, seriesPriceScale);
            } else if (const auto* histogramSeries = qobject_cast<const HistogramSeries*>(chartSeries)) {
                appendHistogramSeries(clipNode, histogramSeries, pane, seriesPriceScale);
            } else if (const auto* lineSeries = qobject_cast<const LineSeries*>(chartSeries)) {
                appendLineSeries(clipNode, lineSeries, pane, seriesPriceScale);
            }
            appendSeriesPriceLines(clipNode, chartSeries, pane, seriesPriceScale);
            appendMarkers(clipNode, chartSeries, pane, seriesPriceScale);
        }

        for (ChartSeries* chartSeries : m_series) {
            if (!chartSeries || !chartSeries->isVisible() || chartSeries->pane() != pane.index) {
                continue;
            }

            const PriceScale seriesPriceScale = renderPriceScale(pane.index, chartSeries->priceScaleId());
            if (const auto* areaSeries = qobject_cast<const AreaSeries*>(chartSeries)) {
                appendLineLikeCrosshairMarker(clipNode,
                    areaSeries->model().linePoints(),
                    pane,
                    seriesPriceScale,
                    areaSeries->crosshairMarkerVisible(),
                    areaSeries->crosshairMarkerRadius(),
                    areaSeries->crosshairMarkerBorderColor(),
                    areaSeries->crosshairMarkerBorderWidth(),
                    areaSeries->crosshairMarkerBackgroundColor(),
                    [areaSeries](const LineDataPoint& point) { return colorOr(point.lineColor, areaSeries->lineColor()); });
            } else if (const auto* baselineSeries = qobject_cast<const BaselineSeries*>(chartSeries)) {
                appendLineLikeCrosshairMarker(clipNode,
                    baselineSeries->model().linePoints(),
                    pane,
                    seriesPriceScale,
                    baselineSeries->crosshairMarkerVisible(),
                    baselineSeries->crosshairMarkerRadius(),
                    baselineSeries->crosshairMarkerBorderColor(),
                    baselineSeries->crosshairMarkerBorderWidth(),
                    baselineSeries->crosshairMarkerBackgroundColor(),
                    [baselineSeries](const LineDataPoint& point) {
                        return point.value >= baselineSeries->baseValue()
                            ? colorOr(point.topLineColor, baselineSeries->topLineColor())
                            : colorOr(point.bottomLineColor, baselineSeries->bottomLineColor());
                    });
            } else if (const auto* lineSeries = qobject_cast<const LineSeries*>(chartSeries)) {
                appendLineLikeCrosshairMarker(clipNode,
                    lineSeries->model().linePoints(),
                    pane,
                    seriesPriceScale,
                    lineSeries->crosshairMarkerVisible(),
                    lineSeries->crosshairMarkerRadius(),
                    lineSeries->crosshairMarkerBorderColor(),
                    lineSeries->crosshairMarkerBorderWidth(),
                    lineSeries->crosshairMarkerBackgroundColor(),
                    [lineSeries](const LineDataPoint& point) { return colorOr(point.color, lineSeries->color()); });
            }
        }
    }

    if (crosshairDrawable() && contains(m_crosshairPosition)) {
        const int activePaneIndex = paneAtY(m_crosshairPosition.y(), layout);
        const qreal x = std::clamp(m_crosshairPosition.x(), layout.panes.first().dataRect.left(), layout.panes.first().dataRect.right());
        if (m_verticalCrosshairLineVisible && m_verticalCrosshairLineWidth > 0.0) {
            Vertices verticalVertices;
            appendStyledVerticalLine(verticalVertices,
                x,
                layout.panes.first().dataRect.top(),
                layout.panes.last().dataRect.bottom(),
                std::max(px, m_verticalCrosshairLineWidth),
                dpr,
                lineStyleFromInt(m_verticalCrosshairLineStyle));
            const QColor color = colorOr(m_verticalCrosshairLineColor, m_theme->crosshairColor());
            if (QSGGeometryNode* node = solidTriangles(verticalVertices, color)) {
                root->appendChildNode(node);
            }
        }
        for (const ChartLayout::Pane& pane : layout.panes) {
            if (pane.index == activePaneIndex) {
                const qreal y = std::clamp(m_crosshairPosition.y(), pane.dataRect.top(), pane.dataRect.bottom());
                if (m_horizontalCrosshairLineVisible && m_horizontalCrosshairLineWidth > 0.0) {
                    Vertices horizontalVertices;
                    appendStyledHorizontalLine(horizontalVertices,
                        pane.dataRect.left(),
                        pane.dataRect.right(),
                        y,
                        std::max(px, m_horizontalCrosshairLineWidth),
                        dpr,
                        lineStyleFromInt(m_horizontalCrosshairLineStyle));
                    const QColor color = colorOr(m_horizontalCrosshairLineColor, m_theme->crosshairColor());
                    if (QSGGeometryNode* node = solidTriangles(horizontalVertices, color)) {
                        root->appendChildNode(node);
                    }
                }
                break;
            }
        }
    }

    for (qsizetype panePosition = 0; panePosition < layout.panes.size(); ++panePosition) {
        const PaneAxisTicks paneTicks = m_renderState.ticksByPane.value(panePosition);
        appendPriceAxis(layout.panes.at(panePosition), paneTicks.left, true);
        appendPriceAxis(layout.panes.at(panePosition), paneTicks.right, false);
    }
    if (m_timeScaleVisible) {
        appendTimeAxis();
    }

    if (crosshairDrawable() && contains(m_crosshairPosition) && !m_model.timePoints().isEmpty()) {
        if (m_timeScaleVisible && m_timeCrosshairLabelVisible && !m_renderState.crosshairTimeLabel.isEmpty()) {
            const qreal x = std::clamp(m_crosshairPosition.x(), layout.timeAxisRect.left(), layout.timeAxisRect.right());
            addText(root,
                m_renderState.crosshairTimeLabel,
                m_theme->crosshairLabelText(),
                colorOr(m_timeCrosshairLabelBackground, m_theme->crosshairLabelBackground()),
                QPointF(x, layout.timeAxisRect.top() + 4.0),
                true,
                false);
        }

        if ((m_priceScaleVisible || m_leftPriceScaleVisible) && m_priceCrosshairLabelVisible && !m_renderState.crosshairPriceLabel.isEmpty()) {
            for (const ChartLayout::Pane& pane : layout.panes) {
                if (pane.index != m_renderState.crosshairPane) {
                    continue;
                }
                const qreal y = std::clamp(m_crosshairPosition.y(), pane.dataRect.top(), pane.dataRect.bottom());
                const QString crosshairScaleId = normalizedPriceScaleId(m_renderState.crosshairPriceScaleId);
                const bool leftAxis = isLeftPriceScale(crosshairScaleId);
                const bool axisVisible = priceAxisVisibleForScale(crosshairScaleId);
                if (!axisVisible) {
                    break;
                }
                const QRectF axisRect = priceAxisRectForScale(pane, crosshairScaleId);
                const QColor labelText = m_theme->crosshairLabelText();
                const QColor labelBackground = colorOr(m_priceCrosshairLabelBackground, m_theme->crosshairLabelBackground());
                const QSizeF labelSize = measureText(m_renderState.crosshairPriceLabel, labelText, labelBackground);
                const QPointF labelTopLeft = priceAxisLabelTopLeft(axisRect, labelSize, y, leftAxis);
                addText(root,
                    m_renderState.crosshairPriceLabel,
                    labelText,
                    labelBackground,
                    QPointF(labelTopLeft.x(), y),
                    false,
                    true);
                break;
            }
        }
    }

    return root;
}

bool FinancialChartItem::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchUpdate
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchCancel) {
        auto* touch = static_cast<QTouchEvent*>(event);
        if (!m_interactive
            || !m_handleScroll
            || (!m_horizontalTouchDrag && !m_verticalTouchDrag)
            || touch->points().isEmpty()) {
            return QQuickItem::event(event);
        }

        const QPointF point = touch->points().first().position();
        const QString payloadType = event->type() == QEvent::TouchBegin
            ? QStringLiteral("touchBegin")
            : (event->type() == QEvent::TouchUpdate
                      ? QStringLiteral("touchUpdate")
                      : (event->type() == QEvent::TouchEnd ? QStringLiteral("touchEnd") : QStringLiteral("touchCancel")));

        if (event->type() == QEvent::TouchBegin) {
            const ChartLayout layout = currentLayout();
            m_touchDragging = touch->points().size() == 1;
            m_touchDragPane = m_touchDragging ? paneAtY(point.y(), layout) : -1;
            m_touchDragPriceScaleId = m_touchDragging ? touchPriceScaleIdAt(point, layout) : QStringLiteral("right");
            m_lastTouchPosition = point;
            m_pressPosition = point;
            m_dragMoved = false;
            updateCrosshair(point, touchEventPayload(touch, payloadType, point));
            touch->accept();
            return true;
        }

        if (event->type() == QEvent::TouchCancel || event->type() == QEvent::TouchEnd) {
            if (m_touchDragging) {
                updateCrosshair(point, touchEventPayload(touch, payloadType, point));
            }
            m_touchDragging = false;
            m_touchDragPane = -1;
            m_touchDragPriceScaleId = QStringLiteral("right");
            m_dragMoved = false;
            touch->accept();
            return true;
        }

        if (!m_touchDragging || touch->points().size() != 1) {
            return QQuickItem::event(event);
        }

        const QPointF delta = point - m_lastTouchPosition;
        bool horizontalChanged = false;
        bool verticalChanged = false;

        if (m_horizontalTouchDrag && !qFuzzyIsNull(delta.x())) {
            m_model.timeScale().panByPixels(delta.x());
            m_fitContentRequested = false;
            horizontalChanged = true;
        }

        if (m_verticalTouchDrag && !qFuzzyIsNull(delta.y()) && m_touchDragPane >= 0) {
            const ChartLayout layout = currentLayout();
            const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
            for (const ChartLayout::Pane& pane : layout.panes) {
                if (pane.index != m_touchDragPane || pane.dataRect.height() <= 1.0) {
                    continue;
                }

                PriceScale priceScale = m_priceScales.value(priceScaleKey(pane.index, m_touchDragPriceScaleId));
                configurePriceScale(priceScale, pane.index, m_touchDragPriceScaleId, visibleFrom, visibleTo);
                const PriceRange range = priceScale.range();
                const double previousPrice = priceScale.coordinateToPrice(m_lastTouchPosition.y(), pane.dataRect);
                const double currentPrice = priceScale.coordinateToPrice(point.y(), pane.dataRect);
                const double deltaPrice = previousPrice - currentPrice;
                if (range.isValid() && std::isfinite(deltaPrice) && std::abs(deltaPrice) > 1e-12) {
                    m_manualPriceRanges.insert(priceScaleKey(pane.index, normalizedPriceScaleId(m_touchDragPriceScaleId)),
                        { range.min + deltaPrice, range.max + deltaPrice });
                    verticalChanged = true;
                }
                break;
            }
        }

        if (horizontalChanged || verticalChanged) {
            m_dragMoved = m_dragMoved
                || std::hypot(point.x() - m_pressPosition.x(), point.y() - m_pressPosition.y()) > 3.0;
            scheduleVisualUpdate();
            if (horizontalChanged) {
                Q_EMIT barSpacingChanged();
                Q_EMIT rightOffsetChanged();
            }
            if (verticalChanged) {
                Q_EMIT axisOptionsChanged();
            }
            emitVisibleRangeSignals();
        }

        m_lastTouchPosition = point;
        updateCrosshair(point, touchEventPayload(touch, payloadType, point));
        touch->accept();
        return true;
    }

    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (!m_interactive || !m_handleScale || !m_pinchScale) {
            return QQuickItem::event(event);
        }

        switch (gesture->gestureType()) {
        case Qt::ZoomNativeGesture: {
            // macOS trackpad pinch reports small positive/negative deltas.
            const qreal factor = std::exp(gesture->value());
            if (zoomAtItemPosition(gesture->position().x(), factor)) {
                updateCrosshair(gesture->position(), QVariantMap {
                                                         { QStringLiteral("type"), QStringLiteral("nativeGesture") },
                                                         { QStringLiteral("point"), pointPayload(gesture->position()) },
                                                         { QStringLiteral("gestureType"), static_cast<int>(gesture->gestureType()) },
                                                         { QStringLiteral("value"), gesture->value() },
                                                     });
                gesture->accept();
                return true;
            }
            break;
        }
        case Qt::SmartZoomNativeGesture:
            fitContent();
            gesture->accept();
            return true;
        default:
            break;
        }
    }

    return QQuickItem::event(event);
}

void FinancialChartItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())
        || !qFuzzyCompare(newGeometry.height(), oldGeometry.height())) {
        const ChartLayout layout = currentLayoutWithSyncedTimeScale();
        scheduleVisualUpdate();
        Q_EMIT timeScaleSizeChanged(layout.timeAxisRect.width(), layout.timeAxisRect.height());
        emitVisibleRangeSignals();
    }
}

void FinancialChartItem::hoverMoveEvent(QHoverEvent* event)
{
    if (!m_interactive) {
        unsetCursor();
        return;
    }

    const ChartLayout layout = currentLayout();
    const int hoveredSeparator = paneSeparatorAt(event->position(), layout);
    if (m_hoveredPaneSeparator != hoveredSeparator) {
        m_hoveredPaneSeparator = hoveredSeparator;
        scheduleVisualUpdate();
    }
    if (hoveredSeparator >= 0) {
        setCursor(QCursor(Qt::SizeVerCursor));
        updateCrosshair(event->position(), hoverEventPayload(event, QStringLiteral("hover")));
        return;
    }

    const QVariantMap hitTest = hitTestPayload(event->position());
    bool okCursor = false;
    const int cursorShape = hitTest.value(QStringLiteral("cursorShape")).toInt(&okCursor);
    if (okCursor && cursorShape >= static_cast<int>(Qt::ArrowCursor) && cursorShape <= static_cast<int>(Qt::LastCursor)) {
        setCursor(QCursor(static_cast<Qt::CursorShape>(cursorShape)));
    } else if (m_handleScale) {
        if (m_timeScaleVisible && layout.timeAxisRect.contains(event->position())) {
            setCursor(QCursor(Qt::SizeHorCursor));
        } else {
            bool overPriceAxis = false;
            if (m_priceScaleVisible || m_leftPriceScaleVisible) {
                for (const ChartLayout::Pane& pane : layout.panes) {
                    if ((m_priceScaleVisible && pane.rightPriceAxisRect.contains(event->position()))
                        || (m_leftPriceScaleVisible && pane.leftPriceAxisRect.contains(event->position()))) {
                        overPriceAxis = true;
                        break;
                    }
                }
            }
            if (overPriceAxis) {
                setCursor(QCursor(Qt::SizeVerCursor));
            } else {
                unsetCursor();
            }
        }
    } else {
        unsetCursor();
    }
    updateCrosshair(event->position(), hoverEventPayload(event, QStringLiteral("hover")));
}

void FinancialChartItem::hoverLeaveEvent(QHoverEvent* event)
{
    QQuickItem::hoverLeaveEvent(event);
    unsetCursor();
    if (m_hoveredPaneSeparator >= 0) {
        m_hoveredPaneSeparator = -1;
    }
    m_crosshairVisible = false;
    m_crosshairPriceScaleId = QStringLiteral("right");
    scheduleVisualUpdate();
    Q_EMIT crosshairMoved(QVariantMap());
}

void FinancialChartItem::mousePressEvent(QMouseEvent* event)
{
    if (!m_interactive || event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }

    m_axisDragMode = AxisDragMode::None;
    m_axisDragPane = -1;
    m_axisDragPriceScaleId = QStringLiteral("right");
    m_draggedPaneSeparator = -1;
    m_dragMoved = false;
    m_pressPosition = event->position();
    m_lastDragX = event->position().x();

    const ChartLayout layout = currentLayout();
    const int separator = paneSeparatorAt(event->position(), layout);
    if (separator >= 0) {
        for (qsizetype i = 0; i + 1 < layout.panes.size(); ++i) {
            const ChartLayout::Pane& upper = layout.panes.at(i);
            const ChartLayout::Pane& lower = layout.panes.at(i + 1);
            if (upper.index != separator) {
                continue;
            }
            m_draggedPaneSeparator = separator;
            m_hoveredPaneSeparator = separator;
            m_separatorDragUpperHeight = upper.dataRect.height();
            m_separatorDragLowerHeight = lower.dataRect.height();
            setCursor(QCursor(Qt::SizeVerCursor));
            event->accept();
            return;
        }
    }

    if (m_handleScale) {
        if (m_timeScaleVisible && layout.timeAxisRect.contains(event->position())) {
            m_axisDragMode = AxisDragMode::Time;
            m_axisDragStartBarSpacing = m_model.timeScale().barSpacing();
        } else if (m_priceScaleVisible || m_leftPriceScaleVisible) {
            const auto [visibleFrom, visibleTo] = m_model.timeScale().visibleIndexRange(2);
            for (const ChartLayout::Pane& pane : layout.panes) {
                QString priceScaleId;
                if (m_priceScaleVisible && pane.rightPriceAxisRect.contains(event->position())) {
                    priceScaleId = QStringLiteral("right");
                } else if (m_leftPriceScaleVisible && pane.leftPriceAxisRect.contains(event->position())) {
                    priceScaleId = QStringLiteral("left");
                }
                if (priceScaleId.isEmpty()) {
                    continue;
                }
                PriceScale priceScale = m_priceScales.value(priceScaleKey(pane.index, priceScaleId));
                configurePriceScale(priceScale, pane.index, priceScaleId, visibleFrom, visibleTo);
                m_axisDragStartRange = priceScale.range();
                if (!m_axisDragStartRange.isValid()) {
                    break;
                }
                m_axisDragMode = AxisDragMode::Price;
                m_axisDragPane = pane.index;
                m_axisDragPriceScaleId = priceScaleId;
                m_axisDragAnchorPrice = priceScale.coordinateToPrice(event->position().y(), pane.dataRect);
                break;
            }
        }
    }

    m_dragging = m_axisDragMode == AxisDragMode::None && m_handleScroll && m_pressedMouseMoveScroll;
    updateCrosshair(event->position(), mouseEventPayload(event, QStringLiteral("mousePress")));
    event->accept();
}

void FinancialChartItem::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_interactive) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    if (m_draggedPaneSeparator >= 0 && event->buttons().testFlag(Qt::LeftButton)) {
        const ChartLayout layout = currentLayout();
        for (qsizetype i = 0; i + 1 < layout.panes.size(); ++i) {
            const ChartLayout::Pane& upper = layout.panes.at(i);
            const ChartLayout::Pane& lower = layout.panes.at(i + 1);
            if (upper.index != m_draggedPaneSeparator) {
                continue;
            }

            constexpr qreal minimumPaneHeight = 32.0;
            const qreal totalHeight = m_separatorDragUpperHeight + m_separatorDragLowerHeight;
            if (totalHeight < minimumPaneHeight * 2.0) {
                break;
            }

            const qreal delta = event->position().y() - m_pressPosition.y();
            const qreal upperHeight = std::clamp(m_separatorDragUpperHeight + delta,
                minimumPaneHeight,
                totalHeight - minimumPaneHeight);
            m_model.setPaneHeight(upper.index, upperHeight);
            m_model.setPaneHeight(lower.index, totalHeight - upperHeight);
            m_dragMoved = m_dragMoved || std::abs(delta) > 3.0;
            scheduleVisualUpdate();
            event->accept();
            return;
        }
    } else if (m_axisDragMode == AxisDragMode::Time && event->buttons().testFlag(Qt::LeftButton)) {
        const ChartLayout layout = currentLayout();
        if (!layout.panes.isEmpty() && layout.panes.first().dataRect.width() > 1.0) {
            const QRectF dataRect = layout.panes.first().dataRect;
            const qreal localX = std::clamp(m_pressPosition.x() - dataRect.left(), 0.0, dataRect.width());
            const qreal delta = event->position().x() - m_pressPosition.x();
            const qreal factor = std::pow(1.01, std::clamp<qreal>(delta, -300.0, 300.0));
            const qreal currentSpacing = std::max<qreal>(0.1, m_model.timeScale().barSpacing());
            const qreal targetSpacing = std::max<qreal>(0.1, m_axisDragStartBarSpacing * factor);
            m_model.timeScale().setWidth(dataRect.width());
            m_model.timeScale().zoomAt(localX, targetSpacing / currentSpacing);
            m_dragMoved = m_dragMoved || std::abs(delta) > 3.0;
            m_fitContentRequested = false;
            Q_EMIT barSpacingChanged();
            Q_EMIT rightOffsetChanged();
            emitVisibleRangeSignals();
        }
    } else if (m_axisDragMode == AxisDragMode::Price && event->buttons().testFlag(Qt::LeftButton)) {
        if (m_axisDragPane >= 0 && m_axisDragStartRange.isValid() && m_axisDragStartRange.span() > 0.0) {
            const qreal delta = event->position().y() - m_pressPosition.y();
            const double factor = std::pow(1.01, std::clamp<qreal>(delta, -300.0, 300.0));
            const double startSpan = std::max(m_axisDragStartRange.span(), 1e-9);
            const double nextSpan = std::max(startSpan * factor, 1e-9);
            const double anchor = std::isfinite(m_axisDragAnchorPrice)
                ? m_axisDragAnchorPrice
                : (m_axisDragStartRange.min + m_axisDragStartRange.max) * 0.5;
            const double anchorRatio = std::clamp((anchor - m_axisDragStartRange.min) / startSpan, 0.0, 1.0);
            const double nextMin = anchor - anchorRatio * nextSpan;
            m_manualPriceRanges.insert(priceScaleKey(m_axisDragPane, normalizedPriceScaleId(m_axisDragPriceScaleId)),
                { nextMin, nextMin + nextSpan });
            m_dragMoved = m_dragMoved || std::abs(delta) > 3.0;
            scheduleVisualUpdate();
            Q_EMIT axisOptionsChanged();
        }
    } else if (m_dragging && m_handleScroll && m_pressedMouseMoveScroll) {
        const qreal x = event->position().x();
        m_model.timeScale().panByPixels(x - m_lastDragX);
        m_dragMoved = m_dragMoved || std::abs(x - m_pressPosition.x()) > 3.0;
        m_lastDragX = x;
        m_fitContentRequested = false;
        Q_EMIT barSpacingChanged();
        Q_EMIT rightOffsetChanged();
        emitVisibleRangeSignals();
    } else if (event->buttons().testFlag(Qt::LeftButton)) {
        m_dragMoved = m_dragMoved || std::hypot(event->position().x() - m_pressPosition.x(), event->position().y() - m_pressPosition.y()) > 3.0;
    }
    updateCrosshair(event->position(), mouseEventPayload(event, QStringLiteral("mouseMove")));
    event->accept();
}

void FinancialChartItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!m_dragMoved) {
            const CrosshairResolution resolution = resolvedCrosshair(event->position());
            Q_EMIT clicked(crosshairPayload(resolution.point,
                mouseEventPayload(event, QStringLiteral("click")),
                resolution.priceScaleId));
        }
        m_axisDragMode = AxisDragMode::None;
        m_axisDragPane = -1;
        m_axisDragPriceScaleId = QStringLiteral("right");
        m_draggedPaneSeparator = -1;
        m_dragging = false;
        m_dragMoved = false;
        scheduleVisualUpdate();
        event->accept();
        return;
    }
    QQuickItem::mouseReleaseEvent(event);
}

void FinancialChartItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QVariantMap resetPayload;
        if (m_interactive) {
            const ChartLayout layout = currentLayout();
            const int separator = paneSeparatorAt(event->position(), layout);
            if (separator >= 0) {
                for (qsizetype i = 0; i + 1 < layout.panes.size(); ++i) {
                    const ChartLayout::Pane& upper = layout.panes.at(i);
                    const ChartLayout::Pane& lower = layout.panes.at(i + 1);
                    if (upper.index != separator) {
                        continue;
                    }
                    m_model.clearPaneHeight(upper.index);
                    m_model.clearPaneHeight(lower.index);
                    scheduleDataUpdate();
                    resetPayload.insert(QStringLiteral("separator"), separator);
                    resetPayload.insert(QStringLiteral("upperPane"), upper.index);
                    resetPayload.insert(QStringLiteral("lowerPane"), lower.index);
                    break;
                }
            } else if (m_handleScale && m_timeScaleVisible && layout.timeAxisRect.contains(event->position())) {
                resetTimeScale();
                resetPayload.insert(QStringLiteral("axis"), QStringLiteral("time"));
            } else if (m_handleScale && (m_priceScaleVisible || m_leftPriceScaleVisible)) {
                for (const ChartLayout::Pane& pane : layout.panes) {
                    QString priceScaleId;
                    if (m_priceScaleVisible && pane.rightPriceAxisRect.contains(event->position())) {
                        priceScaleId = QStringLiteral("right");
                    } else if (m_leftPriceScaleVisible && pane.leftPriceAxisRect.contains(event->position())) {
                        priceScaleId = QStringLiteral("left");
                    }
                    if (priceScaleId.isEmpty()) {
                        continue;
                    }
                    clearVisiblePriceRangeForScale(priceScaleId, pane.index);
                    resetPayload.insert(QStringLiteral("axis"), QStringLiteral("price"));
                    resetPayload.insert(QStringLiteral("pane"), pane.index);
                    resetPayload.insert(QStringLiteral("priceScaleId"), priceScaleId);
                    break;
                }
            }
        }

        updateCrosshair(event->position(), mouseEventPayload(event, QStringLiteral("doubleClick")));
        QVariantMap payload = crosshairPayload(m_crosshairPosition,
            mouseEventPayload(event, QStringLiteral("doubleClick")),
            m_crosshairPriceScaleId);
        if (!resetPayload.isEmpty()) {
            payload.insert(QStringLiteral("axisReset"), resetPayload);
        }
        Q_EMIT doubleClicked(payload);
        event->accept();
        return;
    }
    QQuickItem::mouseDoubleClickEvent(event);
}

void FinancialChartItem::wheelEvent(QWheelEvent* event)
{
    if (!m_interactive || (!m_mouseWheelScale && !m_mouseWheelScroll)) {
        QQuickItem::wheelEvent(event);
        return;
    }

    qreal delta = 0.0;
    bool angleDelta = false;
    if (!event->pixelDelta().isNull()) {
        delta = event->pixelDelta().x() != 0
            ? static_cast<qreal>(event->pixelDelta().x())
            : static_cast<qreal>(event->pixelDelta().y());
    } else if (!event->angleDelta().isNull()) {
        delta = event->angleDelta().x() != 0
            ? static_cast<qreal>(event->angleDelta().x()) / 8.0
            : static_cast<qreal>(event->angleDelta().y()) / 8.0;
        angleDelta = true;
    }

    if (qFuzzyIsNull(delta)) {
        QQuickItem::wheelEvent(event);
        return;
    }

    if (event->inverted()) {
        delta = -delta;
    }

    const qreal steps = angleDelta
        ? delta / 15.0
        : delta / 120.0;

    const bool modifierRequestsScale = event->modifiers().testFlag(Qt::ControlModifier)
        || event->modifiers().testFlag(Qt::MetaModifier);
    const bool shouldScale = m_handleScale && m_mouseWheelScale && (!m_mouseWheelScroll || modifierRequestsScale);
    if (shouldScale) {
        const qreal factor = std::pow(1.10, std::clamp<qreal>(steps, -6.0, 6.0));
        if (zoomAtItemPosition(event->position().x(), factor)) {
            updateCrosshair(event->position(), wheelEventPayload(event));
            event->accept();
            return;
        }
    }

    if (m_handleScroll && m_mouseWheelScroll) {
        m_model.timeScale().panByPixels(delta);
        m_fitContentRequested = false;
        Q_EMIT barSpacingChanged();
        Q_EMIT rightOffsetChanged();
        emitVisibleRangeSignals();
        updateCrosshair(event->position(), wheelEventPayload(event));
        event->accept();
        return;
    }

    QQuickItem::wheelEvent(event);
}

} // namespace QtFinCharts
