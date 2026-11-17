#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 3) { std::cerr << "usage: detect_objects MODEL.onnx COCO.names\n"; return 2; }
    try {
        cv::dnn::Net net = cv::dnn::readNetFromONNX(argv[1]);
        std::ifstream labelsFile(argv[2]);
        std::vector<std::string> labels;
        for (std::string line; std::getline(labelsFile, line); ) labels.push_back(line);
        if (labels.size() != 80) { std::cerr << "expected 80 COCO labels\n"; return 2; }
        auto detect = [&](cv::Mat &frame) {
            cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, cv::Size(320, 320), cv::Scalar(), true, false);
            net.setInput(blob);
            const auto start = std::chrono::steady_clock::now();
            cv::Mat out = net.forward();
            const auto inferMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (out.dims != 3 || out.size[0] != 1 || out.size[1] != 84 || out.size[2] != 2100) {
                std::cerr << "unexpected output shape:";
                for (int i = 0; i < out.dims; ++i) std::cerr << ' ' << out.size[i];
                std::cerr << '\n'; return false;
            }
            std::vector<cv::Rect> boxes;
            std::vector<float> scores;
            std::vector<int> classes;
            const float *p = out.ptr<float>();
            for (int i = 0; i < 2100; ++i) {
                int cls = 0;
                float score = 0;
                for (int c = 0; c < 80; ++c) {
                    float s = p[(c + 4) * 2100 + i];
                    if (s > score) { score = s; cls = c; }
                }
                if (score < 0.30f) continue;
                float cx = p[i] * frame.cols / 320.f;
                float cy = p[2100 + i] * frame.rows / 320.f;
                float w = p[2 * 2100 + i] * frame.cols / 320.f;
                float h = p[3 * 2100 + i] * frame.rows / 320.f;
                boxes.emplace_back(int(cx - w / 2), int(cy - h / 2), int(w), int(h));
                scores.push_back(score);
                classes.push_back(cls);
            }
            std::vector<int> keep;
            cv::dnn::NMSBoxesBatched(boxes, scores, classes, 0.30f, 0.45f, keep);
            for (int i : keep) {
                cv::rectangle(frame, boxes[i], cv::Scalar(0, 255, 0), 2);
                std::string label = labels[classes[i]] + cv::format(" %.0f%%", scores[i] * 100);
                cv::putText(frame, label, cv::Point(std::max(0, boxes[i].x), std::max(20, boxes[i].y)),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
                std::cout << label << ' ';
            }
            cv::putText(frame, cv::format("YOLOv8 objects: %zu", keep.size()), cv::Point(12, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
            std::cout << "objects=" << keep.size() << " infer_ms=" << inferMs << '\n' << std::flush;
            return true;
        };
        cv::Mat nv12(480 * 3 / 2, 640, CV_8UC1);
        cv::namedWindow("Camera2Termux object detection", cv::WINDOW_NORMAL);
        cv::resizeWindow("Camera2Termux object detection", 900, 1200);
        cv::moveWindow("Camera2Termux object detection", 0, 0);
        while (std::fread(nv12.data, 1, nv12.total(), stdin) == nv12.total()) {
            cv::Mat frame;
            cv::cvtColor(nv12, frame, cv::COLOR_YUV2BGR_NV12);
            // shortcut: Pixel 6a rear sensor is 90 degrees clockwise; configure from Camera2 metadata for other devices.
            cv::rotate(frame, frame, cv::ROTATE_90_CLOCKWISE);
            if (!detect(frame)) return 1;
            cv::imshow("Camera2Termux object detection", frame);
            if (cv::waitKey(1) == 27) break;
        }
    } catch (const cv::Exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
