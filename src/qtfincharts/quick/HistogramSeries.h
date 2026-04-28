#pragma once

#include "qtfincharts_export.h"

#include "ChartSeries.h"

#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT HistogramSeries : public ChartSeries {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY optionsChanged)
    Q_PROPERTY(double baseValue READ baseValue WRITE setBaseValue NOTIFY optionsChanged)

public:
    explicit HistogramSeries(QObject* parent = nullptr);

    [[nodiscard]] QColor color() const noexcept { return m_color; }
    void setColor(const QColor& color);

    [[nodiscard]] double baseValue() const noexcept { return m_baseValue; }
    void setBaseValue(double baseValue);

    void setData(const QVariantList& rows) override;
    void update(const QVariantMap& row, bool historicalUpdate = false) override;

private:
    [[nodiscard]] static bool parseRow(const QVariantMap& row, HistogramDataPoint* point);

    QColor m_color = QColor("#26a69a");
    double m_baseValue = 0.0;
};

} // namespace QtFinCharts
