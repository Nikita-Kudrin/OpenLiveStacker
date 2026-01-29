#include "depth_anything_v3.h"
#include <opencv2/imgcodecs.hpp>
#include <booster/log.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void process_image(ols::DepthAnythingV3& depth_model, const fs::path& image_path)
{
    std::cout << "Processing: " << image_path << std::endl;
    cv::Mat image = cv::imread(image_path.string());
    if (image.empty())
    {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return;
    }

    cv::Mat depth = depth_model.estimate_depth(image);
    if (depth.empty())
    {
        std::cerr << "Failed to estimate depth for: " << image_path << std::endl;
        return;
    }

    // Save depth map
    cv::Mat depth_8u;
    depth.convertTo(depth_8u, CV_8U, 255.0);
    std::string ext = image_path.extension().string();
    std::string stem = image_path.stem().string();
    fs::path depth_path = image_path.parent_path() / (stem + "_depth_v3" + ext);

    if (cv::imwrite(depth_path.string(), depth_8u))
    {
        std::cout << "  Depth map saved to: " << depth_path.filename().string() << std::endl;
    }
    else
    {
        std::cerr << "  Failed to save depth map to: " << depth_path.string() << std::endl;
    }

    // Save SBS stereo image
    cv::Mat sbs = depth_model.create_sbs_stereo(image, depth);
    if (!sbs.empty())
    {
        fs::path sbs_path = image_path.parent_path() / (stem + "_sbs_v3" + ext);
        if (cv::imwrite(sbs_path.string(), sbs))
        {
            std::cout << "  SBS image saved to: " << sbs_path.filename().string() << std::endl;
        }
        else
        {
            std::cerr << "  Failed to save SBS image to: " << sbs_path.string() << std::endl;
        }
    }
}

int main(int argc, char** argv)
{
    booster::log::logger::instance().set_default_level(booster::log::debug);
    booster::log::logger::instance().add_sink(std::make_shared<booster::log::sinks::standard_error>());

    std::string data_dir = "test/data/original_for_3d/";
    std::string model_path = (argc >= 2) ? argv[1] : "";

    if (model_path.empty())
    {
        if (fs::exists("data/depth_anything_v3.onnx"))
        {
            model_path = "data/depth_anything_v3.onnx";
        }
        else if (fs::exists("cmake-build-debug/data/depth_anything_v3.onnx"))
        {
            model_path = "cmake-build-debug/data/depth_anything_v3.onnx";
        }
        else if (fs::exists("cmake-build-release/data/depth_anything_v3.onnx"))
        {
            model_path = "cmake-build-release/data/depth_anything_v3.onnx";
        }
        else
        {
            model_path = "data/depth_anything_v3.onnx";
        }
    }

    if (!fs::exists(data_dir))
    {
        // Try absolute path if relative fails
        data_dir = "/opt/GitHub/OpenLiveStacker/test/data/original_for_3d/";
        if (!fs::exists(data_dir))
        {
            std::cerr <<
                "Data directory does not exist: test/data/original_for_3d/ or /opt/GitHub/OpenLiveStacker/test/data/original_for_3d/"
                << std::endl;
            return 1;
        }
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

    for (const auto& entry : fs::directory_iterator(data_dir))
    {
        if (entry.is_regular_file())
        {
            std::string ext = entry.path().extension().string();
            // Convert extension to lowercase for comparison
            std::string ext_lower = ext;
            for (auto& c : ext_lower) c = std::tolower(c);

            if (ext_lower == ".png" || ext_lower == ".jpg" || ext_lower == ".jpeg" || ext_lower == ".tiff" || ext_lower
                == ".tif")
            {
                // Skip already processed images
                std::string stem = entry.path().stem().string();
                if (stem.find("_depth_v3") != std::string::npos ||
                    stem.find("_sbs_v3") != std::string::npos)
                {
                    continue;
                }
                process_image(depth_model, entry.path());
            }
        }
    }

    return 0;
}
