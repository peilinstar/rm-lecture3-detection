#ifndef ARMOR_DETECTOR_HPP
#define ARMOR_DETECTOR_HPP

#include <opencv2/opencv.hpp>
#include <vector>

namespace rm {

enum class ArmorColor { RED = 0, BLUE = 1 };

struct LightParams {
    double min_ratio = 0.1;  ///< 灯条短边/长边最小比值
    double max_ratio = 0.6;  ///< 灯条短边/长边最大比值
    double max_angle = 35.0; ///< 灯条最大倾斜角（与竖直方向的夹角，度）
};

struct ArmorParams {
    double min_light_ratio = 0.7;   ///< 两根灯条长度之比下限
    double min_center_distance = 1.5; ///< 两灯条中心距离 / 平均灯条长度 下限
    double max_center_distance = 5.0; ///< 两灯条中心距离 / 平均灯条长度 上限
    double max_angle = 30.0;        ///< 装甲板中心连线与水平方向的最大夹角
};

struct ArmorParam {
    int binary_thres = 180;          ///< 灰度二值化阈值
    ArmorColor color = ArmorColor::BLUE;
    LightParams light;
    ArmorParams armor;
};

/**
 * @brief 候选灯条
 *
 * 参考 rm_vision 的做法：
 * - 对 minAreaRect 的四个角点按 y 排序，取上面两点的中点作为 top，
 *   下面两点的中点作为 bottom。
 * - 这样 top-bottom 连线一定沿着灯条长边方向，不会歪。
 */
struct LightBar {
    LightBar() = default;

    cv::RotatedRect rect;
    cv::Point2f top;       ///< 灯条上端中点
    cv::Point2f bottom;    ///< 灯条下端中点
    double length = 0.0;   ///< 长边长度
    double width = 0.0;    ///< 短边长度
    double tilt_angle = 0.0; ///< 与竖直方向的夹角（度）
    ArmorColor color = ArmorColor::BLUE;

    explicit LightBar(const cv::RotatedRect& box);
};

struct Armor {
    Armor() = default;
    Armor(const LightBar& l1, const LightBar& l2);

    LightBar left;
    LightBar right;
    cv::Point2f center;
    cv::Point2f corners[4]; // tl, tr, br, bl
};

class ArmorDetector {
public:
    explicit ArmorDetector(const ArmorParam& param = ArmorParam());

    void setParam(const ArmorParam& param);
    void setColor(ArmorColor color);

    std::vector<Armor> detect(const cv::Mat& src);

    cv::Mat preprocessImage(const cv::Mat& src);
    std::vector<LightBar> findLights(const cv::Mat& src, const cv::Mat& binary);
    std::vector<Armor> matchLights(const std::vector<LightBar>& lights);

    cv::Mat visualize(const cv::Mat& src, const std::vector<Armor>& armors) const;
    cv::Mat getBinaryImage() const { return binary_img_; }

private:
    ArmorParam param_;
    cv::Mat binary_img_;

    bool isLight(const LightBar& light);
    bool containLight(const LightBar& l1, const LightBar& l2, const std::vector<LightBar>& lights);
    bool isArmor(const LightBar& l1, const LightBar& l2);
};

} // namespace rm

#endif // ARMOR_DETECTOR_HPP
