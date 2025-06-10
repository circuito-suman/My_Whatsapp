#include <QApplication>
#include <QLabel>
#include "mywhatsappwindow.h"
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-features=PermissionsPolicyHeader,DocumentPolicyHeader,InterestCohort,--disable-gpu "
            "--disable-software-rasterizer "
            "--in-process-gpu"
            "--enable-logging --log-level=0");


    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_UseSoftwareOpenGL);

    app.setWindowIcon(QIcon(":resources/icons/whatsapp.png"));

    QCoreApplication::setOrganizationName("circuito_suman");
    QCoreApplication::setApplicationName("My_Whatsapp");



    MyWhatsappWindow window;
    window.resize(1024, 768);
    window.show();

    \

    return app.exec();
}
