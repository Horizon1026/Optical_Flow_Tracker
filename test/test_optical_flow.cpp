#include "iostream"
#include <cstdint>
#include <string>
#include <vector>
#include <ctime>
#include <thread>

#include "opencv2/opencv.hpp"

#include "log_report.h"

#include "optical_flow_lk.h"
#include "optical_flow_klt.h"

#define CONFIG_OPENCV_DRAW (1)
#define FEATURES_TO_TRACK (200)

std::string test_ref_image_file_name = "../example/optical_flow/ref_image.png";
std::string test_cur_image_file_name = "../example/optical_flow/cur_image.png";

float TestLkOpticalFlow(int32_t pyramid_level, int32_t patch_size, uint8_t method) {
    cv::Mat cv_ref_image, cv_cur_image;
    cv_ref_image = cv::imread(test_ref_image_file_name, 0);
    cv_cur_image = cv::imread(test_cur_image_file_name, 0);

    ImagePyramid ref_pyramid, cur_pyramid;
    ref_pyramid.SetPyramidBuff((uint8_t *)malloc(sizeof(uint8_t) * cv_ref_image.rows * cv_ref_image.cols));
    cur_pyramid.SetPyramidBuff((uint8_t *)malloc(sizeof(uint8_t) * cv_cur_image.rows * cv_cur_image.cols));
    cur_pyramid.SetRawImage(cv_cur_image.data, cv_cur_image.rows, cv_cur_image.cols);
    ref_pyramid.SetRawImage(cv_ref_image.data, cv_ref_image.rows, cv_ref_image.cols);

    std::vector<cv::Point2f> ref_corners;
    cv::goodFeaturesToTrack(cv_ref_image, ref_corners, FEATURES_TO_TRACK, 0.01, 20);

    FEATURE_TRACKER::OpticalFlowLk lk;
    std::vector<Eigen::Vector2f> ref_pixel_uv, cur_pixel_uv;
    std::vector<uint8_t> status;
    ref_pixel_uv.reserve(ref_corners.size());
    for (uint32_t i = 0; i < ref_corners.size(); ++i) {
        ref_pixel_uv.emplace_back(Eigen::Vector2f(ref_corners[i].x, ref_corners[i].y));
    }

    lk.options().kPatchRowHalfSize = patch_size;
    lk.options().kPatchColHalfSize = patch_size;
    lk.options().kMethod = static_cast<FEATURE_TRACKER::OpticalFlowMethod>(method);

    clock_t begin, end;
    begin = clock();
    ref_pyramid.CreateImagePyramid(pyramid_level);
    cur_pyramid.CreateImagePyramid(pyramid_level);
    lk.TrackMultipleLevel(ref_pyramid, cur_pyramid, ref_pixel_uv, cur_pixel_uv, status);
    end = clock();
    const float cost_time = static_cast<float>(end - begin)/ CLOCKS_PER_SEC * 1000.0f;

#if CONFIG_OPENCV_DRAW
    cv::Mat show_ref_image(cv_ref_image.rows, cv_ref_image.cols, CV_8UC3);
    cv::cvtColor(cv_ref_image, show_ref_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < ref_corners.size(); i++) {
        cv::circle(show_ref_image, cv::Point2f(ref_pixel_uv[i].x(), ref_pixel_uv[i].y()), 2, cv::Scalar(255, 255, 0), 3);
    }
    cv::imshow("LK : Feature before multi tracking", show_ref_image);

    cv::Mat show_cur_image(cv_cur_image.rows, cv_cur_image.cols, CV_8UC3);
    cv::cvtColor(cv_cur_image, show_cur_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < ref_corners.size(); i++) {
        if (status[i] != static_cast<uint8_t>(FEATURE_TRACKER::TrackStatus::TRACKED)) {
            continue;
        }
        cv::circle(show_cur_image, cv::Point2f(cur_pixel_uv[i].x(), cur_pixel_uv[i].y()), 2, cv::Scalar(0, 0, 255), 3);
        cv::line(show_cur_image, cv::Point2f(ref_pixel_uv[i].x(), ref_pixel_uv[i].y()), cv::Point2f(cur_pixel_uv[i].x(), cur_pixel_uv[i].y()), cv::Scalar(0, 255, 0), 2);
    }
    cv::imshow("LK : Feature after multi tracking", show_cur_image);

    cv::waitKey(0);
#endif

    free(ref_pyramid.data());
    free(cur_pyramid.data());

    return cost_time;
}

float TestKltOpticalFlow(int32_t pyramid_level, int32_t patch_size, uint8_t method) {
    cv::Mat cv_ref_image, cv_cur_image;
    cv_ref_image = cv::imread(test_ref_image_file_name, 0);
    cv_cur_image = cv::imread(test_cur_image_file_name, 0);

    ImagePyramid ref_pyramid, cur_pyramid;
    ref_pyramid.SetPyramidBuff((uint8_t *)malloc(sizeof(uint8_t) * cv_ref_image.rows * cv_ref_image.cols));
    cur_pyramid.SetPyramidBuff((uint8_t *)malloc(sizeof(uint8_t) * cv_cur_image.rows * cv_cur_image.cols));
    cur_pyramid.SetRawImage(cv_cur_image.data, cv_cur_image.rows, cv_cur_image.cols);
    ref_pyramid.SetRawImage(cv_ref_image.data, cv_ref_image.rows, cv_ref_image.cols);

    std::vector<cv::Point2f> ref_corners;
    cv::goodFeaturesToTrack(cv_ref_image, ref_corners, FEATURES_TO_TRACK, 0.01, 20);

    FEATURE_TRACKER::OpticalFlowKlt klt;
    std::vector<Eigen::Vector2f> ref_pixel_uv, cur_pixel_uv;
    std::vector<uint8_t> status;
    ref_pixel_uv.reserve(ref_corners.size());
    for (uint32_t i = 0; i < ref_corners.size(); ++i) {
        ref_pixel_uv.emplace_back(Eigen::Vector2f(ref_corners[i].x, ref_corners[i].y));
    }

    klt.options().kPatchRowHalfSize = patch_size;
    klt.options().kPatchColHalfSize = patch_size;
    klt.options().kMethod = static_cast<FEATURE_TRACKER::OpticalFlowMethod>(method);

    clock_t begin, end;
    begin = clock();
    ref_pyramid.CreateImagePyramid(pyramid_level);
    cur_pyramid.CreateImagePyramid(pyramid_level);
    klt.TrackMultipleLevel(ref_pyramid, cur_pyramid, ref_pixel_uv, cur_pixel_uv, status);
    end = clock();
    const float cost_time = static_cast<float>(end - begin)/ CLOCKS_PER_SEC * 1000.0f;

#if CONFIG_OPENCV_DRAW
    cv::Mat show_ref_image(cv_ref_image.rows, cv_ref_image.cols, CV_8UC3);
    cv::cvtColor(cv_ref_image, show_ref_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < ref_corners.size(); i++) {
        cv::circle(show_ref_image, cv::Point2f(ref_pixel_uv[i].x(), ref_pixel_uv[i].y()), 2, cv::Scalar(255, 255, 0), 3);
    }
    cv::imshow("KLT : Feature before multi tracking", show_ref_image);

    cv::Mat show_cur_image(cv_cur_image.rows, cv_cur_image.cols, CV_8UC3);
    cv::cvtColor(cv_cur_image, show_cur_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < ref_corners.size(); i++) {
        if (status[i] != static_cast<uint8_t>(FEATURE_TRACKER::TrackStatus::TRACKED)) {
            continue;
        }
        cv::circle(show_cur_image, cv::Point2f(cur_pixel_uv[i].x(), cur_pixel_uv[i].y()), 2, cv::Scalar(0, 0, 255), 3);
        cv::line(show_cur_image, cv::Point2f(ref_pixel_uv[i].x(), ref_pixel_uv[i].y()), cv::Point2f(cur_pixel_uv[i].x(), cur_pixel_uv[i].y()), cv::Scalar(0, 255, 0), 2);
    }
    cv::imshow("KLT : Feature after multi tracking", show_cur_image);

    cv::waitKey(0);
#endif

    free(ref_pyramid.data());
    free(cur_pyramid.data());

    return cost_time;
}

float TestOpencvLk(int32_t pyramid_level, int32_t patch_size) {
    cv::Mat cv_ref_image, cv_cur_image;
    cv_ref_image = cv::imread(test_ref_image_file_name, 0);
    cv_cur_image = cv::imread(test_cur_image_file_name, 0);

    std::vector<cv::Point2f> ref_corners, cur_corners;
    cv::goodFeaturesToTrack(cv_ref_image, ref_corners, FEATURES_TO_TRACK, 0.01, 20);

    std::vector<uchar> status;
    std::vector<float> errors;

    clock_t begin, end;
    begin = clock();
    cv::calcOpticalFlowPyrLK(cv_ref_image, cv_cur_image, ref_corners, cur_corners, status, errors,
        cv::Size(2 * patch_size + 1, 2 * patch_size + 1), pyramid_level - 1);
    end = clock();
    const float cost_time = static_cast<float>(end - begin)/ CLOCKS_PER_SEC * 1000.0f;

#if CONFIG_OPENCV_DRAW
    cv::Mat show_ref_image(cv_ref_image.rows, cv_ref_image.cols, CV_8UC3);
    cv::cvtColor(cv_ref_image, show_ref_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < ref_corners.size(); i++) {
        cv::circle(show_ref_image, ref_corners[i], 2, cv::Scalar(255, 255, 0), 3);
    }
    cv::imshow("OpenCvLk : Feature before multi tracking", show_ref_image);

    cv::Mat show_cur_image(cv_cur_image.rows, cv_cur_image.cols, CV_8UC3);
    cv::cvtColor(cv_cur_image, show_cur_image, cv::COLOR_GRAY2BGR);
    for (unsigned long i = 0; i < cur_corners.size(); i++) {
        if (status[i] != 1) {
            continue;
        }
        cv::circle(show_cur_image, cur_corners[i], 2, cv::Scalar(0, 0, 255), 3);
        cv::line(show_cur_image, ref_corners[i], cur_corners[i], cv::Scalar(0, 255, 0), 2);
    }
    cv::imshow("OpenCvLk : Feature after multi tracking", show_cur_image);

    cv::waitKey(0);
#endif

    return cost_time;
}

int main(int argc, char **argv) {
    uint32_t test_times = 300;
    uint8_t optical_flow_method = 2;
    int32_t pyramid_level = 4;
    int32_t half_patch_size = 6;

#if CONFIG_OPENCV_DRAW
    test_times = 1;
#endif

    std::thread test_lk([] (int32_t pyramid_level, int32_t half_patch_size, uint8_t optical_flow_method, uint32_t test_time) {
        float cost_time = 0.0f;
        for (uint32_t i = 0; i < test_time; ++i) {
            cost_time += TestLkOpticalFlow(pyramid_level, half_patch_size, optical_flow_method);
        }
        std::cout << "lk.TrackMultipleLevel average cost time " << cost_time / static_cast<float>(test_time) << " ms." << std::endl;
    }, pyramid_level, half_patch_size, optical_flow_method, test_times);
    test_lk.join();

    std::thread test_klt([] (int32_t pyramid_level, int32_t half_patch_size, uint8_t optical_flow_method, uint32_t test_time) {
        float cost_time = 0.0f;
        for (uint32_t i = 0; i < test_time; ++i) {
            cost_time += TestKltOpticalFlow(pyramid_level, half_patch_size, optical_flow_method);
        }
        std::cout << "klt.TrackMultipleLevel average cost time " << cost_time / static_cast<float>(test_time) << " ms." << std::endl;
    }, pyramid_level, half_patch_size, optical_flow_method, test_times);
    test_klt.join();

    float cost_time = 0.0f;
    cv::setNumThreads(1);
    for (uint32_t i = 0; i < test_times; ++i) {
        cost_time += TestOpencvLk(pyramid_level, half_patch_size);
    }
    std::cout << "cv::calcOpticalFlowPyrLK average cost time " << cost_time / static_cast<float>(test_times) << " ms." << std::endl;

    return 0;
}

/*
    clock_t begin, end;
    begin = clock();
    end = clock();
    const float cost_time = static_cast<float>(end - begin)/ CLOCKS_PER_SEC * 1000.0f;
*/
