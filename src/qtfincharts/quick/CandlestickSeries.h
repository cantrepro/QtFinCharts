#pragma once

#include "qtfincharts_export.h"

#include "ChartSeries.h"

#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT CandlestickSeries : public ChartSeries {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor upColor READ upColor WRITE setUpColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor downColor READ downColor WRITE setDownColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor borderUpColor READ borderUpColor WRITE setBorderUpColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor borderDownColor READ borderDownColor WRITE setBorderDownColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor wickColor READ wickColor WRITE setWickColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor wickUpColor READ wickUpColor WRITE setWickUpColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor wickDownColor READ wickDownColor WRITE setWickDownColor NOTIFY optionsChanged)
    Q_PROPERTY(bool wickVisible READ wickVisible WRITE setWickVisible NOTIFY optionsChanged)
    Q_PROPERTY(bool borderVisible READ borderVisible WRITE setBorderVisible NOTIFY optionsChanged)

public:
    explicit CandlestickSeries(QObject* parent = nullptr);

    [[nodiscard]] QColor upColor() const noexcept { return m_upColor; }
    void setUpColor(const QColor& color);

    [[nodiscard]] QColor downColor() const noexcept { return m_downColor; }
    void setDownColor(const QColor& color);

    [[nodiscard]] QColor borderColor() const noexcept;
    void setBorderColor(const QColor& color);

    [[nodiscard]] QColor borderUpColor() const noexcept { return m_borderUpColor; }
    void setBorderUpColor(const QColor& color);

    [[nodiscard]] QColor borderDownColor() const noexcept { return m_borderDownColor; }
    void setBorderDownColor(const QColor& color);

    [[nodiscard]] QColor wickColor() const noexcept;
    void setWickColor(const QColor& color);

    [[nodiscard]] QColor wickUpColor() const noexcept { return m_wickUpColor; }
    void setWickUpColor(const QColor& color);

    [[nodiscard]] QColor wickDownColor() const noexcept { return m_wickDownColor; }
    void setWickDownColor(const QColor& color);

    [[nodiscard]] bool wickVisible() const noexcept { return m_wickVisible; }
    void setWickVisible(bool visible);

    [[nodiscard]] bool borderVisible() const noexcept { return m_borderVisible; }
    void setBorderVisible(bool visible);

    void setData(const QVariantList& rows) override;
    void update(const QVariantMap& row, bool historicalUpdate = false) override;

private:
    [[nodiscard]] static bool parseRow(const QVariantMap& row, CandlestickDataPoint* point);

    QColor m_upColor = QColor("#26a69a");
    QColor m_downColor = QColor("#ef5350");
    QColor m_borderUpColor = QColor("#26a69a");
    QColor m_borderDownColor = QColor("#ef5350");
    QColor m_wickUpColor = QColor("#26a69a");
    QColor m_wickDownColor = QColor("#ef5350");
    bool m_wickVisible = true;
    bool m_borderVisible = false;
};

} // namespace QtFinCharts
