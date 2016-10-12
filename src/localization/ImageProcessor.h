//
// Created by olivier on 10/2/16.
//

#ifndef LOCALIZATION_IMAGE_PROCESSOR_H
#define LOCALIZATION_IMAGE_PROCESSOR_H

#include <opencv2/core/core.hpp>
#include "Line.h"

namespace localization {

class LineGroup;

class ImageProcessor
{
public:

    static ImageProcessor* getInstance();
    static void freeInstance();

    void processImage(cv::Mat input);

private:
    static ImageProcessor* instance_;
    cv::Mat image_;

    cv::Mat distortionMap1_;
    cv::Mat distortionMap2_;

    std::vector<Line> lineCluster_;
    std::vector<Line> detectedLines_;

    cv::Mat vLines_, hLines_, lineGroups_, mLines_, intersectionGroup_;

    void preProcess(const cv::Mat& raw, cv::Mat& preProcessed);
    void findEdges(const cv::Mat& src, cv::Mat& edges);
    void findLines(const cv::Mat& edges, cv::Mat& line);

    void analyzeLineCluster();
    void buildLineArray(const std::vector<cv::Vec2f>& lineCluster);

    void groupByOrientation(std::vector<LineGroup>& orientationGroup, const std::vector<Line>& lines);
    void groupByOrientation(std::vector<LineGroup>& group, Line& line);

    void groupByIntersection(std::vector<LineGroup>& intersectingGroup, const std::vector<LineGroup>& orientationGroup);
    void groupByIntersection(std::vector<LineGroup>& intersectingGroup, Line& line);

    void groupByDistance(std::vector<LineGroup>& distanceGroup, Line& line);
    void groupByDistance(std::vector<LineGroup>& distanceGroup, const std::vector<LineGroup>& intersectionGroup);

    bool isInsideRect(const cv::Point2f& point, const cv::Rect& rect);

    void drawRawLines(cv::Mat& dst, const std::vector<cv::Vec2f> &raw_lines) const;
    void drawLines(cv::Mat& dst, const std::vector<Line>& lines) const;
    void drawLine(cv::Mat& dst, const Line& line, const cv::Scalar& color) const;
    void drawLineGroup(cv::Mat& dst, const LineGroup& group, const cv::Scalar& color);

    ImageProcessor();
    ~ImageProcessor() = default;
};

}


#endif // LOCALIZATION_IMAGE_PROCESSOR_H
