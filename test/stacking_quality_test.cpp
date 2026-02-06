#include <gtest/gtest.h>
#include "data_items.h"
#include "processors.h"
#include "util.h"
#include "tiffmat.h"
#ifdef WITH_CFITSIO
#include "fitsmat.h"
#endif
#include "hot_removal.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <booster/log.h>
#include <cppcms/json.h>
#include <thread>
#include <dirent.h>
#include <algorithm>

using namespace ols;

class StackingQualityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static bool initialized = false;
        if (!initialized)
        {
            booster::log::logger::instance().set_default_level(booster::log::debug);
            booster::log::logger::instance().add_sink(std::make_shared<booster::log::sinks::standard_error>());
            initialized = true;
        }
    }

    struct QualityMetrics
    {
        double mean_brightness;
        double std_dev;
        double max_val;
        double min_val;
        cv::Scalar channel_means;
        int channels;
    };

    QualityMetrics compute_metrics(const cv::Mat& img)
    {
        QualityMetrics m;
        cv::Scalar mean, stddev;
        cv::meanStdDev(img, mean, stddev);

        m.channels = img.channels();
        m.mean_brightness = 0;
        for (int i = 0; i < m.channels; ++i) m.mean_brightness += mean[i];
        m.mean_brightness /= m.channels;

        m.std_dev = 0;
        for (int i = 0; i < m.channels; ++i) m.std_dev += stddev[i];
        m.std_dev /= m.channels;

        double minV, maxV;
        cv::minMaxLoc(img, &minV, &maxV);
        m.min_val = minV;
        m.max_val = maxV;
        m.channel_means = mean;
        return m;
    }

    std::vector<std::string> list_files(std::string const& dir)
    {
        std::vector<std::string> files;
        DIR* dp;
        struct dirent* dirp;
        if ((dp = opendir(dir.c_str())) == NULL)
        {
            return files;
        }

        while ((dirp = readdir(dp)) != NULL)
        {
            std::string name = dirp->d_name;
            if (name.size() > 5)
            {
                std::string ext = name.substr(name.size() - 4);
                if (ext == ".tif" || ext == "tiff" || ext == ".jpg" || ext == "jpeg" || ext == "fits" || ext == ".fit")
                {
                    files.push_back(dir + "/" + name);
                }
            }
        }
        closedir(dp);
        std::sort(files.begin(), files.end());
        return files;
    }

    cv::Mat load_any_image(std::string const& path, int& dr, CamBayerType& bayer)
    {
        cv::Mat img;
        bayer = bayer_na;
        if (path.find(".tiff") != std::string::npos || path.find(".tif") != std::string::npos)
        {
            img = load_tiff(path);
            dr = (1ll << (8 * img.elemSize1())) - 1;
        }
#ifdef WITH_CFITSIO
        else if (path.find(".fits") != std::string::npos || path.find(".fit") != std::string::npos)
        {
            auto res = load_fits(path);
            img = res.first;
            bayer = res.second;
            dr = (1ll << (8 * img.elemSize1())) - 1;
        }
#endif
        else
        {
            img = cv::imread(path);
            dr = 255;
        }
        return img;
    }

    cv::Mat run_stacking(std::string const& dir, StackerControl const& initial_cfg, CamBayerType bayer_pattern)
    {
        queue_pointer_type input_queue(new queue_type(10));
        queue_pointer_type stacker_queue(new queue_type(10));
        queue_pointer_type pp_queue(new queue_type(10));
        queue_pointer_type output_queue(new queue_type(10));

        std::shared_ptr<CameraFrame> last_result_frame;
        output_queue->call_on_push([&](std::shared_ptr<QueueData> p)
        {
            auto frame = std::dynamic_pointer_cast<CameraFrame>(p);
            if (frame && frame->jpeg_frame)
            {
                // Ignore dummy frames (checkerboard)
                if (frame->format.format == stream_mjpeg)
                {
                    return;
                }
                last_result_frame = frame;
            }
        });

        std::thread t1, t2, t3;
        try
        {
            t1 = start_preprocessor(input_queue, stacker_queue, nullptr);
            t2 = start_stacker(stacker_queue, pp_queue);
            t3 = start_post_processor(pp_queue, output_queue, nullptr, nullptr, "/tmp");

            input_queue->push(std::make_shared<StackerControl>(initial_cfg));

            auto files = list_files(dir);
            double ts = 0;
            for (auto const& f : files)
            {
                int dr;
                CamBayerType bayer;
                cv::Mat img = load_any_image(f, dr, bayer);
                if (img.empty())
                {
                    continue;
                }

                if (bayer == bayer_na) bayer = bayer_pattern;

                if (bayer != bayer_na && img.channels() == 1)
                {
                    cv::Mat rgb;
                    switch (bayer)
                    {
                    case bayer_rg: cv::cvtColor(img, rgb, cv::COLOR_BayerBG2BGR);
                        break;
                    case bayer_gr: cv::cvtColor(img, rgb, cv::COLOR_BayerGB2BGR);
                        break;
                    case bayer_bg: cv::cvtColor(img, rgb, cv::COLOR_BayerRG2BGR);
                        break;
                    case bayer_gb: cv::cvtColor(img, rgb, cv::COLOR_BayerGR2BGR);
                        break;
                    default: cv::cvtColor(img, rgb, cv::COLOR_GRAY2BGR);
                        break;
                    }
                    img = rgb;
                }

                auto frame = std::make_shared<CameraFrame>();
                frame->frame = img;
                frame->frame_dr = dr;
                frame->timestamp = ts;
                ts += 1.0;
                input_queue->push(frame);
            }

            auto save_ctl = std::make_shared<StackerControl>();
            save_ctl->op = StackerControl::ctl_save;
            input_queue->push(save_ctl);
        }
        catch (std::exception const& e)
        {
            BOOSTER_ERROR("test") << "Exception in run_stacking: " << e.what();
        }

        input_queue->push(std::make_shared<ShutDownData>());

        if (t1.joinable()) t1.join();
        if (t2.joinable()) t2.join();
        if (t3.joinable()) t3.join();

        if (last_result_frame && last_result_frame->jpeg_frame)
        {
            std::vector<unsigned char> data(last_result_frame->jpeg_frame->size());
            memcpy(data.data(), last_result_frame->jpeg_frame->data(), data.size());
            return cv::imdecode(data, cv::IMREAD_COLOR);
        }
        return cv::Mat();
    }
};

TEST_F(StackingQualityTest, DSOSampleTest)
{
    StackerControl cfg;
    cfg.name = "dso_test";
    cfg.width = 1304;
    cfg.height = 976;
    cfg.method = stack_dso;
    cfg.auto_stretch = true;
    cfg.stretch_low = 1.0;
    cfg.stretch_high = 0.0;
    cfg.stretch_gamma = 0.4;
    cfg.derotate = true;

    // Use sim directory as it has known good frames
    cv::Mat result = run_stacking("sim", cfg, bayer_na);
    ASSERT_FALSE(result.empty()) << "Stacking failed to produce an image";

    auto metrics = compute_metrics(result);

    // Stretched image (8-bit) should have reasonable brightness
    EXPECT_GT(metrics.mean_brightness, 5) << "Image is too dark";
    EXPECT_LT(metrics.mean_brightness, 200) << "Image is too bright";
    EXPECT_GT(metrics.std_dev, 2) << "Image lacks contrast";

    // Check that it's not monochromatic if input was color
    if (metrics.channels == 3)
    {
        EXPECT_GT(std::abs(metrics.channel_means[0] - metrics.channel_means[1]) +
                  std::abs(metrics.channel_means[1] - metrics.channel_means[2]), 0.1)
            << "Image appears to be grayscale but color was expected";
    }
}

TEST_F(StackingQualityTest, PlanetarySampleTest)
{
    StackerControl cfg;
    cfg.name = "planetary_test";
    cfg.width = 1304;
    cfg.height = 976;
    cfg.method = stack_planetary;
    cfg.auto_stretch = true;
    cfg.stretch_low = 1.0;
    cfg.stretch_high = 0.0;
    cfg.stretch_gamma = 1.0;
    cfg.derotate = false;

    cv::Mat result = run_stacking("sim", cfg, bayer_na);
    ASSERT_FALSE(result.empty()) << "Stacking failed to produce an image";

    auto metrics = compute_metrics(result);
    // For planetary, we care about the object being bright, not the whole image mean
    EXPECT_GT(metrics.max_val, 150) << "Image max value is too low";
    EXPECT_GT(metrics.std_dev, 2);
}

TEST_F(StackingQualityTest, SingleFrameTest)
{
    // Create a temp directory with one frame
    system("mkdir -p /tmp/ols_test_single && cp sim/frame_00000000.jpeg /tmp/ols_test_single/");

    StackerControl cfg;
    cfg.width = 1304;
    cfg.height = 976;
    cfg.method = stack_dso;
    cfg.auto_stretch = true;

    cv::Mat result = run_stacking("/tmp/ols_test_single", cfg, bayer_na);
    ASSERT_FALSE(result.empty());

    auto metrics = compute_metrics(result);
    EXPECT_GT(metrics.mean_brightness, 5);

    system("rm -rf /tmp/ols_test_single");
}

TEST_F(StackingQualityTest, EmptyDirectoryTest)
{
    system("mkdir -p /tmp/ols_test_empty");

    StackerControl cfg;
    cfg.width = 1304;
    cfg.height = 976;
    cfg.method = stack_dso;

    cv::Mat result = run_stacking("/tmp/ols_test_empty", cfg, bayer_na);
    EXPECT_TRUE(result.empty());

    system("rm -rf /tmp/ols_test_empty");
}
