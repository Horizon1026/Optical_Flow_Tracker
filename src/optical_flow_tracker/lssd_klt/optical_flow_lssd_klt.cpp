#include "optical_flow_lssd_klt.h"
#include "slam_operations.h"
#include "log_report.h"

namespace FEATURE_TRACKER {

bool OpticalFlowLssdKlt::TrackSingleLevel(const GrayImage &ref_image,
                                          const GrayImage &cur_image,
                                          const std::vector<Vec2> &ref_pixel_uv,
                                          std::vector<Vec2> &cur_pixel_uv,
                                          std::vector<uint8_t> &status) {
    // Track per feature.
    const uint32_t max_feature_id = ref_pixel_uv.size() < options().kMaxTrackPointsNumber ?
                                    ref_pixel_uv.size() : options().kMaxTrackPointsNumber;
    for (uint32_t feature_id = 0; feature_id < max_feature_id; ++feature_id) {
        // Do not repeatly track features that has been tracking failed.
        CONTINUE_IF(status[feature_id] > static_cast<uint8_t>(TrackStatus::kTracked));

        switch (options().kMethod) {
            case OpticalFlowMethod::kInverse:
            case OpticalFlowMethod::kDirect:
            case OpticalFlowMethod::kFast:
            default:
                TrackOneFeature(ref_image, cur_image, ref_pixel_uv[feature_id], cur_pixel_uv[feature_id], status[feature_id]);
                break;
        }

        if (status[feature_id] == static_cast<uint8_t>(TrackStatus::kNotTracked)) {
            status[feature_id] = static_cast<uint8_t>(TrackStatus::kLargeResidual);
        }
    }

    return true;
}

void OpticalFlowLssdKlt::TrackOneFeature(const GrayImage &ref_image,
                                         const GrayImage &cur_image,
                                         const Vec2 &ref_pixel_uv,
                                         Vec2 &cur_pixel_uv,
                                         uint8_t &status) {
    float rotation = predict_theta_;
    Vec2 translation = Vec2::Zero();

    for (uint32_t iter = 0; iter < options().kMaxIteration; ++iter) {
        // Compute each pixel in the patch, create hessian * v = bias
        Mat3 hessian = Mat3::Zero();
        Vec3 bias = Vec3::Zero();
        BREAK_IF(ConstructIncrementalFunction(ref_image, cur_image, ref_pixel_uv, rotation, translation, hessian, bias) == 0);

        // Solve hessian * v = bias.
        Vec3 v = hessian.ldlt().solve(bias);
        if (Eigen::isnan(v.array()).any()) {
            status = static_cast<uint8_t>(TrackStatus::kNumericError);
            break;
        }

        // Update rotation and translation.
        rotation += v(0);
        translation += v.tail<2>();

        // Compute cur_pixel_uv.
        const float sin_theta = std::sin(rotation);
        const float cos_theta = std::cos(rotation);
        Mat2 rotation_matrix;
        rotation_matrix << cos_theta, sin_theta, -sin_theta, cos_theta;
        cur_pixel_uv = rotation_matrix * ref_pixel_uv + translation;

        // Check converge status.
        if (cur_pixel_uv.x() < 0 || cur_pixel_uv.x() > cur_image.cols() - 1 ||
            cur_pixel_uv.y() < 0 || cur_pixel_uv.y() > cur_image.rows() - 1) {
            status = static_cast<uint8_t>(TrackStatus::kOutside);
            break;
        }
        if (v.squaredNorm() < options().kMaxConvergeStep) {
            status = static_cast<uint8_t>(TrackStatus::kTracked);
            break;
        }
    }
}

int32_t OpticalFlowLssdKlt::ConstructIncrementalFunction(const GrayImage &ref_image,
                                                         const GrayImage &cur_image,
                                                         const Vec2 &ref_pixel_uv,
                                                         const float &rotation,
                                                         const Vec2 &translation,
                                                         Mat3 &hessian,
                                                         Vec3 &bias) {
    std::array<float, 6> temp_value = {};
    int32_t num_of_valid_pixel = 0;

    // Compute current pixel position.
    const float sin_theta = std::sin(rotation);
    const float cos_theta = std::cos(rotation);
    Mat2 rotation_matrix;
    rotation_matrix << cos_theta, sin_theta, -sin_theta, cos_theta;

    // Compute average pixel value in reference patch and current patch.
    float ref_average_value = 0.0f;
    float cur_average_value = 0.0f;
    // Check pixel valid in reference and current patch.
    MatInt patch_pixel_valid;
    patch_pixel_valid.setConstant(options().kPatchRowHalfSize * 2 + 1, options().kPatchColHalfSize * 2 + 1, 1);

    // Precompute average value in reference and current patch.
    if (options().kMethod == OpticalFlowMethod::kInverse) {
        // For inverse optical flow, use reference image to compute gradient.
        for (int32_t drow = - options().kPatchRowHalfSize; drow <= options().kPatchRowHalfSize; ++drow) {
            for (int32_t dcol = - options().kPatchColHalfSize; dcol <= options().kPatchColHalfSize; ++dcol) {
                const float row_i = static_cast<float>(drow) + ref_pixel_uv.y();
                const float col_i = static_cast<float>(dcol) + ref_pixel_uv.x();
                const Vec2 cur_patch_pixel_uv = rotation_matrix * Vec2(row_i, col_i) + translation;
                const float row_j = cur_patch_pixel_uv.y();
                const float col_j = cur_patch_pixel_uv.x();

                if (ref_image.GetPixelValue(row_i, col_i - 1.0f, &temp_value[0]) &&
                    ref_image.GetPixelValue(row_i, col_i + 1.0f, &temp_value[1]) &&
                    ref_image.GetPixelValue(row_i - 1.0f, col_i, &temp_value[2]) &&
                    ref_image.GetPixelValue(row_i + 1.0f, col_i, &temp_value[3]) &&
                    ref_image.GetPixelValue(row_i, col_i, &temp_value[4]) &&
                    cur_image.GetPixelValue(row_j, col_j, &temp_value[5])) {
                    ref_average_value += temp_value[4];
                    cur_average_value += temp_value[5];
                    ++num_of_valid_pixel;
                } else {
                    patch_pixel_valid(drow + options().kPatchRowHalfSize, dcol + options().kPatchColHalfSize) = 0;
                }
            }
        }
    } else {
        // For direct optical flow, use current image to compute gradient.
        for (int32_t drow = - options().kPatchRowHalfSize; drow <= options().kPatchRowHalfSize; ++drow) {
            for (int32_t dcol = - options().kPatchColHalfSize; dcol <= options().kPatchColHalfSize; ++dcol) {
                const float row_i = static_cast<float>(drow) + ref_pixel_uv.y();
                const float col_i = static_cast<float>(dcol) + ref_pixel_uv.x();
                const Vec2 cur_patch_pixel_uv = rotation_matrix * Vec2(row_i, col_i) + translation;
                const float row_j = cur_patch_pixel_uv.y();
                const float col_j = cur_patch_pixel_uv.x();

                if (cur_image.GetPixelValue(row_j, col_j - 1.0f, &temp_value[0]) &&
                    cur_image.GetPixelValue(row_j, col_j + 1.0f, &temp_value[1]) &&
                    cur_image.GetPixelValue(row_j - 1.0f, col_j, &temp_value[2]) &&
                    cur_image.GetPixelValue(row_j + 1.0f, col_j, &temp_value[3]) &&
                    ref_image.GetPixelValue(row_i, col_i, &temp_value[4]) &&
                    cur_image.GetPixelValue(row_j, col_j, &temp_value[5])) {
                    ref_average_value += temp_value[4];
                    cur_average_value += temp_value[5];
                    ++num_of_valid_pixel;
                } else {
                    patch_pixel_valid(drow + options().kPatchRowHalfSize, dcol + options().kPatchColHalfSize) = 0;
                }
            }
        }
    }
    ref_average_value /= static_cast<float>(num_of_valid_pixel);
    cur_average_value /= static_cast<float>(num_of_valid_pixel);

    // TODO:
    // Compute jacobian of se2.
    Mat2x3 jacobian_se2 = Mat2x3::Zero();
    jacobian_se2.block<2, 2>(0, 1).setIdentity();

    // Compute jacobian and residual.
    if (options().kMethod == OpticalFlowMethod::kInverse) {
        // For inverse optical flow, use reference image to compute gradient.
        for (int32_t drow = - options().kPatchRowHalfSize; drow <= options().kPatchRowHalfSize; ++drow) {
            for (int32_t dcol = - options().kPatchColHalfSize; dcol <= options().kPatchColHalfSize; ++dcol) {
                if (patch_pixel_valid(drow + options().kPatchRowHalfSize, dcol + options().kPatchColHalfSize)) {
                    const float row_i = static_cast<float>(drow) + ref_pixel_uv.y();
                    const float col_i = static_cast<float>(dcol) + ref_pixel_uv.x();
                    const Vec2 cur_patch_pixel_uv = rotation_matrix * Vec2(row_i, col_i) + translation;
                    const float row_j = cur_patch_pixel_uv.y();
                    const float col_j = cur_patch_pixel_uv.x();

                    temp_value[0] = ref_image.GetPixelValueNoCheck(row_i, col_i - 1.0f);
                    temp_value[1] = ref_image.GetPixelValueNoCheck(row_i, col_i + 1.0f);
                    temp_value[2] = ref_image.GetPixelValueNoCheck(row_i - 1.0f, col_i);
                    temp_value[3] = ref_image.GetPixelValueNoCheck(row_i + 1.0f, col_i);
                    temp_value[4] = ref_image.GetPixelValueNoCheck(row_i, col_i);
                    temp_value[5] = cur_image.GetPixelValueNoCheck(row_j, col_j);

                    Mat1x2 jacobian_pixel = Mat1x2(temp_value[1] - temp_value[0], temp_value[3] - temp_value[2]);
                    Mat1x3 jacobian = jacobian_pixel * jacobian_se2;
                    Vec1 residual = Vec1(temp_value[5] / cur_average_value - temp_value[4] / ref_average_value);

                    hessian += jacobian.transpose() * jacobian;
                    bias -= jacobian.transpose() * residual;
                }
            }
        }
    } else {
        // For direct optical flow, use current image to compute gradient.
        for (int32_t drow = - options().kPatchRowHalfSize; drow <= options().kPatchRowHalfSize; ++drow) {
            for (int32_t dcol = - options().kPatchColHalfSize; dcol <= options().kPatchColHalfSize; ++dcol) {
                if (patch_pixel_valid(drow + options().kPatchRowHalfSize, dcol + options().kPatchColHalfSize)) {
                    const float row_i = static_cast<float>(drow) + ref_pixel_uv.y();
                    const float col_i = static_cast<float>(dcol) + ref_pixel_uv.x();
                    const Vec2 cur_patch_pixel_uv = rotation_matrix * Vec2(row_i, col_i) + translation;
                    const float row_j = cur_patch_pixel_uv.y();
                    const float col_j = cur_patch_pixel_uv.x();

                    temp_value[0] = cur_image.GetPixelValueNoCheck(row_j, col_j - 1.0f);
                    temp_value[1] = cur_image.GetPixelValueNoCheck(row_j, col_j + 1.0f);
                    temp_value[2] = cur_image.GetPixelValueNoCheck(row_j - 1.0f, col_j);
                    temp_value[3] = cur_image.GetPixelValueNoCheck(row_j + 1.0f, col_j);
                    temp_value[4] = ref_image.GetPixelValueNoCheck(row_i, col_i);
                    temp_value[5] = cur_image.GetPixelValueNoCheck(row_j, col_j);

                    Mat1x2 jacobian_pixel = Mat1x2(temp_value[1] - temp_value[0], temp_value[3] - temp_value[2]);
                    Mat1x3 jacobian = jacobian_pixel * jacobian_se2;
                    Vec1 residual = Vec1(temp_value[5] / cur_average_value - temp_value[4] / ref_average_value);

                    hessian += jacobian.transpose() * jacobian;
                    bias -= jacobian.transpose() * residual;
                }
            }
        }
    }

    return num_of_valid_pixel;
}

}
