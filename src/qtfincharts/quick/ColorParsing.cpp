#include "ColorParsing.h"

#include <QtCore/QStringList>

#include <algorithm>
#include <cmath>
#include <optional>

namespace QtFinCharts {

namespace {

    std::optional<double> parseCssNumber(QString text)
    {
        text = text.trimmed();
        if (text.isEmpty()) {
            return std::nullopt;
        }

        if (text.endsWith(QStringLiteral("deg"), Qt::CaseInsensitive)) {
            text.chop(3);
        }

        bool ok = false;
        const double value = text.toDouble(&ok);
        return ok && std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
    }

    std::optional<double> parseCssComponent(QString text)
    {
        text = text.trimmed();
        const bool percentage = text.endsWith(QLatin1Char('%'));
        if (percentage) {
            text.chop(1);
        }

        const std::optional<double> value = parseCssNumber(text);
        if (!value.has_value()) {
            return std::nullopt;
        }

        return percentage
            ? std::clamp(value.value(), 0.0, 100.0) * 2.55
            : std::clamp(value.value(), 0.0, 255.0);
    }

    std::optional<double> parseCssRatio(QString text)
    {
        text = text.trimmed();
        const bool percentage = text.endsWith(QLatin1Char('%'));
        if (percentage) {
            text.chop(1);
        }

        const std::optional<double> value = parseCssNumber(text);
        if (!value.has_value()) {
            return std::nullopt;
        }

        return percentage
            ? std::clamp(value.value(), 0.0, 100.0) / 100.0
            : std::clamp(value.value(), 0.0, 1.0);
    }

    std::optional<double> parseCssAlpha(QString text)
    {
        text = text.trimmed();
        const bool percentage = text.endsWith(QLatin1Char('%'));
        if (percentage) {
            text.chop(1);
        }

        const std::optional<double> value = parseCssNumber(text);
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (percentage) {
            return std::clamp(value.value(), 0.0, 100.0) / 100.0;
        }
        if (value.value() > 1.0) {
            return std::clamp(value.value(), 0.0, 255.0) / 255.0;
        }
        return std::clamp(value.value(), 0.0, 1.0);
    }

    QStringList splitCssColorArguments(QString args)
    {
        args = args.trimmed();
        args.replace(QLatin1Char('/'), QLatin1Char(','));
        if (args.contains(QLatin1Char(','))) {
            QStringList result;
            const QStringList commaParts = args.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& commaPart : commaParts) {
                result += commaPart.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            }
            return result;
        }
        return args.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }

    QColor cssColorFromString(const QString& text)
    {
        const QString trimmed = text.trimmed();
        if (trimmed.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0) {
            return QColor(0, 0, 0, 0);
        }

        const int open = trimmed.indexOf(QLatin1Char('('));
        const int close = trimmed.lastIndexOf(QLatin1Char(')'));
        if (open <= 0 || close <= open) {
            return { };
        }

        const QString function = trimmed.left(open).trimmed().toLower();
        const QStringList parts = splitCssColorArguments(trimmed.mid(open + 1, close - open - 1));
        if ((function == QStringLiteral("rgb") || function == QStringLiteral("rgba")) && parts.size() >= 3) {
            const std::optional<double> red = parseCssComponent(parts.at(0));
            const std::optional<double> green = parseCssComponent(parts.at(1));
            const std::optional<double> blue = parseCssComponent(parts.at(2));
            const std::optional<double> alpha = parts.size() >= 4 ? parseCssAlpha(parts.at(3)) : std::optional<double>(1.0);
            if (red.has_value() && green.has_value() && blue.has_value() && alpha.has_value()) {
                return QColor::fromRgbF(red.value() / 255.0,
                    green.value() / 255.0,
                    blue.value() / 255.0,
                    alpha.value());
            }
        }

        if ((function == QStringLiteral("hsl") || function == QStringLiteral("hsla")) && parts.size() >= 3) {
            const std::optional<double> hue = parseCssNumber(parts.at(0));
            const std::optional<double> saturation = parseCssRatio(parts.at(1));
            const std::optional<double> lightness = parseCssRatio(parts.at(2));
            const std::optional<double> alpha = parts.size() >= 4 ? parseCssAlpha(parts.at(3)) : std::optional<double>(1.0);
            if (hue.has_value() && saturation.has_value() && lightness.has_value() && alpha.has_value()) {
                double normalizedHue = std::fmod(hue.value(), 360.0);
                if (normalizedHue < 0.0) {
                    normalizedHue += 360.0;
                }
                return QColor::fromHslF(normalizedHue / 360.0,
                    saturation.value(),
                    lightness.value(),
                    alpha.value());
            }
        }

        return { };
    }

} // namespace

QColor parseColorVariant(const QVariant& value, const QColor& fallback)
{
    if (!value.isValid()) {
        return fallback;
    }

    const QColor color = value.value<QColor>();
    if (color.isValid()) {
        return color;
    }

    const QString text = value.toString().trimmed();
    const QColor named(text);
    if (named.isValid()) {
        return named;
    }

    const QColor cssColor = cssColorFromString(text);
    return cssColor.isValid() ? cssColor : fallback;
}

} // namespace QtFinCharts
