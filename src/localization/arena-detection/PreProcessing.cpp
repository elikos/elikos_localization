
#include "PreProcessing.h"

#include <Eigen/Geometry>
#include <Eigen/Core>

#include <iostream>

#include <cmath>

namespace localization 
{

PreProcessing::PreProcessing()
{
    cv::Mat distortedCamera = (cv::Mat_<float>(3,3) << 422.918640,    0.000000,    350.119451,
            0.000000,  423.121112,    236.380265,
            0.000000,    0.000000,      1.000000);

    cv::Mat cameraDistortion = (cv::Mat_<float>(1,5) << -0.321590, 0.089597, 0.001090, -0.000489, 0.000000);

    cv::Mat undistortedCamera = cv::getOptimalNewCameraMatrix(distortedCamera, cameraDistortion, cv::Size(640, 480), 0);

    cv::initUndistortRectifyMap(distortedCamera, cameraDistortion, cv::Mat(), undistortedCamera,
                                cv::Size(640, 480), CV_32FC1, distortionMap1_, distortionMap2_);

    cv::namedWindow("PreProcessed", 1);
    cv::createTrackbar("white threshold", "PreProcessed", &whiteThreshold_, 255);
    cv::createTrackbar("undistort type", "PreProcessed", &undistortType_, 1);
}

void PreProcessing::preProcessImage(const cv::Mat& raw, const ros::Time& stamp, cv::Mat& preProcessed)
{

    /*
    tf::StampedTransform tf;
    try {
        tfListener_.lookupTransform("elikos_local_origin", "elikos_fcu", ros::Time(0), tf);

        tf::Matrix3x3 m(tf.getRotation());
        double yaw;
        m.getRPY(roll_, pitch_, yaw);
        std::cout << pitch_ << " : " << roll_ << std::endl;

    } catch (tf::TransformException e) {
         ROS_ERROR("%s", e.what());
    }
    */

    //undistort
    // Blur
    cv::Mat typeConverted;
    if (raw.type() != CV_8UC1) {
        cv::cvtColor(raw, typeConverted, CV_BGR2GRAY);
    } else {
        raw.copyTo(typeConverted);
    }

    cv::Mat undistorted = typeConverted;
    if (!undistortType_) {
        cv::remap(typeConverted, undistorted, distortionMap1_, distortionMap2_, CV_INTER_LINEAR);
    }

    cv::Mat perspective;
    removePerspective(undistorted, perspective);
    cv::imshow("undistorted", undistorted);
    cv::imshow("perspective", perspective);

    cv::Mat blured;
    cv::GaussianBlur(perspective, blured, cv::Size(7,7), 8, 8);


    cv::Mat eroded;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::erode(blured, eroded, element, cv::Point(0), 8);

    cv::Mat thresholded;
    cv::threshold(blured, thresholded, whiteThreshold_, 255, CV_THRESH_BINARY);

    preProcessed = thresholded;

    cv::imshow("PreProcessed", preProcessed);
}

void PreProcessing::removePerspective(const cv::Mat& input, cv::Mat& rectified) const
{
    double roll, pitch, yaw = 0.0;
    Eigen::Vector3f direction;
    try {
        tf::StampedTransform tf;
        tfListener_.lookupTransform("elikos_local_origin", "elikos_fcu", ros::Time(0), tf);

        tf::Matrix3x3 m(tf.getRotation());
        m.getRPY(roll, pitch, yaw);

        tf::Vector3 v = m * tf::Vector3(0.0, 0.0, 1.0);
        direction.x() = v.x();
        direction.y() = v.y();
        direction.z() = v.z();

    } catch (tf::TransformException e) {
         ROS_ERROR("%s", e.what());
    }

    Eigen::Matrix3f r = (Eigen::AngleAxisf(-pitch, Eigen::Vector3f::UnitX()) * 
                         Eigen::AngleAxisf(-roll,  Eigen::Vector3f::UnitY())).toRotationMatrix();
                            
    Eigen::Matrix4f R = Eigen::Matrix4f::Zero();
    R(3, 3) = 1;
    for (int i = 0; i < 3; ++i) 
    {
        for (int j = 0; j < 3; j++) 
        {
            R(i, j) = r(i, j);
        }
    }

    double height = input.size().height;
    double width = input.size().width;
    double f = 423.0;
    double HFOV = std::atan( width / (2 * f));
    double VFOV = std::atan( height / (2 * f));

    Eigen::Vector4f src[4] { Eigen::Vector4f( 1.0,  1.0, 0.0, 1.0), 
                             Eigen::Vector4f(-1.0,  1.0, 0.0, 1.0), 
                             Eigen::Vector4f(-1.0, -1.0, 0.0, 1.0), 
                             Eigen::Vector4f( 1.0, -1.0, 0.0, 1.0) };

    Eigen::Vector4f dst[4] { Eigen::Vector4f( 1.0,  1.0, 0.0, 1.0), 
                             Eigen::Vector4f(-1.0,  1.0, 0.0, 1.0), 
                             Eigen::Vector4f(-1.0, -1.0, 0.0, 1.0), 
                             Eigen::Vector4f( 1.0, -1.0, 0.0, 1.0) };
   
    Eigen::Matrix4f P = getPerspectiveProjectionTransform(f, width, height); 
    Eigen::Translation<float, 4> T(Eigen::Vector4f(0.0, 0.0, -1.0, 0.0));

    for (int i = 0; i < 4; ++i) 
    {
        dst[i] = T * dst[i];
        dst[i] = P * dst[i];
        dst[i] /= dst[i][3];

        src[i] = R * src[i];
        src[i] = T * src[i];
        src[i] = P * src[i];
        //src[i] = P * T * R * dst[i];
        src[i] /= src[i][3];
    }

    cv::Point2f tSrc[4], tDst[4];
    for (int i = 0; i < 4; ++i) 
    {
        tSrc[i] = cv::Point2f(src[i].x() * width / 2.0 + width / 2.0, src[i].y() * height / 2.0 + height / 2.0);
        tDst[i] = cv::Point2f(dst[i].x() * width / 2.0 + width / 2.0, dst[i].y() * height / 2.0 + height / 2.0);
    }

    cv::Mat perspectiveTransform = cv::getPerspectiveTransform(tSrc, tDst);
    cv::warpPerspective(input, rectified, perspectiveTransform, input.size());

    //rectified = input.clone();
    for (int i = 0; i < 4; ++i) 
    {
        cv::circle(rectified, tSrc[i], 5, cv::Scalar(0, 200 ,0), -1);
        cv::circle(rectified, tDst[i], 5, cv::Scalar(0, 100 ,0), -1);
    }

    //float y = src[1];
    //float z = src[2];

    //src[1] = y * std::sin(pitch) + z * std::cos(pitch);
    //src[2] = y * std::cos(pitch) - z * std::

    /*
    Eigen::Vector2f src[4] {{0.0, 0.0}, {0.0, height}, { width, height }, { width, 0.0}};
    Eigen::Vector2f dst[4] {{0.0, 0.0}, {0.0, height}, { width, height }, { width, 0.0}};
    
    
    Eigen::Vector2f leftRotationPoint = src[0] + (src[1] - src[0]) / 2.0;
    Eigen::Vector2f rightRotationPoint = src[3] + (src[2] - src[3]) / 2.0;;
    Eigen::Vector2f center = (src[2] - src[0]) / 2.0;

    float theta = std::atan2(direction.y(), -direction.x());
    if (std::isnan(theta)) {
        theta = 0.0;
    }

    float phi = std::atan2(std::sqrt(direction.x() * direction.x() + direction.y() * direction.y()), 1);

    Eigen::Rotation2D<float> R(phi);
    Eigen::Rotation2D<float> cr(theta);

    for (int i = 0; i < 4; ++i) 
    {
        dst[i] = cr * (dst[i] - center) + center;
    }
    
    leftRotationPoint = cr * (leftRotationPoint - center) + center;
    rightRotationPoint = cr * (rightRotationPoint - center) + center;

    src[0] = R.inverse() * (dst[0] - leftRotationPoint) + leftRotationPoint;
    src[1] = R.inverse() * (dst[1] - leftRotationPoint) + leftRotationPoint;
    src[2] = R * (dst[2] - rightRotationPoint) + rightRotationPoint;
    src[3] = R * (dst[3] - rightRotationPoint) + rightRotationPoint;

    */

}

Eigen::Matrix4f PreProcessing::getPerspectiveProjectionTransform(double focalLength, double width, double height) const
{
    Eigen::Matrix4f m = Eigen::Matrix4f::Zero();
    m(0, 0) = 2 * focalLength / width;
    m(1, 1) = 2 * focalLength / height;
    m(3, 2) = -1;

    return m;
}

void PreProcessing::showCalibTrackBars()
{

}

Eigen::Vector2f PreProcessing::translate(const Eigen::Vector2f& v, const Eigen::Vector2f& translation) const
{
    return v + translation;
}

Eigen::Vector2f PreProcessing::rotate(const Eigen::Vector2f& v, double theta) const
{
    double cosTheta = cosf(theta);
    double sinTheta = sinf(theta);

    Eigen::Vector2f rotated;
    rotated.x() = v.x() * cosTheta - v.y() * sinTheta;
    rotated.y() = v.x() * sinTheta + v.y() * cosTheta;

    return rotated;
}

}