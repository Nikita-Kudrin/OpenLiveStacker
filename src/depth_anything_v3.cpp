#include "depth_anything_v3.h"
#include <booster/log.h>
#include <opencv2/core/version.hpp>
#include <fstream>
#include <vector>

namespace ols {

DepthAnythingV3::DepthAnythingV3() {}

bool DepthAnythingV3::load(std::string const &model_path) {
    std::vector<std::string> candidates = { model_path };
    
    // If we were looking for depth_anything_v3.onnx, also look for model.onnx as a fallback
    if (model_path.size() >= 22 && model_path.substr(model_path.size() - 22) == "depth_anything_v3.onnx") {
        candidates.push_back(model_path.substr(0, model_path.size() - 22) + "model.onnx");
    }

    for (const auto& path : candidates) {
        try {
            std::ifstream f(path);
            if (!f.good()) {
                continue;
            }
            f.close();

            net_ = cv::dnn::readNet(path);
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            loaded_ = true;
            BOOSTER_INFO("stacker") << "Depth Anything v3 model loaded from " << path << " (OpenCV " << CV_VERSION << ")";
            return true;
        } catch (cv::Exception const &e) {
            std::string err = e.what();
            BOOSTER_ERROR("stacker") << "Failed to load Depth Anything v3 model from " << path << ": " << err;
            BOOSTER_ERROR("stacker") << "OpenCV version: " << CV_VERSION;
            if (err.find("getInputNodeId") != std::string::npos || err.find("Input node with name") != std::string::npos) {
                BOOSTER_ERROR("stacker") << "This error often occurs with older OpenCV versions when parsing newer ONNX models.";
                
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR == 5 && CV_VERSION_REVISION < 5
                BOOSTER_ERROR("stacker") << "CRITICAL: Your OpenCV version (4.5." << CV_VERSION_REVISION 
                                         << ") does not support ONNX models with external weights (.onnx_data files). "
                                         << "This support was added in OpenCV 4.5.5. Since your model uses external weights, "
                                         << "you MUST either upgrade OpenCV to >= 4.5.5 or embed the weights into the .onnx file.";
#endif
                BOOSTER_ERROR("stacker") << "Try simplifying the model using 'onnxsim' (pip install onnxsim; onnxsim model.onnx simplified.onnx) "
                                         << "and use the simplified model as depth_anything_v3.onnx.";
            }
            BOOSTER_ERROR("stacker") << "Note: if you have a .onnx_data file, ensure it is in the same directory as the .onnx file and has the correct name referenced by the model.";
            // If it failed to parse, don't try fallback as it might be the same model or just broken
            return false;
        }
    }
    
    BOOSTER_WARNING("stacker") << "Depth Anything v3 model file not found in " << model_path << ". 3D feature will be disabled. See README.md for instructions.";
    return false;
}

cv::Mat DepthAnythingV3::estimate_depth(cv::Mat const &input) {
    if (!loaded_) return cv::Mat();

    cv::Mat blob;
    cv::dnn::blobFromImage(input, blob, 1.0 / 255.0, input_size_, cv::Scalar(0.485, 0.456, 0.406), true, false);
    // Note: normalization values might need adjustment for v3
    
    net_.setInput(blob);
    cv::Mat depth = net_.forward();

    // Resize back to original size
    cv::Mat out;
    cv::resize(depth.reshape(1, input_size_.height), out, input.size());
    
    // Normalize depth to 0-1
    double minVal, maxVal;
    cv::minMaxLoc(out, &minVal, &maxVal);
    out = (out - minVal) / (maxVal - minVal);
    
    return out;
}

cv::Mat DepthAnythingV3::create_sbs_stereo(cv::Mat const &image, cv::Mat const &depth, float shift_scale) {
    if (image.empty() || depth.empty()) return image;

    int w = image.cols;
    int h = image.rows;
    cv::Mat left = cv::Mat::zeros(image.size(), image.type());
    cv::Mat right = cv::Mat::zeros(image.size(), image.type());

    float max_shift = w * shift_scale;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float d = depth.at<float>(y, x);
            int shift = static_cast<int>(d * max_shift);

            int xl = x - shift;
            int xr = x + shift;

            if (xl >= 0 && xl < w) {
                left.at<cv::Vec3b>(y, xl) = image.at<cv::Vec3b>(y, x);
            }
            if (xr >= 0 && xr < w) {
                right.at<cv::Vec3b>(y, xr) = image.at<cv::Vec3b>(y, x);
            }
        }
    }
    
    // Simple hole filling (inpaint would be better but slow)
    // For now just return concatenated
    cv::Mat sbs;
    cv::hconcat(left, right, sbs);
    return sbs;
}

} // namespace
