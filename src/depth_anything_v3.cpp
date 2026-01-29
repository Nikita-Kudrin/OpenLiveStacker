#include "depth_anything_v3.h"
#include <booster/log.h>
#include <vector>
#include <numeric>

namespace ols
{
    DepthAnythingV3::DepthAnythingV3()
    {
    }

    bool DepthAnythingV3::load(std::string const& model_path)
    {
        try
        {
            // Initialize ORT Environment
            env_ = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "DepthAnythingV3");

            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(4);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            // Try to enable CUDA if available (requires onnxruntime-gpu)
            // try { OrtSessionOptionsAppendExecutionProvider_CUDA(session_options, 0); } catch(...) {}

            session_ = std::make_shared<Ort::Session>(*env_, model_path.c_str(), session_options);
            loaded_ = true;

            BOOSTER_INFO("stacker") << "Depth Anything v3 loaded via ONNX Runtime from " << model_path;
            return true;
        }
        catch (const Ort::Exception& e)
        {
            BOOSTER_ERROR("stacker") << "Failed to load model: " << e.what();
            return false;
        }
    }

    cv::Mat DepthAnythingV3::estimate_depth(cv::Mat const& input)
    {
        if (!loaded_ || input.empty()) return cv::Mat();

        // 1. Pre-processing
        cv::Mat resized, float_img;
        cv::resize(input, resized, cv::Size(input_width_, input_height_));
        resized.convertTo(float_img, CV_32F, 1.0 / 255.0);

        // Normalize (Mean: [0.485, 0.456, 0.406], Std: [0.229, 0.224, 0.225])
        cv::Mat mean(input_height_, input_width_, CV_32FC3, cv::Scalar(0.485, 0.456, 0.406));
        cv::Mat std(input_height_, input_width_, CV_32FC3, cv::Scalar(0.229, 0.224, 0.225));
        cv::subtract(float_img, mean, float_img);
        cv::divide(float_img, std, float_img);

        // 2. Prepare Input Tensor [1, 1, 3, H, W] (5 Dimensions)
        // We flatten the image from HWC to CHW
        cv::Mat channels[3];
        cv::split(float_img, channels);

        std::vector<float> input_tensor_values;
        input_tensor_values.reserve(1 * 1 * 3 * input_height_ * input_width_);

        // Push data in CHW order
        for (int i = 0; i < 3; ++i)
        {
            input_tensor_values.insert(input_tensor_values.end(), (float*)channels[i].data,
                                       (float*)channels[i].data + input_height_ * input_width_);
        }

        // Define 5D shape
        std::vector<int64_t> input_node_dims = {1, 1, 3, input_height_, input_width_};

        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_values.data(), input_tensor_values.size(), input_node_dims.data(),
            input_node_dims.size()
        );

        // 3. Run Inference
        const char* input_names[] = {"pixel_values"};
        const char* output_names[] = {"predicted_depth"};

        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1
        );

        // 4. Post-processing
        float* depth_data = output_tensors[0].GetTensorMutableData<float>();

        // Create Mat from output (assuming output is 1x1xHxW or similar)
        // We wrap the raw pointer. Note: This pointer is valid only while 'output_tensors' is in scope.
        // We clone it immediately to be safe.
        cv::Mat raw_depth(input_height_, input_width_, CV_32F, depth_data);

        cv::Mat result;
        cv::resize(raw_depth, result, input.size());

        // Normalize 0-1 for visualization/processing
        double minVal, maxVal;
        cv::minMaxLoc(result, &minVal, &maxVal);
        if (maxVal > minVal)
        {
            result = (result - minVal) / (maxVal - minVal);
        }

        return result.clone();
    }

    cv::Mat DepthAnythingV3::create_sbs_stereo(cv::Mat const& image, cv::Mat const& depth, float shift_scale)
    {
        if (image.empty() || depth.empty()) return image;

        int w = image.cols;
        int h = image.rows;
        cv::Mat left = cv::Mat::zeros(image.size(), image.type());
        cv::Mat right = cv::Mat::zeros(image.size(), image.type());

        float max_shift = w * shift_scale;

        for (int y = 0; y < h; ++y)
        {
            // Track the last written pixel position to detect gaps
            int last_xl = -1;
            int last_xr = -1;

            // Pointers for faster access (optional, but good for loops)
            const cv::Vec3b* img_row = image.ptr<cv::Vec3b>(y);
            const float* depth_row = depth.ptr<float>(y);
            cv::Vec3b* left_row = left.ptr<cv::Vec3b>(y);
            cv::Vec3b* right_row = right.ptr<cv::Vec3b>(y);

            for (int x = 0; x < w; ++x)
            {
                float d = depth_row[x];
                int shift = static_cast<int>(d * max_shift);

                // Calculate target positions
                int xl = x - shift;
                int xr = x + shift;

                cv::Vec3b color = img_row[x];

                // --- LEFT IMAGE ---
                if (xl >= 0 && xl < w)
                {
                    left_row[xl] = color;

                    // Gap Filling: If we skipped pixels (grooves), fill them
                    // We limit the fill to small gaps (< 5 pixels) to avoid filling actual object edges
                    if (last_xl != -1 && (xl > last_xl + 1) && (xl - last_xl < 5))
                    {
                        for (int k = last_xl + 1; k < xl; ++k)
                        {
                            left_row[k] = color;
                        }
                    }
                    last_xl = xl;
                }

                // --- RIGHT IMAGE ---
                if (xr >= 0 && xr < w)
                {
                    right_row[xr] = color;

                    // Gap Filling for Right Image
                    if (last_xr != -1 && (xr > last_xr + 1) && (xr - last_xr < 5))
                    {
                        for (int k = last_xr + 1; k < xr; ++k)
                        {
                            right_row[k] = color;
                        }
                    }
                    last_xr = xr;
                }
            }
        }

        cv::Mat sbs;
        cv::hconcat(left, right, sbs);
        return sbs;
    }
} // namespace
