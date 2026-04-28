#pragma once

#include "qtfincharts_export.h"

#include "Types.h"

#include <QtCore/QLocale>
#include <QtCore/QRectF>
#include <QtCore/QVector>

#include <limits>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT PriceScale {
public:
    void setRange(const PriceRange& range, bool padRange = true);
    [[nodiscard]] PriceRange range() const noexcept { return m_range; }

    void setMargins(qreal topFraction, qreal bottomFraction);
    [[nodiscard]] qreal topMargin() const noexcept { return m_topMargin; }
    [[nodiscard]] qreal bottomMargin() const noexcept { return m_bottomMargin; }

    void setInverted(bool inverted) noexcept { m_inverted = inverted; }
    [[nodiscard]] bool inverted() const noexcept { return m_inverted; }

    void setMode(PriceScaleMode mode) noexcept { m_mode = mode; }
    [[nodiscard]] PriceScaleMode mode() const noexcept { return m_mode; }

    void setBaseValue(double value) noexcept { m_baseValue = value; }
    [[nodiscard]] double baseValue() const noexcept { return m_baseValue; }
    [[nodiscard]] bool hasTransformedBaseLine() const noexcept;

    [[nodiscard]] qreal priceToCoordinate(double price, const QRectF& rect) const noexcept;
    [[nodiscard]] double coordinateToPrice(qreal y, const QRectF& rect) const noexcept;
    [[nodiscard]] double priceToDisplayValue(double price) const noexcept;

    [[nodiscard]] QVector<PriceTick> buildTicks(const QRectF& rect,
        const QLocale& locale,
        int precision,
        qreal targetSpacing = 58.0,
        PriceFormat format = PriceFormat::Price) const;

    [[nodiscard]] static QString formatValue(double value,
        const QLocale& locale,
        int precision,
        PriceFormat format);

private:
    [[nodiscard]] static double niceStep(double rawStep) noexcept;
    [[nodiscard]] double transformPrice(double price) const noexcept;
    [[nodiscard]] double inverseTransformPrice(double value) const noexcept;
    [[nodiscard]] PriceRange transformedRange(const PriceRange& range) const noexcept;
    [[nodiscard]] PriceRange resolvedTransformedRange(double fallbackPrice = std::numeric_limits<double>::quiet_NaN()) const noexcept;
    [[nodiscard]] qreal transformedValueToCoordinate(double value, const PriceRange& range, const QRectF& rect) const noexcept;
    [[nodiscard]] bool hasUsableBaseValue() const noexcept;

    PriceRange m_range { -0.5, 0.5 };
    qreal m_topMargin = 0.08;
    qreal m_bottomMargin = 0.08;
    bool m_inverted = false;
    PriceScaleMode m_mode = PriceScaleMode::Normal;
    double m_baseValue = std::numeric_limits<double>::quiet_NaN();
};

} // namespace QtFinCharts
