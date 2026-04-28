#include "AreaSeries.h"

#include <QtCore/QDebug>

#include <algorithm>
#include <cmath>

namespace QtFinCharts {

namespace {

    bool isWhitespaceRow(const QVariantMap& row)
    {
        return row.contains(QStringLiteral("time")) && !row.contains(QStringLiteral("value"));
    }

} // namespace

AreaSeries::AreaSeries(QObject* parent)
    : ChartSeries(SeriesType::Area, parent)
{
}

void AreaSeries::setTopColor(const QColor& color)
{
    if (m_topColor == color) {
        return;
    }
    m_topColor = color;
    notifyOptionsChanged();
}

void AreaSeries::setBottomColor(const QColor& color)
{
    if (m_bottomColor == color) {
        return;
    }
    m_bottomColor = color;
    notifyOptionsChanged();
}

void AreaSeries::setRelativeGradient(bool relative)
{
    if (m_relativeGradient == relative) {
        return;
    }
    m_relativeGradient = relative;
    notifyOptionsChanged();
}

void AreaSeries::setInvertFilledArea(bool invert)
{
    if (m_invertFilledArea == invert) {
        return;
    }
    m_invertFilledArea = invert;
    notifyOptionsChanged();
}

void AreaSeries::setLineColor(const QColor& color)
{
    if (m_lineColor == color) {
        return;
    }
    m_lineColor = color;
    notifyOptionsChanged();
}

void AreaSeries::setLineStyle(int style)
{
    const int normalized = std::clamp(style, static_cast<int>(LineStyle::Solid), static_cast<int>(LineStyle::SparseDotted));
    if (m_lineStyle == normalized) {
        return;
    }
    m_lineStyle = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setLineType(int type)
{
    const int normalized = std::clamp(type, static_cast<int>(LineType::Simple), static_cast<int>(LineType::Curved));
    if (m_lineType == normalized) {
        return;
    }
    m_lineType = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 1.0, 8.0);
    if (qFuzzyCompare(m_lineWidth, normalized)) {
        return;
    }
    m_lineWidth = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setLineVisible(bool visible)
{
    if (m_lineVisible == visible) {
        return;
    }
    m_lineVisible = visible;
    notifyOptionsChanged();
}

void AreaSeries::setPointMarkersVisible(bool visible)
{
    if (m_pointMarkersVisible == visible) {
        return;
    }
    m_pointMarkersVisible = visible;
    notifyOptionsChanged();
}

void AreaSeries::setPointMarkersRadius(qreal radius)
{
    if (!std::isfinite(radius)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(radius, 0.0, 64.0);
    if (qFuzzyCompare(m_pointMarkersRadius, normalized)) {
        return;
    }
    m_pointMarkersRadius = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setCrosshairMarkerVisible(bool visible)
{
    if (m_crosshairMarkerVisible == visible) {
        return;
    }
    m_crosshairMarkerVisible = visible;
    notifyOptionsChanged();
}

void AreaSeries::setCrosshairMarkerRadius(qreal radius)
{
    if (!std::isfinite(radius)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(radius, 0.0, 64.0);
    if (qFuzzyCompare(m_crosshairMarkerRadius, normalized)) {
        return;
    }
    m_crosshairMarkerRadius = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setCrosshairMarkerBorderColor(const QColor& color)
{
    if (m_crosshairMarkerBorderColor == color) {
        return;
    }
    m_crosshairMarkerBorderColor = color;
    notifyOptionsChanged();
}

void AreaSeries::setCrosshairMarkerBorderWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 16.0);
    if (qFuzzyCompare(m_crosshairMarkerBorderWidth, normalized)) {
        return;
    }
    m_crosshairMarkerBorderWidth = normalized;
    notifyOptionsChanged();
}

void AreaSeries::setCrosshairMarkerBackgroundColor(const QColor& color)
{
    if (m_crosshairMarkerBackgroundColor == color) {
        return;
    }
    m_crosshairMarkerBackgroundColor = color;
    notifyOptionsChanged();
}

void AreaSeries::setData(const QVariantList& rows)
{
    QVector<LineDataPoint> parsed;
    QVector<TimePoint> whitespaceTimes;
    QVector<SeriesDataRow> dataRows;
    parsed.reserve(rows.size());
    dataRows.reserve(rows.size());
    for (const QVariant& rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        LineDataPoint point;
        if (parseRow(row, &point)) {
            parsed.push_back(point);
            dataRows.push_back({ point.time, row });
            continue;
        }

        TimePoint whitespaceTime = 0;
        if (isWhitespaceRow(row) && parseTime(row.value(QStringLiteral("time")), &whitespaceTime)) {
            whitespaceTimes.push_back(whitespaceTime);
            dataRows.push_back({ whitespaceTime, row });
            continue;
        }

        qWarning() << "QtFinCharts: skipped invalid area row" << rowValue;
        continue;
    }

    model().setLinePoints(std::move(parsed), SeriesType::Area, std::move(whitespaceTimes));
    model().setOriginalDataRows(std::move(dataRows));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), model().linePoints().size());
    payload.insert(QStringLiteral("whitespaceCount"), model().whitespaceTimes().size());
    notifyDataChanged(QStringLiteral("full"), payload);
}

void AreaSeries::update(const QVariantMap& row, bool historicalUpdate)
{
    LineDataPoint point;
    if (parseRow(row, &point)) {
        if (!shouldAcceptUpdate(point.time, historicalUpdate)) {
            return;
        }
        const bool inserted = model().updateLinePoint(point, SeriesType::Area);
        model().updateOriginalDataRow({ point.time, row });
        QVariantMap payload;
        payload.insert(QStringLiteral("time"), point.time);
        payload.insert(QStringLiteral("inserted"), inserted);
        payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
        notifyDataChanged(QStringLiteral("update"), payload);
        return;
    }

    TimePoint whitespaceTime = 0;
    if (!isWhitespaceRow(row) || !parseTime(row.value(QStringLiteral("time")), &whitespaceTime)) {
        qWarning() << "QtFinCharts: skipped invalid area update" << row;
        return;
    }
    if (!shouldAcceptUpdate(whitespaceTime, historicalUpdate)) {
        return;
    }
    const bool inserted = model().updateWhitespace(whitespaceTime, SeriesType::Area);
    model().updateOriginalDataRow({ whitespaceTime, row });
    QVariantMap payload;
    payload.insert(QStringLiteral("time"), whitespaceTime);
    payload.insert(QStringLiteral("inserted"), inserted);
    payload.insert(QStringLiteral("whitespace"), true);
    payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
    notifyDataChanged(QStringLiteral("update"), payload);
}

bool AreaSeries::parseRow(const QVariantMap& row, LineDataPoint* point)
{
    if (!point) {
        return false;
    }

    TimePoint time = 0;
    if (!parseTime(row.value(QStringLiteral("time")), &time)) {
        return false;
    }

    const std::optional<double> value = parseFiniteDouble(row.value(QStringLiteral("value")));
    if (!value.has_value()) {
        return false;
    }

    *point = {
        time,
        value.value(),
        QColor(),
        colorFromVariant(row.value(QStringLiteral("lineColor")), QColor()),
        colorFromVariant(row.value(QStringLiteral("topColor")), QColor()),
        colorFromVariant(row.value(QStringLiteral("bottomColor")), QColor()),
    };
    return true;
}

} // namespace QtFinCharts
