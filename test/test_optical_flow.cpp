#include "iostream"
#include "cstdint"
#include "string"
#include "vector"
#include "ctime"
#include "thread"

#include "log_report.h"
#include "slam_memory.h"
#include "visualizor.h"

#include "feature_point_detector.h"
#include "feature_harris.h"

#include "optical_flow_lk.h"
#include "optical_flow_klt.h"

#define DRAW_TRACKING_RESULT (1)
#define FEATURES_TO_TRACK (200)

std::string test_ref_image_file_name = "../example/optical_flow/ref_image.png";
std::string test_cur_image_file_name = "../example/optical_flow/cur_image.png";

void DrawReferenceImage(const GrayImage &image, const std::vector<Vec2> &pixel_uv, const std::string &title) {
    uint8_t *show_ref_image_buf = (uint8_t *)SlamMemory::Malloc(image.rows() * image.cols() * 3);
    RgbImage show_ref_image(show_ref_image_buf, image.rows(), image.cols(), true);
    Visualizor::ConvertUint8ToRgb(image.data(), show_ref_image.data(), image.rows() * image.cols());
    for (unsigned long i = 0; i < pixel_uv.size(); i++) {
        Visualizor::DrawSolidCircle(show_ref_image, pixel_uv[i].x(), pixel_uv[i].y(),
            3, RgbPixel{.r = 0, .g = 255, .b = 255});
    }
    Visualizor::ShowImage(title, show_ref_image);
}

void DrawCurrentImage(const GrayImage &image, const std::vector<Vec2> &ref_pixel_uv, const std::vector<Vec2> &cur_pixel_uv, const std::string &title, const std::vector<uint8_t> &status) {
    uint8_t *show_cur_image_buf = (uint8_t *)SlamMemory::Malloc(image.rows() * image.cols() * 3);
    RgbImage show_cur_image(show_cur_image_buf, image.rows(), image.cols(), true);
    Visualizor::ConvertUint8ToRgb(image.data(), show_cur_image.data(), image.rows() * image.cols());
    for (unsigned long i = 0; i < ref_pixel_uv.size(); i++) {
        if (status[i] != static_cast<uint8_t>(FEATURE_TRACKER::TrackStatus::kTracked)) {
            continue;
        }
        Visualizor::DrawSolidCircle(show_cur_image, ref_pixel_uv[i].x(), ref_pixel_uv[i].y(),
            3, RgbPixel{.r = 255, .g = 0, .b = 0});
        Visualizor::DrawBressenhanLine(show_cur_image, ref_pixel_uv[i].x(), ref_pixel_uv[i].y(),
            cur_pixel_uv[i].x(), cur_pixel_uv[i].y(),
            RgbPixel{.r = 0, .g = 255, .b = 0});
    }
    Visualizor::ShowImage(title, show_cur_image);
}

void DetectFeatures(const GrayImage &image, std::vector<Vec2> &pixel_uv) {
    // Detect features.
    FEATURE_DETECTOR::FeaturePointDetector<FEATURE_DETECTOR::HarrisFeature> detector;
    detector.options().kMinFeatureDistance = 20;
    detector.feature().options().kMinValidResponse = 40.0f;

    detector.DetectGoodFeatures(image, FEATURES_TO_TRACK, pixel_uv);
}

float TestLkOpticalFlow(int32_t pyramid_level, int32_t patch_size, uint8_t method) {
    // Load images.
    GrayImage ref_image;
    GrayImage cur_image;
    Visualizor::LoadImage(test_ref_image_file_name, ref_image);
    Visualizor::LoadImage(test_cur_image_file_name, cur_image);

    // Generate image pyramids.
    ImagePyramid ref_pyramid, cur_pyramid;
    ref_pyramid.SetPyramidBuff((uint8_t *)SlamMemory::Malloc(sizeof(uint8_t) * ref_image.rows() * ref_image.cols()), true);
    cur_pyramid.SetPyramidBuff((uint8_t *)SlamMemory::Malloc(sizeof(uint8_t) * cur_image.rows() * cur_image.cols()), true);
    cur_pyramid.SetRawImage(cur_image.data(), cur_image.rows(), cur_image.cols());
    ref_pyramid.SetRawImage(ref_image.data(), ref_image.rows(), ref_image.cols());

    // Detect features.
    std::vector<Vec2> ref_pixel_uv, cur_pixel_uv;
    std::vector<uint8_t> status;
    DetectFeatures(ref_image, ref_pixel_uv);

    // Use LK optical tracker.
    FEATURE_TRACKER::OpticalFlowLk lk;
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

#if DRAW_TRACKING_RESULT
    // DrawReferenceImage(ref_image, ref_pixel_uv, "LK : Feature before multi tracking");
    DrawCurrentImage(ref_image, ref_pixel_uv, cur_pixel_uv, "LK : Feature after multi tracking", status);
    Visualizor::WaitKey(0);
#endif

    return cost_time;
}

float TestKltOpticalFlow(int32_t pyramid_level, int32_t patch_size, uint8_t method) {
    // Load images.
    GrayImage ref_image;
    GrayImage cur_image;
    Visualizor::LoadImage(test_ref_image_file_name, ref_image);
    Visualizor::LoadImage(test_cur_image_file_name, cur_image);

    // Generate image pyramids.
    ImagePyramid ref_pyramid, cur_pyramid;
    ref_pyramid.SetPyramidBuff((uint8_t *)SlamMemory::Malloc(sizeof(uint8_t) * ref_image.rows() * ref_image.cols()), true);
    cur_pyramid.SetPyramidBuff((uint8_t *)SlamMemory::Malloc(sizeof(uint8_t) * cur_image.rows() * cur_image.cols()), true);
    cur_pyramid.SetRawImage(cur_image.data(), cur_image.rows(), cur_image.cols());
    ref_pyramid.SetRawImage(ref_image.data(), ref_image.rows(), ref_image.cols());

    // Detect features.
    std::vector<Vec2> ref_pixel_uv, cur_pixel_uv;
    std::vector<uint8_t> status;
    DetectFeatures(ref_image, ref_pixel_uv);

    FEATURE_TRACKER::OpticalFlowKlt klt;
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

#if DRAW_TRACKING_RESULT
    // DrawReferenceImage(ref_image, ref_pixel_uv, "KLT : Feature before multi tracking");
    DrawCurrentImage(ref_image, ref_pixel_uv, cur_pixel_uv, "KLT : Feature after multi tracking", status);
    Visualizor::WaitKey(0);
#endif

    return cost_time;
}

int main(int argc, char **argv) {
    uint32_t test_times = 300;
    uint8_t optical_flow_method = 2;
    int32_t pyramid_level = 4;
    int32_t half_patch_size = 10;

#if DRAW_TRACKING_RESULT
    test_times = 1;
#endif

    std::thread test_lk([] (int32_t pyramid_level, int32_t half_patch_size, uint8_t optical_flow_method, uint32_t test_time) {
        float cost_time = 0.0f;
        for (uint32_t i = 0; i < test_time; ++i) {
            cost_time += TestLkOpticalFlow(pyramid_level, half_patch_size, optical_flow_method);
        }
        ReportInfo("lk.TrackMultipleLevel average cost time " << cost_time / static_cast<float>(test_time) << " ms.");
    }, pyramid_level, half_patch_size, optical_flow_method, test_times);
    test_lk.join();

    std::thread test_klt([] (int32_t pyramid_level, int32_t half_patch_size, uint8_t optical_flow_method, uint32_t test_time) {
        float cost_time = 0.0f;
        for (uint32_t i = 0; i < test_time; ++i) {
            cost_time += TestKltOpticalFlow(pyramid_level, half_patch_size, optical_flow_method);
        }
        ReportInfo("klt.TrackMultipleLevel average cost time " << cost_time / static_cast<float>(test_time) << " ms.");
    }, pyramid_level, half_patch_size, optical_flow_method, test_times);
    test_klt.join();

    return 0;
}

/*
    clock_t begin, end;
    begin = clock();
    end = clock();
    const float cost_time = static_cast<float>(end - begin)/ CLOCKS_PER_SEC * 1000.0f;
*/
