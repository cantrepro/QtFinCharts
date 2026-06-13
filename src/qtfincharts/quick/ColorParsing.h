#pragma once

#include "qtfincharts_export.h"

#include <QtCore/QVariant>
#include <QtGui/QColor>

namespace QtFinCharts {

[[nodiscard]] QTFINCHARTS_EXPORT QColor parseColorVariant(const QVariant& value, const QColor& fallback = QColor());

} // namespace QtFinCharts
