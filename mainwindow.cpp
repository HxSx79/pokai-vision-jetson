#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>
#include <QCloseEvent>
#include <QJsonArray>
#include <QDateTime>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();

    visionThread = new QThread(this);
    worker = new VisionWorker();
    worker->moveToThread(visionThread);

    setupConnections();
    visionThread->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI() {
    this->setWindowTitle(tr("Decatur MX - Real Time Objection Detection - Poka Yoke"));

    // Timer for the clock
    dateTimeTimer = new QTimer(this);
    connect(dateTimeTimer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    dateTimeTimer->start(1000);
    updateDateTime();

    // Setup tables
    ui->latestDetectionsTable->setColumnCount(3);
    ui->latestDetectionsTable->setHorizontalHeaderLabels({tr("#"), tr("Time"), tr("Result")});
    ui->latestDetectionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->latestDetectionsTable->verticalHeader()->setVisible(false);

    // Initial status
    on_resetButton_clicked();
}

void MainWindow::setupConnections()
{
    connect(visionThread, &QThread::started, worker, &VisionWorker::run);
    connect(worker, &VisionWorker::finished, visionThread, &QThread::quit);
    connect(worker, &VisionWorker::finished, worker, &VisionWorker::deleteLater);
    connect(visionThread, &QThread::finished, visionThread, &QThread::deleteLater);

    connect(worker, &VisionWorker::frameReady, this, &MainWindow::updateFrame);
    connect(worker, &VisionWorker::detectionsReady, this, &MainWindow::handleDetections);

    // Connect buttons to slots
    connect(ui->checkButton, &QPushButton::clicked, this, &MainWindow::on_checkButton_clicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &MainWindow::on_resetButton_clicked);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    worker->stop();
    visionThread->quit();
    visionThread->wait(500);
    event->accept();
}

void MainWindow::updateDateTime()
{
    // This function can be expanded to show date/time if a label is added
}

void MainWindow::updateFrame(const QImage &frame)
{
    lastFrame = frame.copy(); // Keep a copy for snapshots
    ui->webcamFeedLabel->setPixmap(QPixmap::fromImage(frame).scaled(
        ui->webcamFeedLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
    ));
}

void MainWindow::handleDetections(const QJsonObject &detection_data)
{
    lastDetections = detection_data;
}

void MainWindow::on_checkButton_clicked()
{
    int airdam_count = 0;
    bool clip_nok_present = false;
    int clips_ok_count = 0;

    if (lastDetections.contains("detections") && lastDetections["detections"].isArray()) {
        QJsonArray detections = lastDetections["detections"].toArray();
        for (const QJsonValue &value : detections) {
            QJsonObject obj = value.toObject();
            int class_id = obj["class_id"].toInt(-1);

            if (class_id == 0) airdam_count++;
            else if (class_id == 1) clip_nok_present = true;
            else if (class_id == 2) clips_ok_count++;
        }
    }

    bool part_detection_ok = (airdam_count == 1);
    bool clip_detection_ok = (part_detection_ok && !clip_nok_present && clips_ok_count == 1);
    bool final_result_ok = (part_detection_ok && clip_detection_ok);

    // Update status boxes
    setStatusBox(ui->partDetectionBox, tr("PART DETECTION"), part_detection_ok ? "OK" : "NOK");
    setStatusBox(ui->clipDetectionBox, tr("CLIP DETECTION"), clip_detection_ok ? "OK" : "NOK");

    // Update counters and tables
    daily_total++;
    weekly_total++;
    grand_total++;

    if(final_result_ok) {
        daily_ok++;
        weekly_ok++;
        grand_ok++;
        addDetectionToHistory("OK");
        ui->failedPartImageLabel->clear(); // Clear failed image on pass
        ui->failedPartImageLabel->setText("PICTURE");
    } else {
        addDetectionToHistory("NOK");
        // Show snapshot of failed part
        ui->failedPartImageLabel->setPixmap(QPixmap::fromImage(lastFrame).scaled(
            ui->failedPartImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
        ));
    }

    updatePassRateTables();
}

void MainWindow::addDetectionToHistory(const QString &result)
{
    int row = 0;
    ui->latestDetectionsTable->insertRow(row);

    QTableWidgetItem *item_num = new QTableWidgetItem(QString::number(grand_total));
    QTableWidgetItem *item_time = new QTableWidgetItem(QTime::currentTime().toString("HH:mm:ss"));
    // <<< FIX: Use tr() on the raw string, not the QString object
    QTableWidgetItem *item_result = new QTableWidgetItem(tr(result.toStdString().c_str()));

    item_num->setTextAlignment(Qt::AlignCenter);
    item_time->setTextAlignment(Qt::AlignCenter);
    item_result->setTextAlignment(Qt::AlignCenter);

    if(result != "OK"){
        item_result->setForeground(QBrush(Qt::red));
    }

    ui->latestDetectionsTable->setItem(row, 0, item_num);
    ui->latestDetectionsTable->setItem(row, 1, item_time);
    ui->latestDetectionsTable->setItem(row, 2, item_result);

    // Keep table size limited
    if(ui->latestDetectionsTable->rowCount() > 10) {
        ui->latestDetectionsTable->removeRow(10);
    }
}

void MainWindow::updatePassRateTables()
{
    // This is simplified. In a real app, this data would be saved/loaded.
    ui->dayTotalLabel->setText(QString::number(daily_total));
    ui->weekTotalLabel->setText(QString::number(weekly_total));
    ui->grandTotalLabel->setText(QString::number(grand_total));

    double day_rate = (daily_total > 0) ? (double(daily_ok) / daily_total) * 100.0 : 100.0;
    double week_rate = (weekly_total > 0) ? (double(weekly_ok) / weekly_total) * 100.0 : 100.0;
    double grand_rate = (grand_total > 0) ? (double(grand_ok) / grand_total) * 100.0 : 100.0;

    ui->dayRateLabel->setText(QString::number(day_rate, 'f', 1) + "%");
    ui->weekRateLabel->setText(QString::number(week_rate, 'f', 1) + "%");
    ui->grandRateLabel->setText(QString::number(grand_rate, 'f', 1) + "%");
}

void MainWindow::setStatusBox(QWidget *box, const QString &title, const QString &result)
{
    QLabel* titleLabel = box->findChild<QLabel*>("titleLabel");
    QLabel* resultLabel = box->findChild<QLabel*>("resultLabel");

    if(!titleLabel) { // Create labels if they don't exist
        box->setLayout(new QVBoxLayout());
        titleLabel = new QLabel(title);
        resultLabel = new QLabel(result);

        titleLabel->setObjectName("titleLabel");
        resultLabel->setObjectName("resultLabel");

        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        titleLabel->setFont(titleFont);

        QFont resultFont = resultLabel->font();
        resultFont.setBold(true);
        resultFont.setPointSize(28);
        resultLabel->setFont(resultFont);

        titleLabel->setAlignment(Qt::AlignCenter);
        resultLabel->setAlignment(Qt::AlignCenter);

        box->layout()->addWidget(titleLabel);
        box->layout()->addWidget(resultLabel);
    }

    titleLabel->setText(title);
    resultLabel->setText(tr(result.toStdString().c_str())); // <<< FIX: Correctly call tr()

    if(result == "OK"){
        box->setStyleSheet("background-color: #2ecc71; color: white; border-radius: 10px;");
    } else {
        box->setStyleSheet("background-color: #e74c3c; color: white; border-radius: 10px;");
    }
}


void MainWindow::on_resetButton_clicked()
{
    daily_total = 0; daily_ok = 0;
    weekly_total = 0; weekly_ok = 0;
    grand_total = 0; grand_ok = 0;

    updatePassRateTables();
    ui->latestDetectionsTable->setRowCount(0);

    setStatusBox(ui->partDetectionBox, tr("PART DETECTION"), "---");
    setStatusBox(ui->clipDetectionBox, tr("CLIP DETECTION"), "---");
    ui->partDetectionBox->setStyleSheet("background-color: #95a5a6; color: white; border-radius: 10px;");
    ui->clipDetectionBox->setStyleSheet("background-color: #95a5a6; color: white; border-radius: 10px;");

    ui->failedPartImageLabel->clear();
    ui->failedPartImageLabel->setText("PICTURE");
}
