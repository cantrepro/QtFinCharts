#include "PriceScale.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtFinCharts {

void PriceScale::setRange(const PriceRange& range, bool padRange)
{
    m_range = range.isValid()
        ? (padRange ? range.padded(0.04) : range)
        : PriceRange { -0.5, 0.5 };
}

void PriceScale::setMargins(qreal topFraction, qreal bottomFraction)
{
    const qreal safeTop = std::isfinite(topFraction) ? topFraction : m_topMargin;
    const qreal safeBottom = std::isfinite(bottomFraction) ? bottomFraction : m_bottomMargin;
    m_topMargin = std::clamp(safeTop, 0.0, 0.45);
    m_bottomMargin = std::clamp(safeBottom, 0.0, 0.45);
    if (m_topMargin + m_bottomMargin > 0.9) {
        const qreal scale = 0.9 / (m_topMargin + m_bottomMargin);
        m_topMargin *= scale;
        m_bottomMargin *= scale;
    }
}

bool PriceScale::hasTransformedBaseLine() const noexcept
{
    return (m_mode == PriceScaleMode::Percentage || m_mode == PriceScaleMode::IndexedTo100)
        && hasUsableBaseValue();
}

qreal PriceScale::priceToCoordinate(double price, const QRectF& rect) const noexcept
{
    const double value = transformPrice(price);
    if (!std::isfinite(value)) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }

    const PriceRange range = resolvedTransformedRange(price);
    return transformedValueToCoordinate(value, range, rect);
}

double PriceScale::coordinateToPrice(qreal y, const QRectF& rect) const noexcept
{
    const PriceRange range = resolvedTransformedRange();
    const qreal top = rect.top() + rect.height() * m_topMargin;
    const qreal bottom = rect.bottom() - rect.height() * m_bottomMargin;
    const qreal usable = std::max<qreal>(1.0, bottom - top);
    const double normalized = (y - top) / usable;
    const double transformed = m_inverted
        ? range.min + normalized * range.span()
        : range.max - normalized * range.span();
    return inverseTransformPrice(transformed);
}

double PriceScale::priceToDisplayValue(double price) const noexcept
{
    switch (m_mode) {
    case PriceScaleMode::Normal:
    case PriceScaleMode::Logarithmic:
        return price;
    case PriceScaleMode::Percentage:
    case PriceScaleMode::IndexedTo100:
        return transformPrice(price);
    }
    return price;
}

QVector<PriceTick> PriceScale::buildTicks(const QRectF& rect,
    const QLocale& locale,
    int precision,
    qreal targetSpacing,
    PriceFormat format) const
{
    QVector<PriceTick> ticks;
    const PriceRange range = resolvedTransformedRange();
    if (!range.isValid() || rect.height() <= 0.0) {
        return ticks;
    }

    const double span = std::max(range.span(), 1e-9);
    const double approxCount = std::max(2.0, rect.height() / std::max<qreal>(24.0, targetSpacing));
    const double step = niceStep(span / approxCount);
    if (step <= 0.0 || !std::isfinite(step)) {
        return ticks;
    }

    const double first = std::ceil(range.min / step) * step;
    for (double value = first; value <= range.max + step * 0.5; value += step) {
        const qreal y = transformedValueToCoordinate(value, range, rect);
        if (y < rect.top() - 1.0 || y > rect.bottom() + 1.0) {
            continue;
        }
        const double displayValue = m_mode == PriceScaleMode::Logarithmic
            ? inverseTransformPrice(value)
            : ((m_mode == PriceScaleMode::Percentage || m_mode == PriceScaleMode::IndexedTo100)
                      ? value
                      : inverseTransformPrice(value));
        ticks.push_back({ displayValue, y, formatValue(displayValue, locale, precision, format) });
    }
    return ticks;
}

QString PriceScale::formatValue(double value, const QLocale& locale, int precision, PriceFormat format)
{
    const int normalizedPrecision = std::clamp(precision, 0, 8);
    switch (format) {
    case PriceFormat::Price:
        return locale.toString(value, 'f', normalizedPrecision);
    case PriceFormat::Percent:
        return locale.toString(value, 'f', normalizedPrecision) + QStringLiteral("%");
    case PriceFormat::Volume: {
        const double absValue = std::abs(value);
        struct Unit {
            double divisor;
            const char* suffix;
        };
        constexpr Unit units[] = {
            { 1'000'000'000'000.0, "T" },
            { 1'000'000'000.0, "B" },
            { 1'000'000.0, "M" },
            { 1'000.0, "K" },
        };

        for (const Unit& unit : units) {
            if (absValue >= unit.divisor) {
                return locale.toString(value / unit.divisor, 'f', normalizedPrecision) + QLatin1String(unit.suffix);
            }
        }
        return locale.toString(value, 'f', normalizedPrecision);
    }
    }
    return locale.toString(value, 'f', normalizedPrecision);
}

double PriceScale::niceStep(double rawStep) noexcept
{
    if (rawStep <= 0.0 || !std::isfinite(rawStep)) {
        return 1.0;
    }

    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;
    double nice = 10.0;
    if (normalized <= 1.0) {
        nice = 1.0;
    } else if (normalized <= 2.0) {
        nice = 2.0;
    } else if (normalized <= 2.5) {
        nice = 2.5;
    } else if (normalized <= 5.0) {
        nice = 5.0;
    }
    return nice * magnitude;
}

double PriceScale::transformPrice(double price) const noexcept
{
    if (!std::isfinite(price)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    switch (m_mode) {
    case PriceScaleMode::Normal:
        return price;
    case PriceScaleMode::Logarithmic:
        return price > 0.0 ? std::log10(price) : std::numeric_limits<double>::quiet_NaN();
    case PriceScaleMode::Percentage:
        if (!hasUsableBaseValue()) {
            return price;
        }
        return (price - m_baseValue) / m_baseValue * 100.0;
    case PriceScaleMode::IndexedTo100:
        if (!hasUsableBaseValue()) {
            return price;
        }
        return price / m_baseValue * 100.0;
    }
    return price;
}

double PriceScale::inverseTransformPrice(double value) const noexcept
{
    if (!std::isfinite(value)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    switch (m_mode) {
    case PriceScaleMode::Normal:
        return value;
    case PriceScaleMode::Logarithmic:
        return std::pow(10.0, value);
    case PriceScaleMode::Percentage:
        if (!hasUsableBaseValue()) {
            return value;
        }
        return m_baseValue * (1.0 + value / 100.0);
    case PriceScaleMode::IndexedTo100:
        if (!hasUsableBaseValue()) {
            return value;
        }
        return m_baseValue * value / 100.0;
    }
    return value;
}

PriceRange PriceScale::transformedRange(const PriceRange& range) const noexcept
{
    PriceRange transformed;
    if (!range.isValid()) {
        return transformed;
    }

    if (m_mode == PriceScaleMode::Logarithmic) {
        if (range.max <= 0.0) {
            return transformed;
        }

        const double upper = range.max;
        double lower = range.min > 0.0 ? range.min : upper / 10.0;
        lower = std::max(lower, std::numeric_limits<double>::denorm_min());
        if (lower >= upper) {
            transformed.include(transformPrice(upper));
            return transformed;
        }

        transformed.include(transformPrice(lower));
        transformed.include(transformPrice(upper));
        return transformed;
    }

    transformed.include(transformPrice(range.min));
    transformed.include(transformPrice(range.max));
    return transformed;
}

PriceRange PriceScale::resolvedTransformedRange(double fallbackPrice) const noexcept
{
    const PriceRange rawRange = m_range.isValid() && m_range.span() > 0.0
        ? m_range
        : PriceRange { -0.5, 0.5 };
    const PriceRange range = transformedRange(rawRange);
    if (range.isValid() && range.span() > 0.0) {
        return range;
    }

    double center = transformPrice(fallbackPrice);
    if (!std::isfinite(center)) {
        center = 0.0;
    }
    return { center - 0.5, center + 0.5 };
}

qreal PriceScale::transformedValueToCoordinate(double value, const PriceRange& range, const QRectF& rect) const noexcept
{
    if (!std::isfinite(value) || !range.isValid() || range.span() <= 0.0) {
        return std::numeric_limits<qreal>::quiet_NaN();
    }

    const qreal top = rect.top() + rect.height() * m_topMargin;
    const qreal bottom = rect.bottom() - rect.height() * m_bottomMargin;
    const qreal usable = std::max<qreal>(1.0, bottom - top);
    const double normalized = m_inverted
        ? (value - range.min) / range.span()
        : (range.max - value) / range.span();
    return top + usable * normalized;
}

bool PriceScale::hasUsableBaseValue() const noexcept
{
    return std::isfinite(m_baseValue) && std::abs(m_baseValue) > 1e-12;
}

} // namespace QtFinCharts
