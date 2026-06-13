#pragma once

#include "qtfincharts_export.h"

#include "ChartSeries.h"

#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT BarSeries : public ChartSeries {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor upColor READ upColor WRITE setUpColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor downColor READ downColor WRITE setDownColor NOTIFY optionsChanged)
    Q_PROPERTY(bool openVisible READ openVisible WRITE setOpenVisible NOTIFY optionsChanged)
    Q_PROPERTY(bool thinBars READ thinBars WRITE setThinBars NOTIFY optionsChanged)

public:
    explicit BarSeries(QObject* parent = nullptr);

    [[nodiscard]] QColor upColor() const noexcept { return m_upColor; }
    void setUpColor(const QColor& color);

    [[nodiscard]] QColor downColor() const noexcept { return m_downColor; }
    void setDownColor(const QColor& color);

    [[nodiscard]] bool openVisible() const noexcept { return m_openVisible; }
    void setOpenVisible(bool visible);

    [[nodiscard]] bool thinBars() const noexcept { return m_thinBars; }
    void setThinBars(bool thinBars);

    void setData(const QVariantList& rows) override;
    void update(const QVariantMap& row, bool historicalUpdate = false) override;

private:
    [[nodiscard]] static bool parseRow(const QVariantMap& row, CandlestickDataPoint* point);

    QColor m_upColor = QColor("#26a69a");
    QColor m_downColor = QColor("#ef5350");
    bool m_openVisible = true;
    bool m_thinBars = false;
};

} // namespace QtFinCharts
