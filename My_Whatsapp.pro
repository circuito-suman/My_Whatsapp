QT += widgets webenginewidgets printsupport
CONFIG += c++17

SOURCES += main.cpp \
           mywhatsappwindow.cpp

HEADERS += mywhatsappwindow.h

RESOURCES += resources/resources.qrc

TARGET = My_Whatsapp
TEMPLATE = app

FORMS += \
    mywhatsappwindow.ui
