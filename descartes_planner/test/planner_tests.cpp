#include <gtest/gtest.h>

#include <ros/console.h>

#include <descartes_core/path_planner_base.h>
#include <descartes_planner/dense_planner.h>
#include <descartes_planner/sparse_planner.h>

#include <descartes_trajectory_test/cartesian_robot.h>

#include "utils/trajectory_maker.h"


using namespace descartes_planner;
using namespace descartes_core;

// Factory methods for planner construction
template <class T>
PathPlannerBase* createPathPlanner();

template <>
PathPlannerBase* createPathPlanner<DensePlanner>()
{
  return new DensePlanner();
}

template <>
PathPlannerBase* createPathPlanner<SparsePlanner>()
{
  return new SparsePlanner();
}

// Google Test Framework for Path Planners

template <class T>
class PathPlannerTest : public testing::Test
{
protected:
  typedef T planner_type;

  PathPlannerBasePtr makePlanner()
  {
    PathPlannerBasePtr planner = PathPlannerBasePtr(createPathPlanner<T>());
    EXPECT_TRUE(planner->initialize(robot_));
    return planner;
  }

  PathPlannerTest()
    : velocity_limits_(6, 1.0)
    , robot_(new descartes_trajectory_test::CartesianRobot(5.0, 0.001, velocity_limits_))
  {}

  std::vector<double> velocity_limits_;
  RobotModelConstPtr robot_; 
};

using testing::Types;

// Add types of trajectory points here:
typedef Types<DensePlanner, SparsePlanner> Implementations;

TYPED_TEST_CASE(PathPlannerTest, Implementations);

TYPED_TEST(PathPlannerTest, construction)
{
  PathPlannerBasePtr planner = this->makePlanner();
}

TYPED_TEST(PathPlannerTest, preservesTiming)
{
  using namespace descartes_tests;

  PathPlannerBasePtr planner = this->makePlanner();

  std::vector<TrajectoryPtPtr> input, output;
  // Make input trajectory
  input = makeConstantVelocityTrajectory(Eigen::Vector3d(-1.0, 0, 0), // start position
                                         Eigen::Vector3d(1.0, 0, 0), // end position
                                         0.9, // tool velocity
                                         10); // samples
  // Double the dt of every pt to provide some variety
  double dt = input.front().get()->getTiming().upper;
  for (auto& pt : input)
  {
    pt.get()->setTiming(TimingConstraint(dt));
    dt *= 2.0;
  }
  // // Solve
  EXPECT_TRUE(planner->planPath(input));
  // Get the result
  EXPECT_TRUE(planner->getPath(output));
  // Compare timing
  EXPECT_TRUE(input.size() == output.size());
  for (size_t i = 0; i < input.size(); ++i)
  {
    double t1 = input[i].get()->getTiming().upper;
    double t2 = output[i].get()->getTiming().upper;
    EXPECT_TRUE( std::abs(t1 - t2) < 0.00001 ) <<
      "Input/output timing should correspond for same index: " << t1 << " " << t2;
  }
}

TYPED_TEST(PathPlannerTest, simpleVelocityCheck)
{
  PathPlannerBasePtr planner = this->makePlanner();

  std::vector<descartes_core::TrajectoryPtPtr> input;
  input = descartes_tests::makeConstantVelocityTrajectory(Eigen::Vector3d(-1.0, 0, 0), // start position
                                                          Eigen::Vector3d(1.0, 0, 0), // end position
                                                          0.9, // tool velocity (< 1.0 m/s limit)
                                                          10); // samples
  // The nominal trajectory (0.9 m/s) is less than max tool speed of 1.0 m/s
  EXPECT_TRUE(planner->planPath(input));
  // Unconstraining a point should still succeed
  input[3].get()->setTiming(descartes_core::TimingConstraint());
  EXPECT_TRUE(planner->planPath(input));
  // Making a dt for a segment very small should induce failure
  input[3].get()->setTiming(descartes_core::TimingConstraint(0.001));
  EXPECT_FALSE(planner->planPath(input)) << "Trajectory pt (index 3) has very small dt; planner should fail for velocity out of bounds";
}
