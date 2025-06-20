#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // --- Translation Setup ---
    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("pokai-vision"), QLatin1String("_"), QLatin1String("."))) {
        a.installTranslator(&translator);
    }

    MainWindow w;
    w.showMaximized(); // Show maximized to fit the new layout
    return a.exec();
}
