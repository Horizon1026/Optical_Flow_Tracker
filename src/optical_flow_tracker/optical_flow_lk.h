#ifndef _OPTICAL_FLOW_LK_H_
#define _OPTICAL_FLOW_LK_H_

#include "optical_flow.h"
#include <vector>

namespace FEATURE_TRACKER {

class OpticalFlowLk : public OpticalFlow {

public:
    OpticalFlowLk() : OpticalFlow() {}
    virtual ~OpticalFlowLk() = default;

    virtual bool TrackSingleLevel(const Image &ref_image,
                                  const Image &cur_image,
                                  const std::vector<Vec2> &ref_pixel_uv,
                                  std::vector<Vec2> &cur_pixel_uv,
                                  std::vector<uint8_t> &status) override;

    virtual bool PrepareForTracking() override;

private:
    void TrackOneFeatureFast(const Image &ref_image,
                             const Image &cur_image,
                             const Vec2 &ref_pixel_uv,
                             Vec2 &cur_pixel_uv,
                             uint8_t &status);

    void TrackOneFeature(const Image &ref_image,
                         const Image &cur_image,
                         const Vec2 &ref_pixel_uv,
                         Vec2 &cur_pixel_uv,
                         uint8_t &status);

    void ConstructIncrementalFunction(const Image &ref_image,
                                      const Image &cur_image,
                                      const Vec2 &ref_point,
                                      const Vec2 &cur_point,
                                      Mat2 &H,
                                      Vec2 &b,
                                      float &average_residual,
                                      int32_t &num_of_valid_pixel);

    inline void GetPixelValueFrameBuffer(const Image &image,
                                         const int32_t row_idx_buf,
                                         const int32_t col_idx_buf,
                                         const float row_image,
                                         const float col_image,
                                         float *value) {
        float temp = pixel_values_in_patch_(row_idx_buf, col_idx_buf);

        if (temp > 0) {
            *value = temp;
        } else {
            *value = image.GetPixelValueNoCheck(row_image, col_image);
            pixel_values_in_patch_(row_idx_buf, col_idx_buf) = *value;
        }
    }

    void PrecomputeHessian(const Image &ref_image,
                           const Vec2 &ref_point,
                           Mat2 &H);

    float ComputeResidual(const Image &cur_image,
                          const Vec2 &cur_point,
                          Vec2 &b);

private:
    std::vector<Vec3> fx_fy_ti_;
    Mat pixel_values_in_patch_;
};

}

#endif
