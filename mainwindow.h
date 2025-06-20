#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QImage>
#include <QJsonObject>
#include <QTimer>

#include "visionworker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void updateFrame(const QImage &frame);
    void handleDetections(const QJsonObject &detection_data);
    void updateDateTime();
    void on_checkButton_clicked();
    void on_resetButton_clicked();

private:
    Ui::MainWindow *ui;
    QThread* visionThread;
    VisionWorker* worker;
    QTimer *dateTimeTimer;

    QJsonObject lastDetections;
    QImage lastFrame; // To store a snapshot on fail

    // Counters for pass rate calculation
    int daily_total = 0;
    int daily_ok = 0;
    int weekly_total = 0;
    int weekly_ok = 0;
    int grand_total = 0;
    int grand_ok = 0;

    void setupConnections();
    void setupUI();
    void updatePassRateTables();
    void addDetectionToHistory(const QString &result);
    void setStatusBox(QWidget *box, const QString &title, const QString &result);
};
#endif // MAINWINDOW_H
