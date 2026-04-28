#include "BarSeries.h"

#include <QtCore/QDebug>

namespace QtFinCharts {

namespace {

    bool isWhitespaceRow(const QVariantMap& row)
    {
        return row.contains(QStringLiteral("time"))
            && !row.contains(QStringLiteral("open"))
            && !row.contains(QStringLiteral("high"))
            && !row.contains(QStringLiteral("low"))
            && !row.contains(QStringLiteral("close"));
    }

} // namespace

BarSeries::BarSeries(QObject* parent)
    : ChartSeries(SeriesType::Bar, parent)
{
}

void BarSeries::setUpColor(const QColor& color)
{
    if (m_upColor == color) {
        return;
    }
    m_upColor = color;
    notifyOptionsChanged();
}

void BarSeries::setDownColor(const QColor& color)
{
    if (m_downColor == color) {
        return;
    }
    m_downColor = color;
    notifyOptionsChanged();
}

void BarSeries::setOpenVisible(bool visible)
{
    if (m_openVisible == visible) {
        return;
    }
    m_openVisible = visible;
    notifyOptionsChanged();
}

void BarSeries::setThinBars(bool thinBars)
{
    if (m_thinBars == thinBars) {
        return;
    }
    m_thinBars = thinBars;
    notifyOptionsChanged();
}

void BarSeries::setData(const QVariantList& rows)
{
    QVector<CandlestickDataPoint> parsed;
    QVector<TimePoint> whitespaceTimes;
    QVector<SeriesDataRow> dataRows;
    parsed.reserve(rows.size());
    dataRows.reserve(rows.size());
    for (const QVariant& rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        CandlestickDataPoint point;
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

        qWarning() << "QtFinCharts: skipped invalid bar row" << rowValue;
        continue;
    }

    model().setOhlcPoints(std::move(parsed), SeriesType::Bar, std::move(whitespaceTimes));
    model().setOriginalDataRows(std::move(dataRows));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), model().candlesticks().size());
    payload.insert(QStringLiteral("whitespaceCount"), model().whitespaceTimes().size());
    notifyDataChanged(QStringLiteral("full"), payload);
}

void BarSeries::update(const QVariantMap& row, bool historicalUpdate)
{
    CandlestickDataPoint point;
    if (parseRow(row, &point)) {
        if (!shouldAcceptUpdate(point.time, historicalUpdate)) {
            return;
        }
        const bool inserted = model().updateOhlcPoint(point, SeriesType::Bar);
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
        qWarning() << "QtFinCharts: skipped invalid bar update" << row;
        return;
    }
    if (!shouldAcceptUpdate(whitespaceTime, historicalUpdate)) {
        return;
    }
    const bool inserted = model().updateWhitespace(whitespaceTime, SeriesType::Bar);
    model().updateOriginalDataRow({ whitespaceTime, row });
    QVariantMap payload;
    payload.insert(QStringLiteral("time"), whitespaceTime);
    payload.insert(QStringLiteral("inserted"), inserted);
    payload.insert(QStringLiteral("whitespace"), true);
    payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
    notifyDataChanged(QStringLiteral("update"), payload);
}

bool BarSeries::parseRow(const QVariantMap& row, CandlestickDataPoint* point)
{
    if (!point) {
        return false;
    }

    TimePoint time = 0;
    if (!parseTime(row.value(QStringLiteral("time")), &time)) {
        return false;
    }

    const std::optional<double> open = parseFiniteDouble(row.value(QStringLiteral("open")));
    const std::optional<double> high = parseFiniteDouble(row.value(QStringLiteral("high")));
    const std::optional<double> low = parseFiniteDouble(row.value(QStringLiteral("low")));
    const std::optional<double> close = parseFiniteDouble(row.value(QStringLiteral("close")));
    if (!open.has_value() || !high.has_value() || !low.has_value() || !close.has_value()) {
        return false;
    }

    *point = {
        time,
        open.value(),
        high.value(),
        low.value(),
        close.value(),
        colorFromVariant(row.value(QStringLiteral("color")), QColor()),
    };
    return true;
}

} // namespace QtFinCharts
