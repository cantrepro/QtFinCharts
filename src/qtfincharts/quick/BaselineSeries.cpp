#include "BaselineSeries.h"

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

BaselineSeries::BaselineSeries(QObject* parent)
    : ChartSeries(SeriesType::Baseline, parent)
{
    model().setBaselineBaseValue(m_baseValue);
}

void BaselineSeries::setBaseValue(double value)
{
    if (!std::isfinite(value) || qFuzzyCompare(m_baseValue, value)) {
        return;
    }
    m_baseValue = value;
    model().setBaselineBaseValue(m_baseValue);
    notifyOptionsChanged();
}

void BaselineSeries::setRelativeGradient(bool relative)
{
    if (m_relativeGradient == relative) {
        return;
    }
    m_relativeGradient = relative;
    notifyOptionsChanged();
}

void BaselineSeries::setTopFillColor1(const QColor& color)
{
    if (m_topFillColor1 == color) {
        return;
    }
    m_topFillColor1 = color;
    notifyOptionsChanged();
}

void BaselineSeries::setTopFillColor2(const QColor& color)
{
    if (m_topFillColor2 == color) {
        return;
    }
    m_topFillColor2 = color;
    notifyOptionsChanged();
}

void BaselineSeries::setTopLineColor(const QColor& color)
{
    if (m_topLineColor == color) {
        return;
    }
    m_topLineColor = color;
    notifyOptionsChanged();
}

void BaselineSeries::setBottomFillColor1(const QColor& color)
{
    if (m_bottomFillColor1 == color) {
        return;
    }
    m_bottomFillColor1 = color;
    notifyOptionsChanged();
}

void BaselineSeries::setBottomFillColor2(const QColor& color)
{
    if (m_bottomFillColor2 == color) {
        return;
    }
    m_bottomFillColor2 = color;
    notifyOptionsChanged();
}

void BaselineSeries::setBottomLineColor(const QColor& color)
{
    if (m_bottomLineColor == color) {
        return;
    }
    m_bottomLineColor = color;
    notifyOptionsChanged();
}

void BaselineSeries::setLineStyle(int style)
{
    const int normalized = std::clamp(style, static_cast<int>(LineStyle::Solid), static_cast<int>(LineStyle::SparseDotted));
    if (m_lineStyle == normalized) {
        return;
    }
    m_lineStyle = normalized;
    notifyOptionsChanged();
}

void BaselineSeries::setLineType(int type)
{
    const int normalized = std::clamp(type, static_cast<int>(LineType::Simple), static_cast<int>(LineType::Curved));
    if (m_lineType == normalized) {
        return;
    }
    m_lineType = normalized;
    notifyOptionsChanged();
}

void BaselineSeries::setLineWidth(qreal width)
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

void BaselineSeries::setLineVisible(bool visible)
{
    if (m_lineVisible == visible) {
        return;
    }
    m_lineVisible = visible;
    notifyOptionsChanged();
}

void BaselineSeries::setPointMarkersVisible(bool visible)
{
    if (m_pointMarkersVisible == visible) {
        return;
    }
    m_pointMarkersVisible = visible;
    notifyOptionsChanged();
}

void BaselineSeries::setPointMarkersRadius(qreal radius)
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

void BaselineSeries::setCrosshairMarkerVisible(bool visible)
{
    if (m_crosshairMarkerVisible == visible) {
        return;
    }
    m_crosshairMarkerVisible = visible;
    notifyOptionsChanged();
}

void BaselineSeries::setCrosshairMarkerRadius(qreal radius)
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

void BaselineSeries::setCrosshairMarkerBorderColor(const QColor& color)
{
    if (m_crosshairMarkerBorderColor == color) {
        return;
    }
    m_crosshairMarkerBorderColor = color;
    notifyOptionsChanged();
}

void BaselineSeries::setCrosshairMarkerBorderWidth(qreal width)
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

void BaselineSeries::setCrosshairMarkerBackgroundColor(const QColor& color)
{
    if (m_crosshairMarkerBackgroundColor == color) {
        return;
    }
    m_crosshairMarkerBackgroundColor = color;
    notifyOptionsChanged();
}

void BaselineSeries::setData(const QVariantList& rows)
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

        qWarning() << "QtFinCharts: skipped invalid baseline row" << rowValue;
        continue;
    }

    model().setBaselineBaseValue(m_baseValue);
    model().setLinePoints(std::move(parsed), SeriesType::Baseline, std::move(whitespaceTimes));
    model().setOriginalDataRows(std::move(dataRows));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), model().linePoints().size());
    payload.insert(QStringLiteral("whitespaceCount"), model().whitespaceTimes().size());
    notifyDataChanged(QStringLiteral("full"), payload);
}

void BaselineSeries::update(const QVariantMap& row, bool historicalUpdate)
{
    LineDataPoint point;
    if (parseRow(row, &point)) {
        if (!shouldAcceptUpdate(point.time, historicalUpdate)) {
            return;
        }
        model().setBaselineBaseValue(m_baseValue);
        const bool inserted = model().updateLinePoint(point, SeriesType::Baseline);
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
        qWarning() << "QtFinCharts: skipped invalid baseline update" << row;
        return;
    }
    if (!shouldAcceptUpdate(whitespaceTime, historicalUpdate)) {
        return;
    }
    model().setBaselineBaseValue(m_baseValue);
    const bool inserted = model().updateWhitespace(whitespaceTime, SeriesType::Baseline);
    model().updateOriginalDataRow({ whitespaceTime, row });
    QVariantMap payload;
    payload.insert(QStringLiteral("time"), whitespaceTime);
    payload.insert(QStringLiteral("inserted"), inserted);
    payload.insert(QStringLiteral("whitespace"), true);
    payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
    notifyDataChanged(QStringLiteral("update"), payload);
}

bool BaselineSeries::parseRow(const QVariantMap& row, LineDataPoint* point)
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
        QColor(),
        QColor(),
        QColor(),
        colorFromVariant(row.value(QStringLiteral("topFillColor1")), QColor()),
        colorFromVariant(row.value(QStringLiteral("topFillColor2")), QColor()),
        colorFromVariant(row.value(QStringLiteral("topLineColor")), QColor()),
        colorFromVariant(row.value(QStringLiteral("bottomFillColor1")), QColor()),
        colorFromVariant(row.value(QStringLiteral("bottomFillColor2")), QColor()),
        colorFromVariant(row.value(QStringLiteral("bottomLineColor")), QColor()),
    };
    return true;
}

} // namespace QtFinCharts
