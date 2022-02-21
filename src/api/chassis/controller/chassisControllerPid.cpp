/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "okapi/api/chassis/controller/chassisControllerPid.hpp"
#include "okapi/api/util/mathUtil.hpp"
#include "okapi/api/odometry/odomMath.hpp"
#include <cmath>
#include <utility>
#include "extreme_basic.hpp"

namespace okapi
{
  ChassisControllerPID::ChassisControllerPID(
      TimeUtil itimeUtil,
      std::shared_ptr<ChassisModel> ichassisModel,
      std::unique_ptr<IterativePosPIDController> idistanceController,
      std::unique_ptr<IterativePosPIDController> iturnController,
      std::unique_ptr<IterativePosPIDController> iangleController,
      const AbstractMotor::GearsetRatioPair &igearset,
      const ChassisScales &iscales,
      std::shared_ptr<Logger> ilogger,
      double slewdiff)
      : logger(std::move(ilogger)),
        chassisModel(std::move(ichassisModel)),
        timeUtil(std::move(itimeUtil)),
        distancePid(std::move(idistanceController)),
        turnPid(std::move(iturnController)),
        anglePid(std::move(iangleController)),
        scales(iscales),
        gearsetRatioPair(igearset)
  {
    if (igearset.ratio == 0)
    {
      std::string msg("ChassisControllerPID: The gear ratio cannot be zero! Check if you are using "
                      "integer division.");
      LOG_ERROR(msg);
      throw std::invalid_argument(msg);
    }
    chassisModel->setGearing(igearset.internalGearset);
    chassisModel->setEncoderUnits(AbstractMotor::encoderUnits::counts);

    slewL = std::make_unique<IterativeSlewController>(slewdiff);
    slewR = std::make_unique<IterativeSlewController>(slewdiff);
  }

  ChassisControllerPID::~ChassisControllerPID()
  {
    dtorCalled.store(true, std::memory_order_release);
    delete task;
  }

  void ChassisControllerPID::loop()
  {
    LOG_INFO_S("Started ChassisControllerPID task.");

    auto encStartVals = chassisModel->getSensorVals();
    std::valarray<std::int32_t> encVals;
    double distanceElapsed = 0, angleChange = 0;
    modeType pastMode = none;
    auto rate = timeUtil.getRate();

    while (!dtorCalled.load(std::memory_order_acquire) && !task->notifyTake(0))
    {
      /**
     * doneLooping is set to false by moveDistanceAsync and turnAngleAsync and then set to true by
     * waitUntilSettled
     */
      if (doneLooping.load(std::memory_order_acquire))
      {
        doneLoopingSeen.store(true, std::memory_order_release);
      }
      else
      {
        if (mode != pastMode || newMovement.load(std::memory_order_acquire))
        {
          encStartVals = chassisModel->getSensorVals();
          //printf(" START ============== %f\n", static_cast<double>(encStartVals[2]));
          newMovement.store(false, std::memory_order_release);
          slewL->reset();
          slewR->reset();
        }

        switch (mode)
        {
        case distance:
          encVals = chassisModel->getSensorVals() - encStartVals;
          distanceElapsed = static_cast<double>((encVals[0] + encVals[1])) / 2.0;
          angleChange = static_cast<double>(encVals[2]);

          distancePid->step(distanceElapsed);
          anglePid->step(angleChange);

          if (velocityMode)
          {
            chassisModel->driveVector(slewL->step(distancePid->getOutput()), anglePid->getOutput());
          }
          else
          {
            chassisModel->driveVectorVoltage(distancePid->getOutput(), anglePid->getOutput());
          }

          break;

        case angle:
          encVals = chassisModel->getSensorVals() - encStartVals;
          angleChange = static_cast<double>(encVals[2]) ;
          //printf("%f\n", angleChange);

          turnPid->step(angleChange);
          switch (swingg)
          {
          case swing::none:
            if (velocityMode)
            {
              chassisModel->driveVector(0, slewR->step(turnPid->getOutput()));
            }
            else
            {
              chassisModel->driveVectorVoltage(0, turnPid->getOutput());
            }

            break;
          case swing::left:
            if (velocityMode)
            {
              chassisModel->left(2*slewR->step(turnPid->getOutput()));
            }
            else
            {
              printf("AAAAAAAVELOCITYMODE\n");
            }

            break;
          case swing::right:
            if (velocityMode)
            {
              chassisModel->right(-2*slewR->step(turnPid->getOutput()));
            }
            else
            {
              printf("AAAAAAAVELOCITYMODE\n");
            }

            break;
          };
          break;

        default:
          break;
        }

        pastMode = mode;
      }

      rate->delayUntil(threadSleepTime);
    }

    stop();

    LOG_INFO_S("Stopped ChassisControllerPID task.");
  }

  void ChassisControllerPID::trampoline(void *context)
  {
    if (context)
    {
      static_cast<ChassisControllerPID *>(context)->loop();
    }
  }

  void ChassisControllerPID::moveDistanceAsync(const QLength itarget)
  {
    LOG_INFO("ChassisControllerPID: moving " + std::to_string(itarget.convert(meter)) + " meters");
    LOG_DEBUG("ChassisControllerPID: straight " + std::to_string(scales.straight) + " ratio " +
              std::to_string(gearsetRatioPair.ratio));

    distancePid->reset();
    anglePid->reset();
    distancePid->flipDisable(false);
    anglePid->flipDisable(false);
    turnPid->flipDisable(true);
    mode = distance;

    const double newTarget = itarget.convert(meter) * scales.straight * gearsetRatioPair.ratio;

    LOG_INFO("ChassisControllerPID: moving " + std::to_string(newTarget) + " motor ticks");

    distancePid->setTarget(newTarget);
    anglePid->setTarget(0);

    doneLooping.store(false, std::memory_order_release);
    newMovement.store(true, std::memory_order_release);
  }

  void ChassisControllerPID::moveRawAsync(const double itarget)
  {
    // Divide by straightScale so the final result turns back into motor ticks
    moveDistanceAsync((itarget / scales.straight) * meter);
  }

  void ChassisControllerPID::moveDistance(const QLength itarget)
  {
    moveDistanceAsync(itarget);
    waitUntilSettled();
  }

  void ChassisControllerPID::moveRaw(const double itarget)
  {
    // Divide by straightScale so the final result turns back into motor ticks
    moveDistance((itarget / scales.straight) * meter);
  }
  void ChassisControllerPID::turnAngleAsync(const QAngle idegTarget)
  {
    LOG_INFO("ChassisControllerPID: turning " + std::to_string(idegTarget.convert(degree)) +
             " degrees");
    LOG_DEBUG("ChassisControllerPID: scales.turn " + std::to_string(scales.turn) + " ratio " +
              std::to_string(gearsetRatioPair.ratio));

    turnPid->reset();
    turnPid->flipDisable(false);
    distancePid->flipDisable(true);
    anglePid->flipDisable(true);
    mode = angle;

    //const double newTarget =      idegTarget.convert(degree) * scales.turn * gearsetRatioPair.ratio * boolToSign(normalTurns);
    const double newTarget = idegTarget.convert(degree) * boolToSign(normalTurns) * BASIC_CONSTS::GYRO_TIMES;

    LOG_INFO("ChassisControllerPID: turning " + std::to_string(newTarget) + " motor ticks");

    turnPid->setTarget(newTarget);

    doneLooping.store(false, std::memory_order_release);
    newMovement.store(true, std::memory_order_release);
  }

  void ChassisControllerPID::turnRawAsync(const double idegTarget)
  {
    // Divide by turnScale so the final result turns back into motor ticks
    turnAngleAsync((idegTarget / scales.turn) * degree);
  }

  void ChassisControllerPID::turnAngle(const QAngle idegTarget)
  {
    turnAngleAsync(idegTarget);
    waitUntilSettled();
  }

  void ChassisControllerPID::setSwing(swing s)
  {
    swingg = s;
  }

  void ChassisControllerPID::turnRaw(const double idegTarget)
  {
    // Divide by turnScale so the final result turns back into motor ticks
    turnAngle((idegTarget / scales.turn) * degree);
  }

  void ChassisControllerPID::setTurnsMirrored(const bool ishouldMirror)
  {
    normalTurns = !ishouldMirror;
  }

  bool ChassisControllerPID::isSettled()
  {
    switch (mode)
    {
    case distance:
      return distancePid->isSettled() && anglePid->isSettled();

    case angle:
      return turnPid->isSettled();

    default:
      return true;
    }
  }

  void ChassisControllerPID::waitUntilSettled()
  {
    LOG_INFO_S("ChassisControllerPID: Waiting to settle");

    bool completelySettled = false;

    while (!completelySettled)
    {
      switch (mode)
      {
      case distance:
        completelySettled = waitForDistanceSettled();
        break;

      case angle:
        completelySettled = waitForAngleSettled();
        break;

      default:
        completelySettled = true;
        break;
      }
    }

    // Order here is important
    mode = none;
    doneLooping.store(true, std::memory_order_release);
    doneLoopingSeen.store(false, std::memory_order_release);

    // Wait for the thread to finish if it happens to be writing to motors
    auto rate = timeUtil.getRate();
    while (!doneLoopingSeen.load(std::memory_order_acquire))
    {
      rate->delayUntil(threadSleepTime);
    }

    // Stop after the thread has run at least once
    stopAfterSettled();

    LOG_INFO_S("ChassisControllerPID: Done waiting to settle");
  }

  bool ChassisControllerPID::waitForDistanceSettled()
  {
    LOG_INFO_S("ChassisControllerPID: Waiting to settle in distance mode");

    auto rate = timeUtil.getRate();
    while (!(distancePid->isSettled() && anglePid->isSettled()))
    {
      if (mode == angle)
      {
        // False will cause the loop to re-enter the switch
        LOG_WARN_S("ChassisControllerPID: Mode changed to angle while waiting in distance!");
        return false;
      }

      rate->delayUntil(10_ms);
    }

    // True will cause the loop to exit
    return true;
  }

  bool ChassisControllerPID::waitForAngleSettled()
  {
    LOG_INFO_S("ChassisControllerPID: Waiting to settle in angle mode");

    auto rate = timeUtil.getRate();
    while (!turnPid->isSettled())
    {
      if (mode == distance)
      {
        // False will cause the loop to re-enter the switch
        LOG_WARN_S("ChassisControllerPID: Mode changed to distance while waiting in angle!");
        return false;
      }

      rate->delayUntil(10_ms);
    }

    // True will cause the loop to exit
    return true;
  }

  void ChassisControllerPID::stopAfterSettled()
  {
    distancePid->flipDisable(true);
    anglePid->flipDisable(true);
    turnPid->flipDisable(true);
    chassisModel->stop();
  }

  ChassisScales ChassisControllerPID::getChassisScales() const
  {
    return scales;
  }

  AbstractMotor::GearsetRatioPair ChassisControllerPID::getGearsetRatioPair() const
  {
    return gearsetRatioPair;
  }

  void ChassisControllerPID::setVelocityMode(bool ivelocityMode)
  {
    velocityMode = ivelocityMode;
  }

  void ChassisControllerPID::setGains(const okapi::IterativePosPIDController::Gains &idistanceGains,
                                      const okapi::IterativePosPIDController::Gains &iturnGains,
                                      const okapi::IterativePosPIDController::Gains &iangleGains)
  {
    distancePid->setGains(idistanceGains);
    turnPid->setGains(iturnGains);
    anglePid->setGains(iangleGains);
  }

  std::tuple<IterativePosPIDController::Gains,
             IterativePosPIDController::Gains,
             IterativePosPIDController::Gains>
  ChassisControllerPID::getGains() const
  {
    return std::make_tuple(distancePid->getGains(), turnPid->getGains(), anglePid->getGains());
  }

  void ChassisControllerPID::startThread()
  {
    if (!task)
    {
      task = new CrossplatformThread(trampoline, this, "ChassisControllerPID");
    }
  }

  CrossplatformThread *ChassisControllerPID::getThread() const
  {
    return task;
  }

  void ChassisControllerPID::stop()
  {
    LOG_INFO_S("ChassisControllerPID: Stopping");

    mode = none;
    doneLooping.store(true, std::memory_order_release);
    stopAfterSettled();
  }

  void ChassisControllerPID::setMaxVelocity(double imaxVelocity)
  {
    chassisModel->setMaxVelocity(imaxVelocity);
  }

  double ChassisControllerPID::getMaxVelocity() const
  {
    return chassisModel->getMaxVelocity();
  }

  std::shared_ptr<ChassisModel> ChassisControllerPID::getModel()
  {
    return chassisModel;
  }

  ChassisModel &ChassisControllerPID::model()
  {
    return *chassisModel;
  }
} // namespace okapi
