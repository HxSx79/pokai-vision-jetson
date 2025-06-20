QT += core gui widgets multimedia network

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    visionworker.cpp

HEADERS += \
    mainwindow.h \
    visionworker.h

FORMS += \
    mainwindow.ui

# --- Translation Support ---
TRANSLATIONS += pokai-vision_es_MX.ts

# --- Add Model Files to Project View (for organization) ---
# NOTE: Model files are no longer used by the C++ app, but are kept here
# for project organization. They are used by the Python server.
OTHER_FILES += \
    yolo_model/best.pt \
    yolo_model/obj.names

# For Linux systems, using pkg-config is the recommended way to link libraries.
# You will need to install libzmq and cppzmq.
# sudo apt install libzmq3-dev
# And place cppzmq headers in your include path.
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4 libzmq
}
