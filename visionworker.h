#ifndef VISIONWORKER_H
#define VISIONWORKER_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QJsonObject>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

class VisionWorker : public QObject
{
    Q_OBJECT

public:
    explicit VisionWorker(QObject *parent = nullptr);
    ~VisionWorker();

public slots:
    void run();
    void stop();

signals:
    void frameReady(const QImage &frame);
    void detectionsReady(const QJsonObject &detection_data);
    void finished();

private:
    // No longer need a separate declaration for drawDetections
    std::string getGStreamerPipeline(int, int, int, int, int, int);

    cv::VideoCapture cap;
    bool isRunning = true;
    QMutex mutex;

    // ZeroMQ members
    zmq::context_t context;
    zmq::socket_t socket;
};

#endif // VISIONWORKER_H
