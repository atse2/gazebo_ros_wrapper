/*
 * Copyright (C) 2012-2014 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <gtest/gtest.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <gazebo/common/CommonIface.hh>
#include <gazebo/common/Time.hh>

#include <stdio.h>
#include <string>

// This header file isn't needed if shasums are used
// #include "test/data/pr2_state_log_expected.h"
#include "test_config.h"
#include "gazebo/gazebo_config.h"

std::string custom_exec(std::string _cmd)
{
  _cmd += " 2>/dev/null";
  FILE* pipe = popen(_cmd.c_str(), "r");

  if (!pipe)
    return "ERROR";

  char buffer[128];
  std::string result = "";

  while (!feof(pipe))
  {
    if (fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }

  pclose(pipe);
  return result;
}

/////////////////////////////////////////////////
/// Check to make sure that 'gzlog info' returns correct information
TEST(gzlog, Info)
{
  std::string info = custom_exec(std::string("gzlog info ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(info);

  std::string validInfo =
    "Log Version:    1.0\n"
    "Gazebo Version: 1.4.6\n"
    "Random Seed:    32606\n"
    // "Start:          Feb 08 13 05:35:55.667456998\n"
    // "End:            Feb 08 13 05:35:58.947304437\n"
    // "Duration:       00:00:03.279847439\n"
    // "Steps:          3\n"
    "Size:           12.377 KB\n"
    "Encoding:       bz2";
    // "Model Count:    2";

  EXPECT_EQ(validInfo, info);
}

/////////////////////////////////////////////////
/// Check to make sure that 'gzlog echo' returns correct information
TEST(gzlog, Echo)
{
  std::string echo = custom_exec(std::string("gzlog echo ") +
      PROJECT_SOURCE_PATH + "/test/data/empty_state.log");
  boost::trim_right(echo);

  std::string validEcho =
    "<?xml version='1.0'?>\n<gazebo_log>\n<header>\n<log_version>1.0"
    "</log_version>\n<gazebo_version>1.4.6</gazebo_version>\n"
    "<rand_seed>24794</rand_seed>\n</header>\n\n<chunk encoding='txt'>"
    "<![CDATA[\n<sdf version ='1.3'>\n<world name='default'>\n  "
    "<light name='sun' type='directional'>\n    <cast_shadows>1"
    "</cast_shadows>\n    <pose>0.000000 0.000000 10.000000 0.000000 0.000000 "
    "0.000000</pose>\n    <diffuse>0.800000 0.800000 0.800000 1.000000"
    "</diffuse>\n    <specular>0.100000 0.100000 0.100000 1.000000"
    "</specular>\n    <attenuation>\n      <range>1000.000000</range>\n      "
    "<constant>0.900000</constant>\n      <linear>0.010000</linear>\n      "
    "<quadratic>0.001000</quadratic>\n    </attenuation>\n    "
    "<direction>-0.500000 0.500000 -1.000000</direction>\n  </light>\n  "
    "<model name='ground_plane'>\n    <static>1</static>\n    "
    "<link name='link'>\n      <collision name='collision'>\n        "
    "<geometry>\n          <plane>\n            <normal>0.000000 0.000000 "
    "1.000000</normal>\n            <size>100.000000 100.000000"
    "</size>\n          </plane>\n        </geometry>\n        "
    "<surface>\n          <friction>\n            <ode>\n              "
    "<mu>100.000000</mu>\n              <mu2>50.000000</mu2>\n            "
    "</ode>\n          </friction>\n          <bounce/>\n          "
    "<contact>\n            <ode/>\n          </contact>\n        "
    "</surface>\n      </collision>\n      <visual name='visual'>\n        "
    "<cast_shadows>0</cast_shadows>\n        <geometry>\n          "
    "<plane>\n            <normal>0.000000 0.000000 1.000000"
    "</normal>\n            <size>100.000000 100.000000</size>\n          "
    "</plane>\n        </geometry>\n        <material>\n          "
    "<script>\n            <uri>file://media/materials/scripts/gazebo.material"
    "</uri>\n            <name>Gazebo/Grey</name>\n          "
    "</script>\n        </material>\n      </visual>\n      "
    "<velocity_decay>\n        <linear>0.000000</linear>\n        "
    "<angular>0.000000</angular>\n      </velocity_decay>\n      "
    "<self_collide>0</self_collide>\n      <kinematic>0"
    "</kinematic>\n      <gravity>1</gravity>\n    </link>\n  "
    "</model>\n  <physics type='ode'>\n    <update_rate>1000.000000"
    "</update_rate>\n    <gravity>0.000000 0.000000 -9.800000"
    "</gravity>\n  </physics>\n  <scene>\n    <ambient>0.200000 0.200000 "
    "0.200000 1.000000</ambient>\n    <background>0.700000 0.700000 "
    "0.700000 1.000000</background>\n    <shadows>1</shadows>\n  "
    "</scene>\n  <state world_name='default'>\n    <sim_time>0 0"
    "</sim_time>\n    <real_time>0 0</real_time>\n    <wall_time>"
    "1360300141 918692496</wall_time>\n  </state>\n</world>\n</sdf>]]>"
    "</chunk>\n</gazebo_log>";

  EXPECT_EQ(validEcho, echo);
}

/////////////////////////////////////////////////
/// Check to make sure that 'gzlog echo --filter' returns correct information
TEST(gzlog, EchoFilter)
{
  std::string echo, shasum;

  // Test model filter
  echo = custom_exec(
      std::string("gzlog echo --filter pr2 ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2StateLog, echo);
  EXPECT_EQ(shasum, "0bf1f293b164bbe820267f970c4b419acdca4b01");

  echo = custom_exec(
      std::string("gzlog echo --filter pr2.pose ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2PoseStateLog, echo);
  EXPECT_EQ(shasum, "33db2cbd0841466a67abd7d2bbc69cf2cfae19b6");

  echo = custom_exec(
      std::string("gzlog echo --filter pr2.pose.x ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2PoseXStateLog, echo);
  EXPECT_EQ(shasum, "07113f16d44e2484f769fd1947ff5dca93f55cf4");

  echo = custom_exec(
      std::string("gzlog echo --filter pr2.pose.[x,y] ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2PoseXYStateLog, echo);
  EXPECT_EQ(shasum, "7f34f3fac505707727a74ac8659bb8736932ab07");

  // Test link filter
  echo = custom_exec(
      std::string("gzlog echo --filter pr2/r_upper*.pose ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2LinkStateLog, echo);
  EXPECT_EQ(shasum, "d52ba4333511b7e4339db3eb71814c73473fba36");

  // Test joint filter
  echo = custom_exec(
      std::string("gzlog echo --filter pr2//r_upper_arm_roll_joint ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  shasum = gazebo::common::get_sha1<std::string>(echo);
  // EXPECT_EQ(pr2JointStateLog, echo);
  EXPECT_EQ(shasum, "2f689dadc66171a76f7f3400bc218485a923c324");
}

/////////////////////////////////////////////////
/// Check to Hz filtering
TEST(gzlog, HzFilter)
{
  std::string echo, validEcho;

  // Test Hz filter
  echo = custom_exec(
      std::string("gzlog echo -r -z 1.0 --filter pr2.pose.z ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = "-0.000008";
  EXPECT_EQ(validEcho, echo);

  // Test zero Hz filter
  echo = custom_exec(
      std::string("gzlog echo -r -z 0 --filter pr2.pose.z ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = "-0.000008 \n-0.000015";
  EXPECT_EQ(validEcho, echo);

  // Test negative Hz filter
  echo = custom_exec(
      std::string("gzlog echo -r -z -1.0 --filter pr2.pose.z ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = "-0.000008 \n-0.000015";
  EXPECT_EQ(validEcho, echo);
}

/////////////////////////////////////////////////
/// Check to raw filtering with time stamps
TEST(gzlog, RawFilterStamp)
{
  std::string echo, validEcho;

  // Sim time
  echo = custom_exec(
      std::string("gzlog echo -r -s sim --filter pr2.pose.x ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = "0.021344 0.000000 \n0.0289582 0.000000";
  EXPECT_EQ(validEcho, echo);

  // Real time
  echo = custom_exec(
      std::string("gzlog echo -r -s real --filter pr2.pose.x ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = "0.001 0.000000 \n0.002 0.000000";
  EXPECT_EQ(validEcho, echo);

  // Wall time
  echo = custom_exec(
      std::string("gzlog echo -r -s wall --filter pr2.pose.x ") +
      PROJECT_SOURCE_PATH + "/test/data/pr2_state.log");
  boost::trim_right(echo);
  validEcho = std::string("1360301758.939690 0.000000 \n")
            + std::string("1360301758.947304 0.000000");
  EXPECT_EQ(validEcho, echo);
}

/////////////////////////////////////////////////
/// Check to make sure that 'gzlog step' returns correct information
/// Just check number of characters returned for now
TEST(gzlog, Step)
{
  std::string stepCmd, shasum;
  stepCmd = std::string("gzlog step ") + PROJECT_SOURCE_PATH +
    std::string("/test/data/pr2_state.log");

  // Call gzlog step and press q immediately
  std::string stepq0 = custom_exec(std::string("echo 'q' | ") + stepCmd);
  shasum = gazebo::common::get_sha1<std::string>(stepq0);
  EXPECT_EQ(shasum, "6d3af4f4d1214fe3a4860ab42777eb4d0f89c6b2");

  // Call gzlog step and press space once, then q
  std::string stepq1 = custom_exec(std::string("echo ' q' | ") + stepCmd);
  shasum = gazebo::common::get_sha1<std::string>(stepq1);
  EXPECT_EQ(shasum, "43eacb140e00ef0525d54667bc558d63dac3d21f");

  // Call gzlog step and press space twice, then q
  std::string stepq2 = custom_exec(std::string("echo '  q' | ") + stepCmd);
  shasum = gazebo::common::get_sha1<std::string>(stepq2);
  EXPECT_EQ(shasum, "37e133d15d3f74cbc686bfceb26b8db46e2f6bf5");
}

/////////////////////////////////////////////////
TEST(gzlog, HangCheck)
{
  gazebo::common::Time start = gazebo::common::Time::GetWallTime();
  custom_exec("gzlog stop");
  gazebo::common::Time end = gazebo::common::Time::GetWallTime();

  EXPECT_LT(end - start, gazebo::common::Time(60, 0));
}

/////////////////////////////////////////////////
/// Main
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
