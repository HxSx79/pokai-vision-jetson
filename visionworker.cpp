#include "visionworker.h"
#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <vector>

VisionWorker::VisionWorker(QObject *parent)
    : QObject(parent), context(1), socket(context, zmq::socket_type::req)
{
    // Constructor now sets up the ZMQ socket
}

VisionWorker::~VisionWorker()
{
    // Cleanly close the socket
    socket.close();
}

std::string VisionWorker::getGStreamerPipeline(int w, int h, int disp_w, int disp_h, int rate, int flip) {
    // This function is kept for reference but is not used for USB cameras.
    return "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=(int)" + std::to_string(w) + ", height=(int)" + std::to_string(h) +
           ", format=(string)NV12, framerate=(fraction)" + std::to_string(rate) + "/1 ! nvvidconv flip-method=" + std::to_string(flip) +
           " ! video/x-raw, width=(int)" + std::to_string(disp_w) + ", height=(int)" + std::to_string(disp_h) +
           ", format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink";
}

void VisionWorker::run()
{
    // --- Connect to the Python Inference Server ---
    try {
        qInfo() << "Connecting to YOLO inference server...";
        socket.connect("tcp://localhost:5555");
    } catch(const zmq::error_t& e) {
        qWarning() << "Error connecting ZMQ socket:" << e.what();
        emit finished();
        return;
    }

    // --- Open Camera ---
    int capture_width = 1280;
    int capture_height = 1024;

    // Use V4L2 for all Linux systems with USB cameras (including Jetson).
    cap.open(0, cv::CAP_V4L2);

    if (!cap.isOpened()) {
       qWarning() << "Error: Cannot open camera.";
       emit finished();
       return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, capture_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, capture_height);
    cap.set(cv::CAP_PROP_FPS, 25);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    qInfo() << "Camera opened. Starting client loop.";

    while (true) {
        {
            QMutexLocker locker(&mutex);
            if (!isRunning) break;
        }

        cv::Mat frame;
        cap.read(frame);
        if (frame.empty()) {
            qWarning() << "Warning: Dropped frame";
            QThread::msleep(100);
            continue;
        }

        // --- Send Frame to Server ---
        std::vector<uchar> buf;
        cv::imencode(".jpg", frame, buf);
        zmq::message_t request(buf.size());
        memcpy(request.data(), buf.data(), buf.size());
        socket.send(request, zmq::send_flags::none);

        // --- Wait for and Process Reply ---
        zmq::message_t reply;
        auto res = socket.recv(reply, zmq::recv_flags::none);

        QJsonObject detection_data;

        if (res) {
            std::string reply_str = reply.to_string();
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(reply_str).toUtf8());
            detection_data = doc.object();

            // --- Draw Detections on Frame ---
            // <<< FIX: Logic from drawDetections is now moved directly inside the loop.
            if (detection_data.contains("detections") && detection_data["detections"].isArray()) {
                QJsonArray detections = detection_data["detections"].toArray();

                for (const QJsonValue &value : detections) {
                    QJsonObject obj = value.toObject();
                    QJsonArray box = obj["box"].toArray();

                    int left = static_cast<int>(box[0].toDouble());
                    int top = static_cast<int>(box[1].toDouble());
                    int right = static_cast<int>(box[2].toDouble());
                    int bottom = static_cast<int>(box[3].toDouble());

                    QString class_name = obj["class_name"].toString();
                    double confidence = obj["confidence"].toDouble();

                    cv::Scalar color(0, 255, 0); // Green for OK
                    if (class_name == "Clip_NOK") {
                        color = cv::Scalar(0, 0, 255); // Red for NOK
                    } else if (class_name == "Airdam") {
                        color = cv::Scalar(255, 255, 0); // Cyan for Airdam
                    }

                    cv::rectangle(frame, cv::Point(left, top), cv::Point(right, bottom), color, 2);
                    std::string label = class_name.toStdString() + ": " + cv::format("%.2f", confidence);
                    cv::putText(frame, label, cv::Point(left, top - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
                }
            }
        }

        // --- Emit Signals for UI ---
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        emit frameReady(qimg.copy());
        emit detectionsReady(detection_data);

        QThread::msleep(30);
    }
    cap.release();
    emit finished();
}


void VisionWorker::stop() { QMutexLocker locker(&mutex); isRunning = false; }
