//
// Created by olivier on 10/5/16.
//

#include "LineGroup.h"

#include "Line.h"

namespace localization
{

LineGroup::LineGroup(Line& line)
    : avgOrientation_(line.getOrientation()),
      avgRho_(line.getRho())
{
    lines_.push_back(&line);
}

void LineGroup::add(Line& line)
{
    float product = line.getOrientation().dot(avgOrientation_);
    if (product < 0)
    {
        line.inverseOrientation();
    }

    avgOrientation_ *= (double)(lines_.size());
    avgRho_ *= (double)(lines_.size());

    lines_.push_back(&line);

    avgOrientation_ += line.getOrientation();
    avgRho_ += line.getRho();

    avgOrientation_ /= (double)(lines_.size());
    avgRho_ /= (double)(lines_.size());
}

Line LineGroup::convertToLine() const
{
    return Line(avgRho_, avgOrientation_);
}

}
