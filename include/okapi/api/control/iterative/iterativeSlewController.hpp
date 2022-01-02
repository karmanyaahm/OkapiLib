#include "math.h"

#include <cstdio>

#pragma once
namespace okapi {
class IterativeSlewController {
  private:
  bool on = true;
  const double maxdiff;
  double prev;

  public:
  IterativeSlewController() : maxdiff(MAXFLOAT){};
  IterativeSlewController(double imaxdiff) : maxdiff(imaxdiff) {
  };

  // returns the value to input
  double step(double target);
  void reset() {
    prev = 0;
  };

  void disable() {
    on = false;
  };
  void enable() {
    on = true;
  };
};

} // namespace okapi