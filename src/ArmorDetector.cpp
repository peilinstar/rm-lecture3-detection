#include "ArmorDetector.hpp"

#include <algorithm>
#include <cmath>

namespace rm {

// -------------------- LightBar --------------------

LightBar::LightBar(const cv::RotatedRect& box) : rect(box) {
    cv::Point2f p[4];
    box.points(p);

    // 按 y 坐标从小到大排序，上面两个点是 top 边，下面两个点是 bottom 边
    std::sort(p, p + 4, [](const cv::Point2f& a, const cv::Point2f& b) { return a.y < b.y; });

    top = (p[0] + p[1]) * 0.5f;
    bottom = (p[2] + p[3]) * 0.5f;

    length = cv::norm(top - bottom);
    width = cv::norm(p[0] - p[1]);

    // 与竖直方向的夹角
    tilt_angle = std::atan2(std::abs(top.x - bottom.x), std::abs(top.y - bottom.y)) * 180.0 / CV_PI;
}

// -------------------- Armor --------------------

Armor::Armor(const LightBar& l1, const LightBar& l2) {
    if (l1.rect.center.x < l2.rect.center.x) {
        left = l1;
        right = l2;
    } else {
        left = l2;
        right = l1;
    }
    center = (left.rect.center + right.rect.center) * 0.5f;

    corners[0] = left.top;     // tl
    corners[1] = right.top;    // tr
    corners[2] = right.bottom; // br
    corners[3] = left.bottom;  // bl
}

// -------------------- ArmorDetector --------------------

ArmorDetector::ArmorDetector(const ArmorParam& param) : param_(param) {}

void ArmorDetector::setParam(const ArmorParam& param) { param_ = param; }

void ArmorDetector::setColor(ArmorColor color) { param_.color = color; }

cv::Mat ArmorDetector::preprocessImage(const cv::Mat& src) {
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

    cv::Mat binary;
    cv::threshold(gray, binary, param_.binary_thres, 255, cv::THRESH_BINARY);

    return binary;
}

std::vector<LightBar> ArmorDetector::findLights(const cv::Mat& src, const cv::Mat& binary) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LightBar> lights;
    cv::Mat mask = cv::Mat::zeros(binary.size(), CV_8UC1);

    for (const auto& cnt : contours) {
        if (cnt.size() < 5) continue;

        LightBar light(cv::minAreaRect(cnt));
        if (!isLight(light)) continue;

        // 判断灯条颜色：在轮廓区域内比较 B 通道和 R 通道的平均值
        mask.setTo(0);
        cv::drawContours(mask, std::vector<std::vector<cv::Point>>{cnt}, -1, 255, -1);

        std::vector<cv::Mat> channels;
        cv::split(src, channels);
        double mean_b = cv::mean(channels[0], mask)[0];
        double mean_r = cv::mean(channels[2], mask)[0];
        light.color = (mean_b > mean_r) ? ArmorColor::BLUE : ArmorColor::RED;

        if (light.color == param_.color) {
            lights.push_back(light);
        }
    }

    return lights;
}

bool ArmorDetector::isLight(const LightBar& light) {
    if (light.length <= 1e-6) return false;

    double ratio = light.width / light.length;
    bool ratio_ok = param_.light.min_ratio < ratio && ratio < param_.light.max_ratio;
    bool angle_ok = light.tilt_angle < param_.light.max_angle;

    return ratio_ok && angle_ok;
}

std::vector<Armor> ArmorDetector::matchLights(const std::vector<LightBar>& lights) {
    std::vector<Armor> armors;

    for (size_t i = 0; i < lights.size(); ++i) {
        for (size_t j = i + 1; j < lights.size(); ++j) {
            const LightBar& a = lights[i];
            const LightBar& b = lights[j];

            // 只配对同颜色灯条
            if (a.color != b.color) continue;

            // 排除中间还夹着其他灯条的情况
            if (containLight(a, b, lights)) continue;

            if (isArmor(a, b)) {
                armors.emplace_back(a, b);
            }
        }
    }

    return armors;
}

bool ArmorDetector::containLight(const LightBar& l1, const LightBar& l2,
                                 const std::vector<LightBar>& lights) {
    std::vector<cv::Point2f> pts = {l1.top, l1.bottom, l2.top, l2.bottom};
    cv::Rect bounding_rect = cv::boundingRect(pts);

    for (const auto& test : lights) {
        if (test.rect.center == l1.rect.center || test.rect.center == l2.rect.center) continue;

        if (bounding_rect.contains(test.top) || bounding_rect.contains(test.bottom) ||
            bounding_rect.contains(test.rect.center)) {
            return true;
        }
    }

    return false;
}

bool ArmorDetector::isArmor(const LightBar& l1, const LightBar& l2) {
    // 两根灯条长度应接近
    double max_len = std::max(l1.length, l2.length);
    double min_len = std::min(l1.length, l2.length);
    if (max_len <= 1e-6) return false;
    double light_ratio = min_len / max_len;
    if (light_ratio < param_.armor.min_light_ratio) return false;

    // 中心距离应在合理范围
    double avg_len = (l1.length + l2.length) * 0.5;
    if (avg_len <= 1e-6) return false;
    double center_distance = cv::norm(l1.rect.center - l2.rect.center) / avg_len;
    if (center_distance < param_.armor.min_center_distance ||
        center_distance > param_.armor.max_center_distance) {
        return false;
    }

    // 中心连线不能太倾斜
    cv::Point2f diff = l1.rect.center - l2.rect.center;
    if (std::abs(diff.x) <= 1e-6) return false;
    double angle = std::abs(std::atan(diff.y / diff.x)) * 180.0 / CV_PI;
    if (angle > param_.armor.max_angle) return false;

    return true;
}

std::vector<Armor> ArmorDetector::detect(const cv::Mat& src) {
    std::vector<Armor> empty;
    if (src.empty()) return empty;

    binary_img_ = preprocessImage(src);
    std::vector<LightBar> lights = findLights(src, binary_img_);
    return matchLights(lights);
}

cv::Mat ArmorDetector::visualize(const cv::Mat& src, const std::vector<Armor>& armors) const {
    cv::Mat dst = src.clone();

    for (const auto& armor : armors) {
        // 画装甲板外框
        cv::Point pts[4];
        for (int i = 0; i < 4; ++i) {
            pts[i] = cv::Point(static_cast<int>(armor.corners[i].x),
                               static_cast<int>(armor.corners[i].y));
        }
        const cv::Point* ppt[1] = {pts};
        int npts = 4;
        cv::polylines(dst, ppt, &npts, 1, true, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

        // 画角点
        for (int i = 0; i < 4; ++i) {
            cv::circle(dst, armor.corners[i], 6, cv::Scalar(0, 255, 0), -1, cv::LINE_AA);
            cv::putText(dst, std::to_string(i), armor.corners[i] + cv::Point2f(8, -8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
        }

        // 画中心连线
        cv::line(dst, armor.left.rect.center, armor.right.rect.center,
                 cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    }

    return dst;
}

} // namespace rm
