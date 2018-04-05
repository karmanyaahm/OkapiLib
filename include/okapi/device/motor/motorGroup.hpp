/**
 * @author Ryan Benasutti, WPI
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _OKAPI_MOTORGROUP_HPP_
#define _OKAPI_MOTORGROUP_HPP_

#include "okapi/device/motor/abstractMotor.hpp"
#include "okapi/device/motor/motor.hpp"
#include <array>
#include <initializer_list>

namespace okapi {
template <size_t motorNum> class MotorGroup : public AbstractMotor {
  public:
  MotorGroup(const std::array<okapi::Motor, motorNum> &imotors)
    : AbstractMotor(imotors[0]), motors(imotors) {
  }

  // int32_t move_absolute(const float iposition, const int32_t ivelocity) const override {
  //   for (size_t i = 0; i < motorNum; i++) {
  //     motors[i].move_absolute(iposition, ivelocity);
  //   }
  // }

  // int32_t move_relative(const float iposition, const int32_t ivelocity) const override {
  //   for (size_t i = 0; i < motorNum; i++) {
  //     motors[i].move_relative(iposition, ivelocity);
  //   }
  // }

  // int32_t move_velocity(const int16_t ivelocity) const override {
  //   for (size_t i = 0; i < motorNum; i++) {
  //     motors[i].move_velocity(ivelocity);
  //   }
  // }

  // int32_t move_voltage(const int16_t ivoltage) const override {
  //   for (size_t i = 0; i < motorNum; i++) {
  //     motors[i].move_voltage(ivoltage);
  //   }
  // }

  /**
   * Writes the value of the controller output. This method might be automatically called in another
   * thread by the controller.
   *
   * @param ivalue the controller's output
   */
  virtual void controllerSet(const double ivalue) override {
    for (size_t i = 0; i < motorNum; i++) {
      motors[i].move_velocity(ivalue);
    }
  }

  /**
   * Get the encoder associated with this motor.
   *
   * @return encoder for this motor
   */
  virtual IntegratedEncoder getEncoder() const override {
    return motors[0].getEncoder();
  }

  protected:
  const std::array<okapi::Motor, motorNum> motors;
};
} // namespace okapi

#endif
