#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QMetaType>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>
#include <QtGui/QColor>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtFinCharts {

using TimePoint = qint64;

enum class SeriesType {
    Candlestick,
    Line,
    Bar,
    Area,
    Baseline,
    Histogram,
};

enum class LineStyle {
    Solid = 0,
    Dotted = 1,
    Dashed = 2,
    LargeDashed = 3,
    SparseDotted = 4,
};

enum class LineType {
    Simple = 0,
    WithSteps = 1,
    Curved = 2,
};

enum class PriceFormat {
    Price = 0,
    Volume = 1,
    Percent = 2,
};

enum class PriceScaleMode {
    Normal = 0,
    Logarithmic = 1,
    Percentage = 2,
    IndexedTo100 = 3,
};

enum class CrosshairMode {
    Normal = 0,
    Magnet = 1,
    MagnetOHLC = 2,
    Hidden = 3,
};

enum class PriceLineSource {
    LastBar = 0,
    LastVisible = 1,
};

enum class MarkerPosition {
    AboveBar,
    BelowBar,
    InBar,
    AtPriceTop,
    AtPriceMiddle,
    AtPriceBottom,
};

enum class MarkerShape {
    Circle,
    ArrowUp,
    ArrowDown,
    Square,
};

struct LogicalRange {
    double from = 0.0;
    double to = -1.0;

    [[nodiscard]] bool isValid() const noexcept { return from <= to; }
};

struct PriceRange {
    double min = std::numeric_limits<double>::quiet_NaN();
    double max = std::numeric_limits<double>::quiet_NaN();

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite(min) && std::isfinite(max) && min <= max;
    }

    [[nodiscard]] double span() const noexcept { return max - min; }

    void include(double value) noexcept
    {
        if (!std::isfinite(value)) {
            return;
        }

        if (!isValid()) {
            min = value;
            max = value;
            return;
        }

        min = std::min(min, value);
        max = std::max(max, value);
    }

    void include(const PriceRange& other) noexcept
    {
        if (!other.isValid()) {
            return;
        }
        include(other.min);
        include(other.max);
    }

    [[nodiscard]] PriceRange padded(double fraction, double fallbackSpan = 1.0) const noexcept
    {
        if (!isValid()) {
            return { -fallbackSpan * 0.5, fallbackSpan * 0.5 };
        }

        const double currentSpan = span();
        const double pad = currentSpan > 0.0 ? currentSpan * fraction : fallbackSpan * fraction;
        const double center = (min + max) * 0.5;
        if (currentSpan <= 0.0) {
            return { center - fallbackSpan * 0.5, center + fallbackSpan * 0.5 };
        }
        return { min - pad, max + pad };
    }
};

struct CandlestickDataPoint {
    TimePoint time = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    QColor color;
    QColor borderColor;
    QColor wickColor;
};

struct LineDataPoint {
    TimePoint time = 0;
    double value = 0.0;
    QColor color;
    QColor lineColor;
    QColor topColor;
    QColor bottomColor;
    QColor topFillColor1;
    QColor topFillColor2;
    QColor topLineColor;
    QColor bottomFillColor1;
    QColor bottomFillColor2;
    QColor bottomLineColor;
};

struct HistogramDataPoint {
    TimePoint time = 0;
    double value = 0.0;
    QColor color;
};

struct SeriesDataRow {
    TimePoint time = 0;
    QVariantMap row;
};

struct SeriesMarker {
    QString id;
    TimePoint time = 0;
    MarkerPosition position = MarkerPosition::InBar;
    MarkerShape shape = MarkerShape::Circle;
    QColor color = QColor(41, 98, 255);
    qreal size = 1.0;
    double price = std::numeric_limits<double>::quiet_NaN();
    QString text;
    int cursorShape = -1;
};

struct SeriesPriceLine {
    QString id;
    double price = std::numeric_limits<double>::quiet_NaN();
    QColor color;
    qreal lineWidth = 1.0;
    LineStyle lineStyle = LineStyle::Solid;
    bool lineVisible = true;
    bool axisLabelVisible = true;
    QString title;
    QColor axisLabelTextColor;
    QColor axisLabelBackgroundColor;
    int cursorShape = -1;
};

struct TimeTick {
    int index = 0;
    TimePoint time = 0;
    qreal x = 0.0;
    QString label;
};

struct PriceTick {
    double price = 0.0;
    qreal y = 0.0;
    QString label;
};

struct ChartLayout {
    struct Pane {
        int index = 0;
        QRectF leftPriceAxisRect;
        QRectF dataRect;
        QRectF rightPriceAxisRect;
        QRectF priceAxisRect;
    };

    QVector<Pane> panes;
    QRectF timeAxisRect;
    qreal leftPriceAxisWidth = 0.0;
    qreal rightPriceAxisWidth = 72.0;
    qreal priceAxisWidth = 72.0;
    qreal timeAxisHeight = 28.0;
};

} // namespace QtFinCharts

Q_DECLARE_METATYPE(QtFinCharts::SeriesType)
Q_DECLARE_METATYPE(QtFinCharts::LineStyle)
Q_DECLARE_METATYPE(QtFinCharts::LineType)
Q_DECLARE_METATYPE(QtFinCharts::PriceFormat)
Q_DECLARE_METATYPE(QtFinCharts::PriceScaleMode)
Q_DECLARE_METATYPE(QtFinCharts::CrosshairMode)
Q_DECLARE_METATYPE(QtFinCharts::PriceLineSource)
Q_DECLARE_METATYPE(QtFinCharts::MarkerPosition)
Q_DECLARE_METATYPE(QtFinCharts::MarkerShape)
