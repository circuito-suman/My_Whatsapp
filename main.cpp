#include <QApplication>
#include <QLabel>
#include "mywhatsappwindow.h"

int main(int argc, char *argv[]) {

    QCoreApplication::setOrganizationName("circuito_suman");
    QCoreApplication::setApplicationName("My_Whatsapp");


    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":resources/icons/whatsapp.png"));

    MyWhatsappWindow window;
    window.resize(1024, 768);
    window.show();


    \

    return app.exec();
}
