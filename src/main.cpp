#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include "ArmorDetector.hpp"

using namespace rm;

cv::Mat generateDemoImage() {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));

    // 模拟装甲板中心的两个蓝色灯条（高灰度，保证能被二值化阈值分割）
    cv::rectangle(img, cv::Point(250, 180), cv::Point(265, 300), cv::Scalar(255, 240, 240), -1);
    cv::rectangle(img, cv::Point(430, 180), cv::Point(445, 300), cv::Scalar(255, 240, 240), -1);

    // 模拟车身中心灰色区域
    cv::rectangle(img, cv::Point(265, 210), cv::Point(430, 270), cv::Scalar(40, 40, 40), -1);

    cv::Mat noise(img.size(), CV_8UC3);
    cv::randn(noise, cv::Scalar(0, 0, 0), cv::Scalar(10, 10, 10));
    img += noise;
    return img;
}

void printArmorInfo(const std::vector<Armor>& armors) {
    for (size_t i = 0; i < armors.size(); ++i) {
        const auto& a = armors[i];
        std::cout << "--- Armor " << i << " ---" << std::endl;
        std::cout << "center: (" << a.center.x << ", " << a.center.y << ")" << std::endl;
        std::cout << "corners:" << std::endl;
        for (int k = 0; k < 4; ++k) {
            std::cout << "  [" << k << "] (" << a.corners[k].x << ", " << a.corners[k].y << ")" << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    std::string image_path = (argc > 1) ? argv[1] : "data/sample.jpg";
    cv::Mat src;
    bool is_demo = false;

    if (image_path == "--demo" || image_path == "-d") {
        src = generateDemoImage();
        is_demo = true;
        std::cout << "正在使用内置 demo 图片..." << std::endl;
    } else {
        src = cv::imread(image_path, cv::IMREAD_COLOR);
        if (src.empty()) {
            std::cerr << "无法读取图片: " << image_path << std::endl;
            std::cerr << "将回退到内置 demo 图片。你也可以使用 ./rm_armor_detector --demo" << std::endl;
            src = generateDemoImage();
            is_demo = true;
        }
    }

    ArmorParam param;
    param.color = ArmorColor::BLUE;
    ArmorDetector detector(param);

    auto armors = detector.detect(src);
    std::cout << "检测到装甲板数量: " << armors.size() << std::endl;
    printArmorInfo(armors);

    cv::Mat result = detector.visualize(src, armors);
    std::string output_path = is_demo ? "data/demo_result.jpg" : "result.jpg";
    if (!cv::imwrite(output_path, result)) {
        std::cerr << "保存结果图片失败: " << output_path << std::endl;
        return 1;
    }
    std::cout << "可视化结果已保存到: " << output_path << std::endl;

    if (is_demo) {
        cv::imwrite("data/demo_input.jpg", src);
    }

    return 0;
}
