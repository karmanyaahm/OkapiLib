/**
 * @author Ryan Benasutti, WPI
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "okapi/device/button/controllerButton.hpp"

namespace okapi {
ControllerButton::ControllerButton(const controller_digital_e_t ibtn, const bool iinverted)
  : controller(E_CONTROLLER_MASTER), btn(ibtn), inverted(iinverted) {
}

ControllerButton::ControllerButton(const controller_id_e_t icontroller,
                                   const controller_digital_e_t ibtn, const bool iinverted)
  : controller(icontroller), btn(ibtn), inverted(iinverted) {
}

bool ControllerButton::isPressed() {
  wasPressedLast = currentlyPressed();
  return wasPressedLast;
}

bool ControllerButton::changed() {
  const bool pressed = currentlyPressed();
  const bool out = pressed ^ wasPressedLast;
  wasPressedLast = pressed;
  return out;
}

bool ControllerButton::changedToPressed() {
  return changed() && wasPressedLast;
}

bool ControllerButton::changedToReleased() {
  return changed() && !wasPressedLast;
}

bool ControllerButton::currentlyPressed() {
  const bool pressed = controller.get_digital(btn) != 0;
  return inverted ? !pressed : pressed;
}
} // namespace okapi
