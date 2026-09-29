qt_add_executable(testqt6 WIN32
    main.cpp
    mainwindow.h
    mainwindow.cpp
    QSDLCanvas.h
    QSDLCanvas.cpp
)
target_compile_features(testqt6 PRIVATE cxx_std_17)
target_link_libraries(testqt6 PRIVATE Qt6::Widgets SDL3::SDL3)
set_property(TARGET testqt6 PROPERTY AUTOMOC TRUE)
