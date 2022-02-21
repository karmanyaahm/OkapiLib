#include "okapi/api/control/iterative/iterativeSlewController.hpp"
#include "EZ-Template/api.hpp"
#include <cmath>
using namespace okapi;
double IterativeSlewController::step(double target)
{
    if (!on)
        return target;

    //printf("SLEWWWW %f %f\n", prev, target);
    if (std::signbit(target) != std::signbit(prev))
    {
        prev = ez::util::sgn(target) * 0.000000000001;
        return 0;
    }
    if (fabs(target) > fabs(prev))
    {

        double err = target - prev;
        double allowed_err_diff = std::clamp(err, -maxdiff, maxdiff);
        //printf("SLEW %f", allowed_err_diff);

        prev = prev + allowed_err_diff;
        return prev;
    }
    prev = target;
    return target;
}