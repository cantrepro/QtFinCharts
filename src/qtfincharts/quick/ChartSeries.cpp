#include "ChartSeries.h"

#include "ColorParsing.h"
#include "FinancialChartItem.h"

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QMetaType>
#include <QtCore/QTime>
#include <QtCore/QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace QtFinCharts {

namespace {

    MarkerPosition markerPositionFromString(const QString& value)
    {
        if (value.compare(QStringLiteral("aboveBar"), Qt::CaseInsensitive) == 0) {
            return MarkerPosition::AboveBar;
        }
        if (value.compare(QStringLiteral("belowBar"), Qt::CaseInsensitive) == 0) {
            return MarkerPosition::BelowBar;
        }
        if (value.compare(QStringLiteral("atPriceTop"), Qt::CaseInsensitive) == 0) {
            return MarkerPosition::AtPriceTop;
        }
        if (value.compare(QStringLiteral("atPriceMiddle"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("atPrice"), Qt::CaseInsensitive) == 0) {
            return MarkerPosition::AtPriceMiddle;
        }
        if (value.compare(QStringLiteral("atPriceBottom"), Qt::CaseInsensitive) == 0) {
            return MarkerPosition::AtPriceBottom;
        }
        return MarkerPosition::InBar;
    }

    MarkerShape markerShapeFromString(const QString& value)
    {
        if (value.compare(QStringLiteral("arrowUp"), Qt::CaseInsensitive) == 0) {
            return MarkerShape::ArrowUp;
        }
        if (value.compare(QStringLiteral("arrowDown"), Qt::CaseInsensitive) == 0) {
            return MarkerShape::ArrowDown;
        }
        if (value.compare(QStringLiteral("square"), Qt::CaseInsensitive) == 0) {
            return MarkerShape::Square;
        }
        return MarkerShape::Circle;
    }

    int cursorShapeFromVariant(const QVariant& value, int fallback = -1)
    {
        if (!value.isValid()) {
            return fallback;
        }

        bool ok = false;
        const int numeric = value.toInt(&ok);
        if (ok && numeric >= static_cast<int>(Qt::ArrowCursor) && numeric <= static_cast<int>(Qt::LastCursor)) {
            return numeric;
        }

        QString key = value.toString().trimmed().toLower();
        key.remove(QLatin1Char('-'));
        key.remove(QLatin1Char('_'));
        key.remove(QLatin1Char(' '));
        if (key.endsWith(QStringLiteral("cursor"))) {
            key.chop(6);
        }

        if (key == QStringLiteral("arrow")) {
            return static_cast<int>(Qt::ArrowCursor);
        }
        if (key == QStringLiteral("cross") || key == QStringLiteral("crosshair")) {
            return static_cast<int>(Qt::CrossCursor);
        }
        if (key == QStringLiteral("pointinghand") || key == QStringLiteral("pointer") || key == QStringLiteral("hand")) {
            return static_cast<int>(Qt::PointingHandCursor);
        }
        if (key == QStringLiteral("openhand") || key == QStringLiteral("grab")) {
            return static_cast<int>(Qt::OpenHandCursor);
        }
        if (key == QStringLiteral("closedhand") || key == QStringLiteral("grabbing")) {
            return static_cast<int>(Qt::ClosedHandCursor);
        }
        if (key == QStringLiteral("sizever") || key == QStringLiteral("resizevertical") || key == QStringLiteral("nsresize")) {
            return static_cast<int>(Qt::SizeVerCursor);
        }
        if (key == QStringLiteral("sizehor") || key == QStringLiteral("resizehorizontal") || key == QStringLiteral("ewresize")) {
            return static_cast<int>(Qt::SizeHorCursor);
        }
        if (key == QStringLiteral("sizeall") || key == QStringLiteral("move")) {
            return static_cast<int>(Qt::SizeAllCursor);
        }
        if (key == QStringLiteral("ibeam") || key == QStringLiteral("text")) {
            return static_cast<int>(Qt::IBeamCursor);
        }
        if (key == QStringLiteral("forbidden") || key == QStringLiteral("notallowed")) {
            return static_cast<int>(Qt::ForbiddenCursor);
        }
        if (key == QStringLiteral("wait")) {
            return static_cast<int>(Qt::WaitCursor);
        }
        if (key == QStringLiteral("busy")) {
            return static_cast<int>(Qt::BusyCursor);
        }
        if (key == QStringLiteral("whatsthis") || key == QStringLiteral("help")) {
            return static_cast<int>(Qt::WhatsThisCursor);
        }
        return fallback;
    }

    TimePoint dateToUtcTimePoint(const QDate& date)
    {
        return QDateTime(date, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    }

    QString seriesTypeName(SeriesType type)
    {
        switch (type) {
        case SeriesType::Candlestick:
            return QStringLiteral("candlestick");
        case SeriesType::Line:
            return QStringLiteral("line");
        case SeriesType::Bar:
            return QStringLiteral("bar");
        case SeriesType::Area:
            return QStringLiteral("area");
        case SeriesType::Baseline:
            return QStringLiteral("baseline");
        case SeriesType::Histogram:
            return QStringLiteral("histogram");
        }
        return QStringLiteral("unknown");
    }

} // namespace

ChartSeries::ChartSeries(SeriesType type, QObject* parent)
    : QObject(parent)
    , m_model(type)
{
}

void ChartSeries::setName(const QString& name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    notifyOptionsChanged();
}

void ChartSeries::setTitle(const QString& title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    notifyOptionsChanged();
}

void ChartSeries::setPriceScaleId(const QString& id)
{
    const QString trimmed = id.trimmed();
    const QString normalized = trimmed.isEmpty() ? QStringLiteral("right") : trimmed;
    if (m_priceScaleId == normalized) {
        return;
    }
    m_priceScaleId = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setPane(int pane)
{
    const int normalized = std::max(0, pane);
    if (m_pane == normalized) {
        return;
    }
    m_pane = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    notifyOptionsChanged();
}

void ChartSeries::setLastValueLabelVisible(bool visible)
{
    if (m_lastValueLabelVisible == visible) {
        return;
    }
    m_lastValueLabelVisible = visible;
    notifyOptionsChanged();
}

void ChartSeries::setPriceLineVisible(bool visible)
{
    if (m_priceLineVisible == visible) {
        return;
    }
    m_priceLineVisible = visible;
    notifyOptionsChanged();
}

void ChartSeries::setPriceLineSource(int source)
{
    const int normalized = std::clamp(source,
        static_cast<int>(PriceLineSource::LastBar),
        static_cast<int>(PriceLineSource::LastVisible));
    if (m_priceLineSource == normalized) {
        return;
    }
    m_priceLineSource = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setPriceLineColor(const QColor& color)
{
    if (m_priceLineColor == color) {
        return;
    }
    m_priceLineColor = color;
    notifyOptionsChanged();
}

void ChartSeries::setPriceLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 16.0);
    if (qFuzzyCompare(m_priceLineWidth, normalized)) {
        return;
    }
    m_priceLineWidth = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setPriceLineStyle(int style)
{
    const int normalized = std::clamp(style,
        static_cast<int>(LineStyle::Solid),
        static_cast<int>(LineStyle::SparseDotted));
    if (m_priceLineStyle == normalized) {
        return;
    }
    m_priceLineStyle = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setBaseLineVisible(bool visible)
{
    if (m_baseLineVisible == visible) {
        return;
    }
    m_baseLineVisible = visible;
    notifyOptionsChanged();
}

void ChartSeries::setBaseLineColor(const QColor& color)
{
    if (m_baseLineColor == color) {
        return;
    }
    m_baseLineColor = color;
    notifyOptionsChanged();
}

void ChartSeries::setBaseLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(width, 0.0, 16.0);
    if (qFuzzyCompare(m_baseLineWidth, normalized)) {
        return;
    }
    m_baseLineWidth = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setBaseLineStyle(int style)
{
    const int normalized = std::clamp(style,
        static_cast<int>(LineStyle::Solid),
        static_cast<int>(LineStyle::SparseDotted));
    if (m_baseLineStyle == normalized) {
        return;
    }
    m_baseLineStyle = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setHitTestTolerance(qreal tolerance)
{
    if (!std::isfinite(tolerance)) {
        return;
    }
    const qreal normalized = std::clamp<qreal>(tolerance, 0.0, 128.0);
    if (qFuzzyCompare(m_hitTestTolerance, normalized)) {
        return;
    }
    m_hitTestTolerance = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setDataConflationThreshold(qreal threshold)
{
    const qreal normalized = std::isfinite(threshold) ? std::clamp<qreal>(threshold, 0.0, 64.0) : 1.0;
    if (std::abs(m_dataConflationThreshold - normalized) <= 0.0001) {
        return;
    }
    m_dataConflationThreshold = normalized;
    notifyOptionsChanged();
}

void ChartSeries::setAutoscaleInfoProvider(const QJSValue& provider)
{
    m_autoscaleInfoProvider = provider;
    notifyOptionsChanged();
}

QVariantList ChartSeries::data() const
{
    QVariantList rows;
    rows.reserve(m_model.originalDataRows().size());
    for (const SeriesDataRow& row : m_model.originalDataRows()) {
        rows.push_back(row.row);
    }
    return rows;
}

QVariantMap ChartSeries::dataByIndex(int logicalIndex, int mismatchDirection) const
{
    const QVector<SeriesDataRow>& rows = m_model.originalDataRows();
    if (rows.isEmpty()) {
        return { };
    }

    if (!m_chart) {
        if (mismatchDirection == 0) {
            return logicalIndex >= 0 && logicalIndex < rows.size() ? rows.at(logicalIndex).row : QVariantMap();
        }
        if (mismatchDirection < 0) {
            return logicalIndex >= 0 ? rows.at(std::min(logicalIndex, static_cast<int>(rows.size() - 1))).row : QVariantMap();
        }
        return logicalIndex < rows.size() ? rows.at(std::max(0, logicalIndex)).row : QVariantMap();
    }

    if (mismatchDirection == 0) {
        const std::optional<TimePoint> time = m_chart->timeAtIndex(logicalIndex);
        if (!time.has_value()) {
            return { };
        }

        const auto it = std::lower_bound(rows.cbegin(), rows.cend(), time.value(), [](const SeriesDataRow& row, TimePoint value) {
            return row.time < value;
        });
        return it != rows.cend() && it->time == time.value() ? it->row : QVariantMap();
    }

    const SeriesDataRow* candidate = nullptr;
    int candidateIndex = mismatchDirection < 0 ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (const SeriesDataRow& row : rows) {
        const int rowIndex = m_chart->timeToIndex(row.time);
        if (rowIndex < 0) {
            continue;
        }
        if (mismatchDirection < 0 && rowIndex <= logicalIndex && rowIndex > candidateIndex) {
            candidate = &row;
            candidateIndex = rowIndex;
        } else if (mismatchDirection > 0 && rowIndex >= logicalIndex && rowIndex < candidateIndex) {
            candidate = &row;
            candidateIndex = rowIndex;
        }
    }

    return candidate ? candidate->row : QVariantMap();
}

void ChartSeries::setChart(FinancialChartItem* chart)
{
    m_chart = chart;
}

void ChartSeries::update(const QVariantMap& row)
{
    update(row, false);
}

void ChartSeries::pop(int count)
{
    const int removed = m_model.pop(count);
    if (removed > 0) {
        QVariantMap payload;
        payload.insert(QStringLiteral("count"), removed);
        notifyDataChanged(QStringLiteral("pop"), payload);
    }
}

QVariantMap ChartSeries::barsInLogicalRange(qreal from, qreal to) const
{
    if (!m_chart || !std::isfinite(from) || !std::isfinite(to) || from > to) {
        return { };
    }

    const QVector<TimePoint> times = m_model.times();
    if (times.isEmpty()) {
        return { };
    }

    int barsBefore = 0;
    int barsAfter = 0;
    std::optional<int> firstBar;
    std::optional<int> lastBar;

    for (TimePoint time : times) {
        const int index = m_chart->timeToIndex(time);
        if (index < 0) {
            continue;
        }
        if (index < from) {
            ++barsBefore;
            continue;
        }
        if (index > to) {
            ++barsAfter;
            continue;
        }

        if (!firstBar.has_value()) {
            firstBar = index;
        }
        lastBar = index;
    }

    if (!firstBar.has_value() || !lastBar.has_value()) {
        return { };
    }

    return {
        { QStringLiteral("barsBefore"), barsBefore },
        { QStringLiteral("barsAfter"), barsAfter },
        { QStringLiteral("from"), firstBar.value() },
        { QStringLiteral("to"), lastBar.value() },
    };
}

void ChartSeries::setMarkers(const QVariantList& markers)
{
    QVector<SeriesMarker> parsed;
    parsed.reserve(markers.size());

    for (const QVariant& markerValue : markers) {
        const QVariantMap markerMap = markerValue.toMap();
        TimePoint time = 0;
        if (!parseTime(markerMap.value(QStringLiteral("time")), &time)) {
            qWarning() << "QtFinCharts: marker is missing a valid time" << markerMap;
            continue;
        }

        SeriesMarker marker;
        marker.id = markerMap.value(QStringLiteral("id")).toString();
        marker.time = time;
        marker.position = markerPositionFromString(markerMap.value(QStringLiteral("position")).toString());
        marker.shape = markerShapeFromString(markerMap.value(QStringLiteral("shape")).toString());
        marker.color = colorFromVariant(markerMap.value(QStringLiteral("color")), marker.color);
        bool okSize = false;
        const qreal markerSize = markerMap.value(QStringLiteral("size")).toDouble(&okSize);
        if (okSize) {
            marker.size = std::isfinite(markerSize) ? std::clamp(markerSize, 0.25, 8.0) : marker.size;
        }
        bool okPrice = false;
        const double markerPrice = markerMap.value(QStringLiteral("price")).toDouble(&okPrice);
        if (okPrice && std::isfinite(markerPrice)) {
            marker.price = markerPrice;
        }
        marker.text = markerMap.value(QStringLiteral("text")).toString();
        marker.cursorShape = cursorShapeFromVariant(markerMap.value(QStringLiteral("cursorShape")),
            cursorShapeFromVariant(markerMap.value(QStringLiteral("cursor"))));
        parsed.push_back(marker);
    }

    m_model.setMarkers(std::move(parsed));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), m_model.markers().size());
    notifyDataChanged(QStringLiteral("markers"), payload);
}

QString ChartSeries::createPriceLine(const QVariantMap& options)
{
    bool okPrice = false;
    const double price = options.value(QStringLiteral("price")).toDouble(&okPrice);
    if (!okPrice || !std::isfinite(price)) {
        qWarning() << "QtFinCharts: price line is missing a finite price" << options;
        return { };
    }

    SeriesPriceLine line;
    line.id = options.value(QStringLiteral("id")).toString();
    if (line.id.isEmpty()) {
        do {
            line.id = QStringLiteral("priceLine%1").arg(m_nextPriceLineId++);
        } while (std::any_of(m_priceLines.cbegin(), m_priceLines.cend(), [&line](const SeriesPriceLine& existing) {
            return existing.id == line.id;
        }));
    }

    line.price = price;
    line.color = colorFromVariant(options.value(QStringLiteral("color")), QColor());
    bool okWidth = false;
    const qreal width = options.value(QStringLiteral("width")).toDouble(&okWidth);
    if (!okWidth) {
        okWidth = false;
        const qreal lineWidth = options.value(QStringLiteral("lineWidth")).toDouble(&okWidth);
        if (okWidth && std::isfinite(lineWidth)) {
            line.lineWidth = std::clamp<qreal>(lineWidth, 0.0, 16.0);
        }
    } else {
        if (std::isfinite(width)) {
            line.lineWidth = std::clamp<qreal>(width, 0.0, 16.0);
        }
    }
    const QVariant styleValue = options.contains(QStringLiteral("style"))
        ? options.value(QStringLiteral("style"))
        : options.value(QStringLiteral("lineStyle"));
    bool okStyle = false;
    const int style = styleValue.toInt(&okStyle);
    if (okStyle) {
        line.lineStyle = static_cast<LineStyle>(std::clamp(style,
            static_cast<int>(LineStyle::Solid),
            static_cast<int>(LineStyle::SparseDotted)));
    }
    if (options.contains(QStringLiteral("lineVisible"))) {
        line.lineVisible = options.value(QStringLiteral("lineVisible")).toBool();
    }
    if (options.contains(QStringLiteral("axisLabelVisible"))) {
        line.axisLabelVisible = options.value(QStringLiteral("axisLabelVisible")).toBool();
    }
    line.title = options.value(QStringLiteral("title")).toString();
    line.axisLabelTextColor = colorFromVariant(options.value(QStringLiteral("axisLabelTextColor")), QColor());
    line.axisLabelBackgroundColor = colorFromVariant(options.value(QStringLiteral("axisLabelBackgroundColor")), QColor());
    line.cursorShape = cursorShapeFromVariant(options.value(QStringLiteral("cursorShape")),
        cursorShapeFromVariant(options.value(QStringLiteral("cursor"))));

    const auto existing = std::find_if(m_priceLines.begin(), m_priceLines.end(), [&line](const SeriesPriceLine& priceLine) {
        return priceLine.id == line.id;
    });
    if (existing == m_priceLines.end()) {
        m_priceLines.push_back(line);
    } else {
        *existing = line;
    }

    notifyOptionsChanged();
    return line.id;
}

bool ChartSeries::removePriceLine(const QString& id)
{
    const auto oldSize = m_priceLines.size();
    m_priceLines.erase(std::remove_if(m_priceLines.begin(), m_priceLines.end(), [&id](const SeriesPriceLine& line) {
        return line.id == id;
    }),
        m_priceLines.end());
    if (m_priceLines.size() == oldSize) {
        return false;
    }
    notifyOptionsChanged();
    return true;
}

void ChartSeries::removeAllPriceLines()
{
    if (m_priceLines.isEmpty()) {
        return;
    }
    m_priceLines.clear();
    notifyOptionsChanged();
}

QVariantList ChartSeries::priceLines() const
{
    QVariantList lines;
    lines.reserve(m_priceLines.size());
    for (const SeriesPriceLine& line : m_priceLines) {
        lines.push_back(QVariantMap {
            { QStringLiteral("id"), line.id },
            { QStringLiteral("price"), line.price },
            { QStringLiteral("color"), line.color },
            { QStringLiteral("width"), line.lineWidth },
            { QStringLiteral("lineWidth"), line.lineWidth },
            { QStringLiteral("style"), static_cast<int>(line.lineStyle) },
            { QStringLiteral("lineStyle"), static_cast<int>(line.lineStyle) },
            { QStringLiteral("lineVisible"), line.lineVisible },
            { QStringLiteral("axisLabelVisible"), line.axisLabelVisible },
            { QStringLiteral("title"), line.title },
            { QStringLiteral("axisLabelTextColor"), line.axisLabelTextColor },
            { QStringLiteral("axisLabelBackgroundColor"), line.axisLabelBackgroundColor },
            { QStringLiteral("cursorShape"), line.cursorShape },
        });
    }
    return lines;
}

QVariantMap ChartSeries::priceScale() const
{
    QVariantMap result {
        { QStringLiteral("id"), m_priceScaleId },
        { QStringLiteral("priceScaleId"), m_priceScaleId },
        { QStringLiteral("pane"), m_pane },
    };
    if (m_chart) {
        result.insert(QStringLiteral("visibleRange"), m_chart->visiblePriceRangeForScale(m_priceScaleId, m_pane));
    }
    return result;
}

QVariantMap ChartSeries::visiblePriceRange() const
{
    return m_chart ? m_chart->visiblePriceRangeForScale(m_priceScaleId, m_pane) : QVariantMap();
}

void ChartSeries::setVisiblePriceRange(double minimum, double maximum)
{
    if (m_chart) {
        m_chart->setVisiblePriceRangeForScale(m_priceScaleId, m_pane, minimum, maximum);
    }
}

void ChartSeries::clearVisiblePriceRange()
{
    if (m_chart) {
        m_chart->clearVisiblePriceRangeForScale(m_priceScaleId, m_pane);
    }
}

bool ChartSeries::parseTime(const QVariant& value, TimePoint* out)
{
    if (!out || !value.isValid()) {
        return false;
    }

    auto parseText = [](const QString& text, TimePoint* parsed) {
        if (text.isEmpty()) {
            return false;
        }

        const QDate date = QDate::fromString(text, Qt::ISODate);
        if (date.isValid() && !text.contains(QLatin1Char('T'))) {
            *parsed = dateToUtcTimePoint(date);
            return true;
        }

        QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
        if (dateTime.isValid()) {
            if (dateTime.timeSpec() == Qt::LocalTime) {
                dateTime = QDateTime(dateTime.date(), dateTime.time(), QTimeZone::UTC);
            }
            *parsed = dateTime.toSecsSinceEpoch();
            return true;
        }

        if (date.isValid()) {
            *parsed = dateToUtcTimePoint(date);
            return true;
        }

        return false;
    };

    const QVariantMap dateMap = value.toMap();
    if (dateMap.contains(QStringLiteral("year"))
        && dateMap.contains(QStringLiteral("month"))
        && dateMap.contains(QStringLiteral("day"))) {
        bool okYear = false;
        bool okMonth = false;
        bool okDay = false;
        const int year = dateMap.value(QStringLiteral("year")).toInt(&okYear);
        const int month = dateMap.value(QStringLiteral("month")).toInt(&okMonth);
        const int day = dateMap.value(QStringLiteral("day")).toInt(&okDay);
        const QDate date(year, month, day);
        if (okYear && okMonth && okDay && date.isValid()) {
            *out = dateToUtcTimePoint(date);
            return true;
        }
    }

    if (value.metaType().id() == QMetaType::QString || value.metaType().id() == QMetaType::QByteArray) {
        return parseText(value.toString(), out);
    }

    if (value.canConvert<QDateTime>()) {
        const QDateTime dateTime = value.toDateTime();
        if (dateTime.isValid()) {
            *out = dateTime.toSecsSinceEpoch();
            return true;
        }
    }

    if (value.canConvert<QDate>()) {
        const QDate date = value.toDate();
        if (date.isValid()) {
            *out = dateToUtcTimePoint(date);
            return true;
        }
    }

    bool ok = false;
    const qlonglong numeric = value.toLongLong(&ok);
    if (ok) {
        *out = numeric;
        return true;
    }

    return parseText(value.toString(), out);
}

std::optional<double> ChartSeries::parseFiniteDouble(const QVariant& value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return std::nullopt;
    }
    return number;
}

QColor ChartSeries::colorFromVariant(const QVariant& value, const QColor& fallback)
{
    return parseColorVariant(value, fallback);
}

bool ChartSeries::shouldAcceptUpdate(TimePoint time, bool historicalUpdate) const
{
    if (historicalUpdate) {
        return true;
    }

    const std::optional<TimePoint> latest = m_model.lastTime();
    if (!latest.has_value() || time >= latest.value()) {
        return true;
    }

    qWarning() << "QtFinCharts: skipped historical update for" << seriesTypeName(m_model.type())
               << "series at time" << time
               << "because historicalUpdate is false";
    return false;
}

void ChartSeries::notifyDataChanged(const QString& scope, QVariantMap payload)
{
    payload.insert(QStringLiteral("scope"), scope.isEmpty() ? QStringLiteral("full") : scope);
    payload.insert(QStringLiteral("type"), seriesTypeName(m_model.type()));
    if (!m_name.isEmpty()) {
        payload.insert(QStringLiteral("series"), m_name);
    }
    Q_EMIT dataChangedDetailed(payload);
    Q_EMIT dataChanged();
}

void ChartSeries::notifyOptionsChanged()
{
    Q_EMIT optionsChanged();
}

} // namespace QtFinCharts
