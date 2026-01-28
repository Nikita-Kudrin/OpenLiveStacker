#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

namespace ols {
    class DepthAnythingV3 {
    public:
        DepthAnythingV3();
        bool load(std::string const &model_path);
        bool is_loaded() const { return loaded_; }
        cv::Mat estimate_depth(cv::Mat const &input);
        cv::Mat create_sbs_stereo(cv::Mat const &image, cv::Mat const &depth, float shift_scale = 0.05f);

    private:
        cv::dnn::Net net_;
        bool loaded_ = false;
        cv::Size input_size_ = cv::Size(518, 518);
    };
}
