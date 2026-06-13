#include "HistogramSeries.h"

#include <QtCore/QDebug>

#include <cmath>

namespace QtFinCharts {

namespace {

    bool isWhitespaceRow(const QVariantMap& row)
    {
        return row.contains(QStringLiteral("time")) && !row.contains(QStringLiteral("value"));
    }

} // namespace

HistogramSeries::HistogramSeries(QObject* parent)
    : ChartSeries(SeriesType::Histogram, parent)
{
    model().setHistogramBaseValue(m_baseValue);
}

void HistogramSeries::setColor(const QColor& color)
{
    if (m_color == color) {
        return;
    }
    m_color = color;
    notifyOptionsChanged();
}

void HistogramSeries::setBaseValue(double baseValue)
{
    if (!std::isfinite(baseValue) || qFuzzyCompare(m_baseValue, baseValue)) {
        return;
    }
    m_baseValue = baseValue;
    model().setHistogramBaseValue(m_baseValue);
    notifyOptionsChanged();
}

void HistogramSeries::setData(const QVariantList& rows)
{
    QVector<HistogramDataPoint> parsed;
    QVector<TimePoint> whitespaceTimes;
    QVector<SeriesDataRow> dataRows;
    parsed.reserve(rows.size());
    dataRows.reserve(rows.size());
    for (const QVariant& rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        HistogramDataPoint point;
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

        qWarning() << "QtFinCharts: skipped invalid histogram row" << rowValue;
        continue;
    }

    model().setHistogramBaseValue(m_baseValue);
    model().setHistogramPoints(std::move(parsed), std::move(whitespaceTimes));
    model().setOriginalDataRows(std::move(dataRows));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), model().histogramPoints().size());
    payload.insert(QStringLiteral("whitespaceCount"), model().whitespaceTimes().size());
    notifyDataChanged(QStringLiteral("full"), payload);
}

void HistogramSeries::update(const QVariantMap& row, bool historicalUpdate)
{
    HistogramDataPoint point;
    if (parseRow(row, &point)) {
        if (!shouldAcceptUpdate(point.time, historicalUpdate)) {
            return;
        }
        model().setHistogramBaseValue(m_baseValue);
        const bool inserted = model().updateHistogramPoint(point);
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
        qWarning() << "QtFinCharts: skipped invalid histogram update" << row;
        return;
    }
    if (!shouldAcceptUpdate(whitespaceTime, historicalUpdate)) {
        return;
    }
    model().setHistogramBaseValue(m_baseValue);
    const bool inserted = model().updateWhitespace(whitespaceTime, SeriesType::Histogram);
    model().updateOriginalDataRow({ whitespaceTime, row });
    QVariantMap payload;
    payload.insert(QStringLiteral("time"), whitespaceTime);
    payload.insert(QStringLiteral("inserted"), inserted);
    payload.insert(QStringLiteral("whitespace"), true);
    payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
    notifyDataChanged(QStringLiteral("update"), payload);
}

bool HistogramSeries::parseRow(const QVariantMap& row, HistogramDataPoint* point)
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

    *point = { time, value.value(), colorFromVariant(row.value(QStringLiteral("color")), QColor()) };
    return true;
}

} // namespace QtFinCharts
