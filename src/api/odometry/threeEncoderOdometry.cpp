/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "okapi/api/odometry/threeEncoderOdometry.hpp"
#include "extreme_basic.hpp"
#include "okapi/api/units/QSpeed.hpp"
#include <math.h>

namespace okapi {
ThreeEncoderOdometry::ThreeEncoderOdometry(const TimeUtil &itimeUtil,
                                           const std::shared_ptr<ReadOnlyChassisModel> &imodel,
                                           const ChassisScales &ichassisScales,
                                           const std::shared_ptr<Logger> &logger)
  : TwoEncoderOdometry(itimeUtil, imodel, ichassisScales, logger) {
  if (ichassisScales.middle == 0) {
    std::string msg = "ThreeEncoderOdometry: Middle scale cannot be zero.";
    LOG_ERROR(msg);
    throw std::invalid_argument(msg);
  }
}

OdomState ThreeEncoderOdometry::odomMathStep(const std::valarray<std::int32_t> &itickDiff,
                                             const QTime &) {
  if (itickDiff.size() < 3) {
    LOG_ERROR_S("ThreeEncoderOdometry: itickDiff did not have at least three elements.");
    return OdomState{};
  }

  for (auto &&elem : itickDiff) {
    if (std::abs(elem) > maximumTickDiff) {
      LOG_ERROR("ThreeEncoderOdometry: A tick diff (" + std::to_string(elem) +
                ") was greater than the maximum allowable diff (" +
                std::to_string(maximumTickDiff) + "). Skipping this odometry step.");
      return OdomState{};
    }
  }

  const double deltaL = itickDiff[0] / chassisScales.straight;
  const double deltaR = itickDiff[1] / chassisScales.straight;

  // typical dela theta
  // double deltaTheta = (deltaL - deltaR) / chassisScales.wheelTrack.convert(meter);
  //  my gyro based delta theta
  double deltaTheta = ((itickDiff[2] / BASIC_CONSTS::GYRO_TIMES) * 1_deg).convert(radian);

  double localOffX, localOffY;

  if (deltaTheta != 0) {
    localOffX = 2 * std::sin(deltaTheta / 2) * chassisScales.middleWheelDistance.convert(meter);
    localOffY = 2 * std::sin(deltaTheta / 2) *
                (deltaR / deltaTheta + chassisScales.wheelTrack.convert(meter) / 2);
  } else {
    localOffX = 0;
    localOffY = deltaR;
  }

  double avgA = state.theta.convert(radian) + (deltaTheta / 2);

  double polarR = std::sqrt((localOffX * localOffX) + (localOffY * localOffY));
  double polarA = std::atan2(localOffY, localOffX) - avgA;

  double dX = std::sin(polarA) * polarR;
  double dY = std::cos(polarA) * polarR;

  if (isnan(dX)) {
    dX = 0;
  }

  if (isnan(dY)) {
    dY = 0;
  }

  if (isnan(deltaTheta)) {
    deltaTheta = 0;
  }

  return OdomState{dX * meter, dY * meter, deltaTheta * radian};
}
} // namespace okapi
