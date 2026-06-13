qt_add_executable(QtFinChartsBasic
    main.cpp
)

qt_add_qml_module(QtFinChartsBasic
    URI QtFinCharts.Basic
    VERSION 1.0
    IMPORT_PATH
        ${QTFINCHARTS_QML_OUTPUT_BASE_DIR}
    QML_FILES
        Main.qml
)

target_link_libraries(QtFinChartsBasic
    PRIVATE
        Qt6::Quick
        QtFinCharts::QtFinCharts
)
