#pragma once

#include "qtfincharts_export.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT ChartTheme : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString backgroundType READ backgroundType WRITE setBackgroundType NOTIFY changed)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY changed)
    Q_PROPERTY(QColor backgroundTopColor READ backgroundTopColor WRITE setBackgroundTopColor NOTIFY changed)
    Q_PROPERTY(QColor backgroundBottomColor READ backgroundBottomColor WRITE setBackgroundBottomColor NOTIFY changed)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY changed)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY changed)
    Q_PROPERTY(QColor axisColor READ axisColor WRITE setAxisColor NOTIFY changed)
    Q_PROPERTY(QColor priceAxisBorderColor READ priceAxisBorderColor WRITE setPriceAxisBorderColor NOTIFY changed)
    Q_PROPERTY(QColor timeAxisBorderColor READ timeAxisBorderColor WRITE setTimeAxisBorderColor NOTIFY changed)
    Q_PROPERTY(QColor crosshairColor READ crosshairColor WRITE setCrosshairColor NOTIFY changed)
    Q_PROPERTY(QColor crosshairLabelBackground READ crosshairLabelBackground WRITE setCrosshairLabelBackground NOTIFY changed)
    Q_PROPERTY(QColor crosshairLabelText READ crosshairLabelText WRITE setCrosshairLabelText NOTIFY changed)
    Q_PROPERTY(QColor paneSeparatorColor READ paneSeparatorColor WRITE setPaneSeparatorColor NOTIFY changed)
    Q_PROPERTY(QColor paneSeparatorHoverColor READ paneSeparatorHoverColor WRITE setPaneSeparatorHoverColor NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY changed)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY changed)

public:
    explicit ChartTheme(QObject* parent = nullptr);

    [[nodiscard]] QString backgroundType() const { return m_backgroundType; }
    void setBackgroundType(const QString& type);

    [[nodiscard]] QColor backgroundColor() const noexcept { return m_backgroundColor; }
    void setBackgroundColor(const QColor& color);

    [[nodiscard]] QColor backgroundTopColor() const noexcept { return m_backgroundTopColor; }
    void setBackgroundTopColor(const QColor& color);

    [[nodiscard]] QColor backgroundBottomColor() const noexcept { return m_backgroundBottomColor; }
    void setBackgroundBottomColor(const QColor& color);

    [[nodiscard]] QColor textColor() const noexcept { return m_textColor; }
    void setTextColor(const QColor& color);

    [[nodiscard]] QColor gridColor() const noexcept { return m_gridColor; }
    void setGridColor(const QColor& color);

    [[nodiscard]] QColor axisColor() const noexcept;
    void setAxisColor(const QColor& color);

    [[nodiscard]] QColor priceAxisBorderColor() const noexcept { return m_priceAxisBorderColor; }
    void setPriceAxisBorderColor(const QColor& color);

    [[nodiscard]] QColor timeAxisBorderColor() const noexcept { return m_timeAxisBorderColor; }
    void setTimeAxisBorderColor(const QColor& color);

    [[nodiscard]] QColor crosshairColor() const noexcept { return m_crosshairColor; }
    void setCrosshairColor(const QColor& color);

    [[nodiscard]] QColor crosshairLabelBackground() const noexcept { return m_crosshairLabelBackground; }
    void setCrosshairLabelBackground(const QColor& color);

    [[nodiscard]] QColor crosshairLabelText() const noexcept { return m_crosshairLabelText; }
    void setCrosshairLabelText(const QColor& color);

    [[nodiscard]] QColor paneSeparatorColor() const noexcept { return m_paneSeparatorColor; }
    void setPaneSeparatorColor(const QColor& color);

    [[nodiscard]] QColor paneSeparatorHoverColor() const noexcept { return m_paneSeparatorHoverColor; }
    void setPaneSeparatorHoverColor(const QColor& color);

    [[nodiscard]] QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& family);

    [[nodiscard]] int fontPixelSize() const noexcept { return m_fontPixelSize; }
    void setFontPixelSize(int size);

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] static QString normalizeBackgroundType(const QString& type);

    template <typename T>
    void setIfChanged(T& member, const T& value);

    QString m_backgroundType = QStringLiteral("solid");
    QColor m_backgroundColor = QColor("#111318");
    QColor m_backgroundTopColor = QColor("#111318");
    QColor m_backgroundBottomColor = QColor("#111318");
    QColor m_textColor = QColor("#d6d8df");
    QColor m_gridColor = QColor("#2a2e39");
    QColor m_priceAxisBorderColor = QColor("#3a3f4b");
    QColor m_timeAxisBorderColor = QColor("#3a3f4b");
    QColor m_crosshairColor = QColor("#8d95a8");
    QColor m_crosshairLabelBackground = QColor("#2962ff");
    QColor m_crosshairLabelText = QColor("#ffffff");
    QColor m_paneSeparatorColor = QColor("#20242d");
    QColor m_paneSeparatorHoverColor = QColor("#3a3f4b");
    QString m_fontFamily;
    int m_fontPixelSize = 11;
};

} // namespace QtFinCharts
