#pragma once
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h> // Include ORT
#include <memory>
#include <string>

namespace ols
{
    class DepthAnythingV3
    {
    public:
        DepthAnythingV3();
        bool load(std::string const& model_path);
        bool is_loaded() const { return loaded_; }
        cv::Mat estimate_depth(cv::Mat const& input);
        cv::Mat create_sbs_stereo(cv::Mat const& image, cv::Mat const& depth, float shift_scale = 0.05f);

    private:
        // ORT Resources
        std::shared_ptr<Ort::Env> env_;
        std::shared_ptr<Ort::Session> session_;
        bool loaded_ = false;

        // Fixed input size for the model (must match onnxsim export)
        const int input_width_ = 518;
        const int input_height_ = 518;
    };
} // namespace
