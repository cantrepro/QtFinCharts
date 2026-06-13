#pragma once

#include "qtfincharts_export.h"

#include "../core/SeriesModel.h"

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>
#include <QtQml/QJSValue>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace QtFinCharts {

class FinancialChartItem;

class QTFINCHARTS_EXPORT ChartSeries : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ChartSeries)
    QML_UNCREATABLE("Use a concrete series type.")

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY optionsChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY optionsChanged)
    Q_PROPERTY(QString priceScaleId READ priceScaleId WRITE setPriceScaleId NOTIFY optionsChanged)
    Q_PROPERTY(int pane READ pane WRITE setPane NOTIFY optionsChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY optionsChanged)
    Q_PROPERTY(bool lastValueLabelVisible READ lastValueLabelVisible WRITE setLastValueLabelVisible NOTIFY optionsChanged)
    Q_PROPERTY(bool priceLineVisible READ priceLineVisible WRITE setPriceLineVisible NOTIFY optionsChanged)
    Q_PROPERTY(int priceLineSource READ priceLineSource WRITE setPriceLineSource NOTIFY optionsChanged)
    Q_PROPERTY(QColor priceLineColor READ priceLineColor WRITE setPriceLineColor NOTIFY optionsChanged)
    Q_PROPERTY(qreal priceLineWidth READ priceLineWidth WRITE setPriceLineWidth NOTIFY optionsChanged)
    Q_PROPERTY(int priceLineStyle READ priceLineStyle WRITE setPriceLineStyle NOTIFY optionsChanged)
    Q_PROPERTY(bool baseLineVisible READ baseLineVisible WRITE setBaseLineVisible NOTIFY optionsChanged)
    Q_PROPERTY(QColor baseLineColor READ baseLineColor WRITE setBaseLineColor NOTIFY optionsChanged)
    Q_PROPERTY(qreal baseLineWidth READ baseLineWidth WRITE setBaseLineWidth NOTIFY optionsChanged)
    Q_PROPERTY(int baseLineStyle READ baseLineStyle WRITE setBaseLineStyle NOTIFY optionsChanged)
    Q_PROPERTY(qreal hitTestTolerance READ hitTestTolerance WRITE setHitTestTolerance NOTIFY optionsChanged)
    Q_PROPERTY(qreal dataConflationThreshold READ dataConflationThreshold WRITE setDataConflationThreshold NOTIFY optionsChanged)
    Q_PROPERTY(QJSValue autoscaleInfoProvider READ autoscaleInfoProvider WRITE setAutoscaleInfoProvider NOTIFY optionsChanged)

public:
    explicit ChartSeries(SeriesType type, QObject* parent = nullptr);

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name);

    [[nodiscard]] QString title() const { return m_title; }
    void setTitle(const QString& title);

    [[nodiscard]] QString priceScaleId() const { return m_priceScaleId; }
    void setPriceScaleId(const QString& id);

    [[nodiscard]] int pane() const noexcept { return m_pane; }
    void setPane(int pane);

    [[nodiscard]] bool isVisible() const noexcept { return m_visible; }
    void setVisible(bool visible);

    [[nodiscard]] bool lastValueLabelVisible() const noexcept { return m_lastValueLabelVisible; }
    void setLastValueLabelVisible(bool visible);

    [[nodiscard]] bool priceLineVisible() const noexcept { return m_priceLineVisible; }
    void setPriceLineVisible(bool visible);

    [[nodiscard]] int priceLineSource() const noexcept { return m_priceLineSource; }
    void setPriceLineSource(int source);

    [[nodiscard]] QColor priceLineColor() const noexcept { return m_priceLineColor; }
    void setPriceLineColor(const QColor& color);

    [[nodiscard]] qreal priceLineWidth() const noexcept { return m_priceLineWidth; }
    void setPriceLineWidth(qreal width);

    [[nodiscard]] int priceLineStyle() const noexcept { return m_priceLineStyle; }
    void setPriceLineStyle(int style);

    [[nodiscard]] bool baseLineVisible() const noexcept { return m_baseLineVisible; }
    void setBaseLineVisible(bool visible);

    [[nodiscard]] QColor baseLineColor() const noexcept { return m_baseLineColor; }
    void setBaseLineColor(const QColor& color);

    [[nodiscard]] qreal baseLineWidth() const noexcept { return m_baseLineWidth; }
    void setBaseLineWidth(qreal width);

    [[nodiscard]] int baseLineStyle() const noexcept { return m_baseLineStyle; }
    void setBaseLineStyle(int style);

    [[nodiscard]] qreal hitTestTolerance() const noexcept { return m_hitTestTolerance; }
    void setHitTestTolerance(qreal tolerance);

    [[nodiscard]] qreal dataConflationThreshold() const noexcept { return m_dataConflationThreshold; }
    void setDataConflationThreshold(qreal threshold);

    [[nodiscard]] QJSValue autoscaleInfoProvider() const { return m_autoscaleInfoProvider; }
    void setAutoscaleInfoProvider(const QJSValue& provider);

    [[nodiscard]] const SeriesModel& model() const noexcept { return m_model; }
    [[nodiscard]] SeriesModel& model() noexcept { return m_model; }
    [[nodiscard]] const QVector<SeriesPriceLine>& customPriceLines() const noexcept { return m_priceLines; }

    void setChart(FinancialChartItem* chart);
    [[nodiscard]] FinancialChartItem* chart() const noexcept { return m_chart; }

    Q_INVOKABLE virtual void setData(const QVariantList& rows) = 0;
    Q_INVOKABLE QVariantList data() const;
    Q_INVOKABLE QVariantMap dataByIndex(int logicalIndex, int mismatchDirection = 0) const;
    Q_INVOKABLE void update(const QVariantMap& row);
    Q_INVOKABLE virtual void update(const QVariantMap& row, bool historicalUpdate) = 0;
    Q_INVOKABLE void pop(int count);
    Q_INVOKABLE QVariantMap barsInLogicalRange(qreal from, qreal to) const;
    Q_INVOKABLE void setMarkers(const QVariantList& markers);
    Q_INVOKABLE QString createPriceLine(const QVariantMap& options);
    Q_INVOKABLE bool removePriceLine(const QString& id);
    Q_INVOKABLE void removeAllPriceLines();
    Q_INVOKABLE QVariantList priceLines() const;
    Q_INVOKABLE QVariantMap priceScale() const;
    Q_INVOKABLE QVariantMap visiblePriceRange() const;
    Q_INVOKABLE void setVisiblePriceRange(double minimum, double maximum);
    Q_INVOKABLE void clearVisiblePriceRange();

Q_SIGNALS:
    void dataChanged();
    void dataChangedDetailed(const QVariantMap& payload);
    void optionsChanged();

protected:
    [[nodiscard]] static bool parseTime(const QVariant& value, TimePoint* out);
    [[nodiscard]] static std::optional<double> parseFiniteDouble(const QVariant& value);
    [[nodiscard]] static QColor colorFromVariant(const QVariant& value, const QColor& fallback);
    [[nodiscard]] bool shouldAcceptUpdate(TimePoint time, bool historicalUpdate) const;
    void notifyDataChanged(const QString& scope = QString(), QVariantMap payload = { });
    void notifyOptionsChanged();

private:
    QString m_name;
    QString m_title;
    QString m_priceScaleId = QStringLiteral("right");
    int m_pane = 0;
    bool m_visible = true;
    bool m_lastValueLabelVisible = false;
    bool m_priceLineVisible = false;
    int m_priceLineSource = static_cast<int>(PriceLineSource::LastBar);
    QColor m_priceLineColor;
    qreal m_priceLineWidth = 1.0;
    int m_priceLineStyle = static_cast<int>(LineStyle::Dashed);
    bool m_baseLineVisible = false;
    QColor m_baseLineColor;
    qreal m_baseLineWidth = 1.0;
    int m_baseLineStyle = static_cast<int>(LineStyle::Solid);
    qreal m_hitTestTolerance = 4.0;
    qreal m_dataConflationThreshold = 1.0;
    QJSValue m_autoscaleInfoProvider;
    SeriesModel m_model;
    QVector<SeriesPriceLine> m_priceLines;
    int m_nextPriceLineId = 1;
    FinancialChartItem* m_chart = nullptr;
};

} // namespace QtFinCharts
