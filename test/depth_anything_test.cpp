#include "depth_anything_v3.h"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>
#include <filesystem>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <image_path> [model_path]" << std::endl;
        return 1;
    }

    std::string image_path = argv[1];
    std::string model_path = (argc >= 3) ? argv[2] : "data/depth_anything_v3.onnx";

    if (!std::filesystem::exists(image_path))
    {
        std::cerr << "Image file does not exist: " << image_path << std::endl;
        return 1;
    }

    cv::Mat image = cv::imread(image_path);
    if (image.empty())
    {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return 1;
    }

    ols::DepthAnythingV3 depth_model;
    std::cout << "Loading model from: " << model_path << std::endl;
    if (!depth_model.load(model_path))
    {
        std::cerr << "Failed to load model: " << model_path << std::endl;
        std::cerr << "Note: This might be due to an old OpenCV version or a missing model file." << std::endl;
        return 1;
    }
    std::cout << "Model loaded successfully." << std::endl;

    cv::Mat depth = depth_model.estimate_depth(image);
    if (depth.empty())
    {
        std::cerr << "Failed to estimate depth." << std::endl;
        return 1;
    }

    // Convert depth map (0-1 float) to 8-bit for saving (0-255)
    cv::Mat depth_8u;
    depth.convertTo(depth_8u, CV_8U, 255.0);

    // Generate output path: suffix "_depth_v3" before extension
    std::filesystem::path p(image_path);
    std::string ext = p.extension().string();
    std::string stem = p.stem().string();
    std::filesystem::path output_path = p.parent_path() / (stem + "_depth_v3" + ext);

    if (cv::imwrite(output_path.string(), depth_8u))
    {
        std::cout << "Depth map saved to: " << output_path.string() << std::endl;
    }
    else
    {
        std::cerr << "Failed to save depth map to: " << output_path.string() << std::endl;
        return 1;
    }

    return 0;
}
