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

#include <stdio.h>
#include <string>

#include "ServerFixture.hh"

using namespace gazebo;

/////////////////////////////////////////////////
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
ServerFixture::ServerFixture()
{
  this->server = NULL;
  this->serverRunning = false;
  this->paused = false;
  this->percentRealTime = 0;
  this->gotImage = 0;
  this->imgData = NULL;
  this->serverThread = NULL;

  common::Console::Instance()->Init("test.log");
  common::SystemPaths::Instance()->AddGazeboPaths(
      TEST_INTEGRATION_PATH);

  // Add local search paths
  std::string path = TEST_INTEGRATION_PATH;
  path += "/../..";
  gazebo::common::SystemPaths::Instance()->AddGazeboPaths(path);

  path = TEST_INTEGRATION_PATH;
  path += "/../../sdf";
  gazebo::common::SystemPaths::Instance()->AddGazeboPaths(path);

  path = TEST_INTEGRATION_PATH;
  path += "/../../gazebo";
  gazebo::common::SystemPaths::Instance()->AddGazeboPaths(path);

  path = TEST_INTEGRATION_PATH;
  path += "/../../build/plugins";
  gazebo::common::SystemPaths::Instance()->AddPluginPaths(path);

  path = TEST_PATH;
  gazebo::common::SystemPaths::Instance()->AddGazeboPaths(path);
}

/////////////////////////////////////////////////
void ServerFixture::TearDown()
{
  this->Unload();
}

/////////////////////////////////////////////////
void ServerFixture::Unload()
{
  gzdbg << "ServerFixture::Unload" << std::endl;
  this->serverRunning = false;
  if (this->node)
    this->node->Fini();

  if (this->server)
  {
    this->server->Stop();

    if (this->serverThread)
    {
      this->serverThread->join();
    }
  }

  delete this->serverThread;
  this->serverThread = NULL;
}

/////////////////////////////////////////////////
void ServerFixture::Load(const std::string &_worldFilename)
{
  this->Load(_worldFilename, false);
}

/////////////////////////////////////////////////
void ServerFixture::Load(const std::string &_worldFilename, bool _paused)
{
  this->Load(_worldFilename, _paused, "");
}

/////////////////////////////////////////////////
void ServerFixture::Load(const std::string &_worldFilename,
                  bool _paused, const std::string &_physics)
{
  delete this->server;
  this->server = NULL;

  // Create, load, and run the server in its own thread
  this->serverThread = new boost::thread(
     boost::bind(&ServerFixture::RunServer, this, _worldFilename,
                 _paused, _physics));

  // Wait for the server to come up
  // Use a 60 second timeout.
  int waitCount = 0, maxWaitCount = 6000;
  while ((!this->server || !this->server->GetInitialized()) &&
         ++waitCount < maxWaitCount)
    common::Time::MSleep(100);
  gzdbg << "ServerFixture load in "
         << static_cast<double>(waitCount)/10.0
         << " seconds, timeout after "
         << static_cast<double>(maxWaitCount)/10.0
         << " seconds\n";

  if (waitCount >= maxWaitCount)
    this->launchTimeoutFailure(
        "while waiting for Load() function", waitCount);

  this->node = transport::NodePtr(new transport::Node());
  ASSERT_NO_THROW(this->node->Init());
  this->poseSub = this->node->Subscribe("~/pose/local/info",
      &ServerFixture::OnPose, this, true);
  this->statsSub = this->node->Subscribe("~/world_stats",
      &ServerFixture::OnStats, this);

  this->factoryPub =
    this->node->Advertise<msgs::Factory>("~/factory");

  this->requestPub =
    this->node->Advertise<msgs::Request>("~/request");

  // Wait for the world to reach the correct pause state.
  // This might not work properly with multiple worlds.
  // Use a 30 second timeout.
  waitCount = 0;
  maxWaitCount = 3000;
  while ((!physics::get_world() ||
           physics::get_world()->IsPaused() != _paused) &&
         ++waitCount < maxWaitCount)
    common::Time::MSleep(100);
  ASSERT_LT(waitCount, maxWaitCount);

  this->factoryPub->WaitForConnection();
  this->requestPub->WaitForConnection();
}

/////////////////////////////////////////////////
void ServerFixture::RunServer(const std::string &_worldFilename)
{
  this->RunServer(_worldFilename, false, "");
}

/////////////////////////////////////////////////
rendering::ScenePtr ServerFixture::GetScene(
    const std::string &_sceneName)
{
  // Wait for the scene to get loaded.
  int i = 0;
  int timeoutDS = 20;
  while (rendering::get_scene(_sceneName) == NULL && i < timeoutDS)
  {
    common::Time::MSleep(100);
    ++i;
  }

  if (i >= timeoutDS)
  {
    gzerr << "Unable to load the rendering scene.\n"
          << "Test will fail";
    this->launchTimeoutFailure(
        "while waiting to load rendering scene", i);
  }

  return rendering::get_scene(_sceneName);
}

/////////////////////////////////////////////////
void ServerFixture::RunServer(const std::string &_worldFilename, bool _paused,
               const std::string &_physics)
{
  ASSERT_NO_THROW(this->server = new Server());
  this->server->PreLoad();
  if (_physics.length())
    ASSERT_NO_THROW(this->server->LoadFile(_worldFilename,
                                           _physics));
  else
    ASSERT_NO_THROW(this->server->LoadFile(_worldFilename));

  if (!rendering::get_scene(
        gazebo::physics::get_world()->GetName()))
  {
    rendering::create_scene(
        gazebo::physics::get_world()->GetName(), false, true);
  }

  this->SetPause(_paused);

  this->server->Run();

  ASSERT_NO_THROW(this->server->Fini());

  delete this->server;
  this->server = NULL;
}

/////////////////////////////////////////////////
void ServerFixture::OnStats(ConstWorldStatisticsPtr &_msg)
{
  this->simTime = msgs::Convert(_msg->sim_time());
  this->realTime = msgs::Convert(_msg->real_time());
  this->pauseTime = msgs::Convert(_msg->pause_time());
  this->paused = _msg->paused();

  if (this->realTime == 0)
    this->percentRealTime = 0;
  else
    this->percentRealTime =
      (this->simTime / this->realTime).Double();

  this->serverRunning = true;
}

/////////////////////////////////////////////////
void ServerFixture::SetPause(bool _pause)
{
  physics::pause_worlds(_pause);
}

/////////////////////////////////////////////////
double ServerFixture::GetPercentRealTime() const
{
  while (!this->serverRunning)
    common::Time::MSleep(100);

  return this->percentRealTime;
}

/////////////////////////////////////////////////
void ServerFixture::OnPose(ConstPosesStampedPtr &_msg)
{
  boost::mutex::scoped_lock lock(this->receiveMutex);
  for (int i = 0; i < _msg->pose_size(); ++i)
  {
    this->poses[_msg->pose(i).name()] =
      msgs::Convert(_msg->pose(i));
  }
}

/////////////////////////////////////////////////
math::Pose ServerFixture::GetEntityPose(const std::string &_name)
{
  boost::mutex::scoped_lock lock(this->receiveMutex);

  std::map<std::string, math::Pose>::iterator iter;
  iter = this->poses.find(_name);
  EXPECT_TRUE(iter != this->poses.end());
  return iter->second;
}

/////////////////////////////////////////////////
bool ServerFixture::HasEntity(const std::string &_name)
{
  boost::mutex::scoped_lock lock(this->receiveMutex);
  std::map<std::string, math::Pose>::iterator iter;
  iter = this->poses.find(_name);
  return iter != this->poses.end();
}

/////////////////////////////////////////////////
void ServerFixture::PrintImage(const std::string &_name, unsigned char **_image,
    unsigned int _width, unsigned int _height, unsigned int _depth)
{
  unsigned int count = _height * _width * _depth;
  printf("\n");
  printf("static unsigned char __%s[] = {", _name.c_str());
  unsigned int i;
  for (i = 0; i < count-1; i++)
  {
    if (i % 10 == 0)
      printf("\n");
    else
      printf(" ");
    printf("%d,", (*_image)[i]);
  }
  printf(" %d};\n", (*_image)[i]);
  printf("static unsigned char *%s = __%s;\n", _name.c_str(),
      _name.c_str());
}

/////////////////////////////////////////////////
void ServerFixture::PrintScan(const std::string &_name, double *_scan,
               unsigned int _sampleCount)
{
  printf("static double __%s[] = {\n", _name.c_str());
  for (unsigned int i = 0; i < _sampleCount-1; ++i)
  {
    if ((i+1) % 5 == 0)
      printf("%13.10f,\n", math::precision(_scan[i], 10));
    else
      printf("%13.10f, ", math::precision(_scan[i], 10));
  }
  printf("%13.10f};\n",
      math::precision(_scan[_sampleCount-1], 10));
  printf("static double *%s = __%s;\n", _name.c_str(),
      _name.c_str());
}

/////////////////////////////////////////////////
void ServerFixture::FloatCompare(float *_scanA, float *_scanB,
    unsigned int _sampleCount, float &_diffMax,
    float &_diffSum, float &_diffAvg)
{
  float diff;
  _diffMax = 0;
  _diffSum = 0;
  _diffAvg = 0;
  for (unsigned int i = 0; i < _sampleCount; ++i)
  {
    diff = fabs(math::precision(_scanA[i], 10) -
                math::precision(_scanB[i], 10));
    _diffSum += diff;
    if (diff > _diffMax)
    {
      _diffMax = diff;
    }
  }
  _diffAvg = _diffSum / _sampleCount;
}

/////////////////////////////////////////////////
void ServerFixture::DoubleCompare(double *_scanA, double *_scanB,
    unsigned int _sampleCount, double &_diffMax,
    double &_diffSum, double &_diffAvg)
{
  double diff;
  _diffMax = 0;
  _diffSum = 0;
  _diffAvg = 0;
  for (unsigned int i = 0; i < _sampleCount; ++i)
  {
    diff = fabs(math::precision(_scanA[i], 10) -
                math::precision(_scanB[i], 10));
    _diffSum += diff;
    if (diff > _diffMax)
    {
      _diffMax = diff;
    }
  }
  _diffAvg = _diffSum / _sampleCount;
}

/////////////////////////////////////////////////
void ServerFixture::ImageCompare(unsigned char *_imageA,
    unsigned char *_imageB,
    unsigned int _width, unsigned int _height, unsigned int _depth,
    unsigned int &_diffMax, unsigned int &_diffSum,
    double &_diffAvg)
{
  _diffMax = 0;
  _diffSum = 0;
  _diffAvg = 0;

  for (unsigned int y = 0; y < _height; y++)
  {
    for (unsigned int x = 0; x < _width*_depth; x++)
    {
      unsigned int a = _imageA[(y*_width*_depth)+x];
      unsigned int b = _imageB[(y*_width*_depth)+x];

      unsigned int diff = (unsigned int)(abs(a - b));

      if (diff > _diffMax)
        _diffMax = diff;

      _diffSum += diff;
    }
  }
  _diffAvg = _diffSum / (_height*_width*_depth);
}

/////////////////////////////////////////////////
void ServerFixture::OnNewFrame(const unsigned char *_image,
                 unsigned int _width, unsigned int _height,
                 unsigned int _depth,
                 const std::string &/*_format*/)
{
  memcpy(*this->imgData, _image, _width * _height * _depth);
  this->gotImage+= 1;
}

/////////////////////////////////////////////////
void ServerFixture::GetFrame(const std::string &_cameraName,
    unsigned char **_imgData, unsigned int &_width,
    unsigned int &_height)
{
  sensors::SensorPtr sensor = sensors::get_sensor(_cameraName);
  EXPECT_TRUE(sensor);
  sensors::CameraSensorPtr camSensor =
    boost::dynamic_pointer_cast<sensors::CameraSensor>(sensor);

  _width = camSensor->GetImageWidth();
  _height = camSensor->GetImageHeight();

  if (*_imgData)
  {
    delete *_imgData;
    *_imgData = NULL;
  }
  (*_imgData) = new unsigned char[_width *_height*3];
  this->imgData = _imgData;

  this->gotImage = 0;
  event::ConnectionPtr c =
    camSensor->GetCamera()->ConnectNewImageFrame(
        boost::bind(&ServerFixture::OnNewFrame,
                    this, _1, _2, _3, _4, _5));

  while (this->gotImage < 20)
    common::Time::MSleep(100);

  camSensor->GetCamera()->DisconnectNewImageFrame(c);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnCamera(const std::string &_modelName,
    const std::string &_cameraName,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    unsigned int _width, unsigned int _height,
    double _rate,
    const std::string &_noiseType,
    double _noiseMean,
    double _noiseStdDev)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _modelName << "'>"
    << "<static>true</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <sensor name ='" << _cameraName
    << "' type ='camera'>"
    << "    <always_on>1</always_on>"
    << "    <update_rate>" << _rate << "</update_rate>"
    << "    <visualize>true</visualize>"
    << "    <camera>"
    << "      <horizontal_fov>0.78539816339744828</horizontal_fov>"
    << "      <image>"
    << "        <width>" << _width << "</width>"
    << "        <height>" << _height << "</height>"
    << "        <format>R8G8B8</format>"
    << "      </image>"
    << "      <clip>"
    << "        <near>0.1</near><far>100</far>"
    << "      </clip>";
    // << "      <save enabled ='true' path ='/tmp/camera/'/>"

  if (_noiseType.size() > 0)
    newModelStr << "      <noise>"
    << "        <type>" << _noiseType << "</type>"
    << "        <mean>" << _noiseMean << "</mean>"
    << "        <stddev>" << _noiseStdDev << "</stddev>"
    << "      </noise>";

  newModelStr << "    </camera>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_modelName, 100, 50);
  WaitUntilSensorSpawn(_cameraName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnRaySensor(const std::string &_modelName,
    const std::string &_raySensorName,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    double _hMinAngle, double _hMaxAngle,
    double _minRange, double _maxRange,
    double _rangeResolution, unsigned int _samples,
    const std::string &_noiseType, double _noiseMean,
    double _noiseStdDev)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _modelName << "'>"
    << "<static>true</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "<collision name='parent_collision'>"
    << "  <pose>0 0 0.0205 0 0 0</pose>"
    << "  <geometry>"
    << "    <cylinder>"
    << "      <radius>0.021</radius>"
    << "      <length>0.029</length>"
    << "    </cylinder>"
    << "  </geometry>"
    << "</collision>"
    << "  <sensor name ='" << _raySensorName << "' type ='ray'>"
    << "    <ray>"
    << "      <scan>"
    << "        <horizontal>"
    << "          <samples>" << _samples << "</samples>"
    << "          <resolution> 1 </resolution>"
    << "          <min_angle>" << _hMinAngle << "</min_angle>"
    << "          <max_angle>" << _hMaxAngle << "</max_angle>"
    << "        </horizontal>"
    << "      </scan>"
    << "      <range>"
    << "        <min>" << _minRange << "</min>"
    << "        <max>" << _maxRange << "</max>"
    << "        <resolution>" << _rangeResolution <<"</resolution>"
    << "      </range>";

  if (_noiseType.size() > 0)
    newModelStr << "      <noise>"
    << "        <type>" << _noiseType << "</type>"
    << "        <mean>" << _noiseMean << "</mean>"
    << "        <stddev>" << _noiseStdDev << "</stddev>"
    << "      </noise>";

  newModelStr << "    </ray>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_modelName, 100, 100);
  WaitUntilSensorSpawn(_raySensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnGpuRaySensor(const std::string &_modelName,
    const std::string &_raySensorName,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    double _hMinAngle, double _hMaxAngle,
    double _minRange, double _maxRange,
    double _rangeResolution, unsigned int _samples,
    const std::string &_noiseType, double _noiseMean,
    double _noiseStdDev)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _modelName << "'>"
    << "<static>true</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "<collision name='parent_collision'>"
    << "  <pose>0 0 0.0205 0 0 0</pose>"
    << "  <geometry>"
    << "    <cylinder>"
    << "      <radius>0.021</radius>"
    << "      <length>0.029</length>"
    << "    </cylinder>"
    << "  </geometry>"
    << "</collision>"
    << "  <sensor name ='" << _raySensorName
    << "' type ='gpu_ray'>"
    << "    <ray>"
    << "      <scan>"
    << "        <horizontal>"
    << "          <samples>" << _samples << "</samples>"
    << "          <resolution> 1 </resolution>"
    << "          <min_angle>" << _hMinAngle << "</min_angle>"
    << "          <max_angle>" << _hMaxAngle << "</max_angle>"
    << "        </horizontal>"
    << "      </scan>"
    << "      <range>"
    << "        <min>" << _minRange << "</min>"
    << "        <max>" << _maxRange << "</max>"
    << "        <resolution>" << _rangeResolution <<"</resolution>"
    << "      </range>";

  if (_noiseType.size() > 0)
    newModelStr << "      <noise>"
    << "        <type>" << _noiseType << "</type>"
    << "        <mean>" << _noiseMean << "</mean>"
    << "        <stddev>" << _noiseStdDev << "</stddev>"
    << "      </noise>";

  newModelStr << "    </ray>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_modelName, 100, 100);
  WaitUntilSensorSpawn(_raySensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnImuSensor(const std::string &_modelName,
    const std::string &_imuSensorName,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    const std::string &_noiseType,
    double _rateNoiseMean, double _rateNoiseStdDev,
    double _rateBiasMean, double _rateBiasStdDev,
    double _accelNoiseMean, double _accelNoiseStdDev,
    double _accelBiasMean, double _accelBiasStdDev)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _modelName << "'>" << std::endl
    << "<static>true</static>" << std::endl
    << "<pose>" << _pos << " " << _rpy << "</pose>" << std::endl
    << "<link name ='body'>" << std::endl
    << "<inertial>" << std::endl
    << "<mass>0.1</mass>" << std::endl
    << "</inertial>" << std::endl
    << "<collision name='parent_collision'>" << std::endl
    << "  <pose>0 0 0.0205 0 0 0</pose>" << std::endl
    << "  <geometry>" << std::endl
    << "    <cylinder>" << std::endl
    << "      <radius>0.021</radius>" << std::endl
    << "      <length>0.029</length>" << std::endl
    << "    </cylinder>" << std::endl
    << "  </geometry>" << std::endl
    << "</collision>" << std::endl
    << "  <sensor name ='" << _imuSensorName
    << "' type ='imu'>" << std::endl
    << "    <imu>" << std::endl;

  if (_noiseType.size() > 0)
  {
    newModelStr << "      <noise>" << std::endl
    << "        <type>" << _noiseType << "</type>" << std::endl
    << "        <rate>" << std::endl
    << "          <mean>" << _rateNoiseMean
    << "</mean>" << std::endl
    << "          <stddev>" << _rateNoiseStdDev
    << "</stddev>" << std::endl
    << "          <bias_mean>" << _rateBiasMean
    << "</bias_mean>" << std::endl
    << "          <bias_stddev>" << _rateBiasStdDev
    << "</bias_stddev>" << std::endl
    << "        </rate>" << std::endl
    << "        <accel>" << std::endl
    << "          <mean>" << _accelNoiseMean << "</mean>"
    << std::endl
    << "          <stddev>" << _accelNoiseStdDev << "</stddev>"
    << std::endl
    << "          <bias_mean>" << _accelBiasMean
    << "</bias_mean>" << std::endl
    << "          <bias_stddev>" << _accelBiasStdDev
    << "</bias_stddev>" << std::endl
    << "        </accel>" << std::endl
    << "      </noise>" << std::endl;
  }

  newModelStr << "    </imu>" << std::endl
    << "  </sensor>" << std::endl
    << "</link>" << std::endl
    << "</model>" << std::endl
    << "</sdf>" << std::endl;

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_modelName, 100, 1000);
  WaitUntilSensorSpawn(_imuSensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnUnitContactSensor(const std::string &_name,
    const std::string &_sensorName,
    const std::string &_collisionType, const math::Vector3 &_pos,
    const math::Vector3 &_rpy, bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;
  std::ostringstream shapeStr;

  if (_collisionType == "box")
  {
    shapeStr << " <box><size>1 1 1</size></box>";
  }
  else if (_collisionType == "cylinder")
  {
    shapeStr << "<cylinder>"
             << "  <radius>.5</radius><length>1.0</length>"
             << "</cylinder>";
  }

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='contact_collision'>"
    << "    <geometry>"
    << shapeStr.str()
    << "    </geometry>"
    << "    <surface>"
    << "      <contact>"
    << "        <ode>"
    << "          <min_depth>0.005</min_depth>"
    << "        </ode>"
    << "      </contact>"
    << "    </surface>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << shapeStr.str()
    << "    </geometry>"
    << "  </visual>"
    << "  <sensor name='" << _sensorName << "' type='contact'>"
    << "    <contact>"
    << "      <collision>contact_collision</collision>"
    << "    </contact>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_name, 100, 100);
  WaitUntilSensorSpawn(_sensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnUnitImuSensor(const std::string &_name,
    const std::string &_sensorName,
    const std::string &_collisionType,
    const std::string &_topic, const math::Vector3 &_pos,
    const math::Vector3 &_rpy, bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;
  std::ostringstream shapeStr;
  if (_collisionType == "box")
    shapeStr << " <box><size>1 1 1</size></box>";
  else if (_collisionType == "cylinder")
  {
    shapeStr << "<cylinder>"
             << "  <radius>.5</radius><length>1.0</length>"
             << "</cylinder>";
  }
  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='contact_collision'>"
    << "    <geometry>"
    << shapeStr.str()
    << "    </geometry>"
    << "    <surface>"
    << "      <contact>"
    << "        <ode>"
    << "          <min_depth>0.01</min_depth>"
    << "        </ode>"
    << "      </contact>"
    << "    </surface>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << shapeStr.str()
    << "    </geometry>"
    << "  </visual>"
    << "  <sensor name='" << _sensorName << "' type='imu'>"
    << "    <imu>"
    << "      <topic>" << _topic << "</topic>"
    << "    </imu>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_name, 20, 50);
  WaitUntilSensorSpawn(_sensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::launchTimeoutFailure(const char *_logMsg,
                                         const int _timeoutCS)
{
     FAIL() << "ServerFixture timeout (wait more than " << _timeoutCS / 100
            << "s): " << _logMsg;
}

/////////////////////////////////////////////////
void ServerFixture::SpawnWirelessTransmitterSensor(const std::string &_name,
    const std::string &_sensorName,
    const math::Vector3 &_pos,
    const math::Vector3 &_rpy,
    const std::string &_essid,
    double _freq,
    double _power,
    double _gain,
    bool _visualize)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>true</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='link'>"
    << "  <sensor name='" << _sensorName
    <<         "' type='wireless_transmitter'>"
    << "    <always_on>1</always_on>"
    << "    <update_rate>1</update_rate>"
    << "    <visualize>" << _visualize << "</visualize>"
    << "    <transceiver>"
    << "      <essid>" << _essid << "</essid>"
    << "      <frequency>" << _freq << "</frequency>"
    << "      <power>" << _power << "</power>"
    << "      <gain>" << _gain << "</gain>"
    << "    </transceiver>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_name, 100, 100);
  WaitUntilSensorSpawn(_sensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnWirelessReceiverSensor(const std::string &_name,
    const std::string &_sensorName,
    const math::Vector3 &_pos,
    const math::Vector3 &_rpy,
    double _minFreq,
    double _maxFreq,
    double _power,
    double _gain,
    double _sensitivity,
    bool _visualize)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>true</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='link'>"
    << "  <sensor name='" << _sensorName
    <<         "' type='wireless_receiver'>"
    << "    <update_rate>1</update_rate>"
    << "    <visualize>" << _visualize << "</visualize>"
    << "    <transceiver>"
    << "      <min_frequency>" << _minFreq << "</min_frequency>"
    << "      <max_frequency>" << _maxFreq << "</max_frequency>"
    << "      <power>" << _power << "</power>"
    << "      <gain>" << _gain << "</gain>"
    << "      <sensitivity>" << _sensitivity << "</sensitivity>"
    << "    </transceiver>"
    << "  </sensor>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  WaitUntilEntitySpawn(_name, 100, 100);
  WaitUntilSensorSpawn(_sensorName, 100, 100);
}

/////////////////////////////////////////////////
void ServerFixture::WaitUntilEntitySpawn(const std::string &_name,
                        unsigned int _sleepEach,
                        int _retries)
{
  int i = 0;
  // Wait for the entity to spawn
  while (!this->HasEntity(_name) && i < _retries)
  {
    common::Time::MSleep(_sleepEach);
    ++i;
  }
  EXPECT_LT(i, _retries);

  if (i >= _retries)
    FAIL() << "ServerFixture timeout: max number of retries ("
           << _retries
           << ") exceeded while awaiting the spawn of " << _name;
}

/////////////////////////////////////////////////
void ServerFixture::WaitUntilSensorSpawn(const std::string &_name,
                        unsigned int _sleepEach,
                        int _retries)
{
  int i = 0;
  // Wait for the sensor to spawn
  while (!sensors::get_sensor(_name) && i < _retries)
  {
    common::Time::MSleep(_sleepEach);
    ++i;
  }
  EXPECT_LT(i, _retries);

  if (i >= _retries)
    FAIL() << "ServerFixture timeout: max number of retries ("
           << _retries
           << ") exceeded while awaiting the spawn of " << _name;
}

/////////////////////////////////////////////////
void ServerFixture::SpawnCylinder(const std::string &_name,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='geom'>"
    << "    <geometry>"
    << "      <cylinder>"
    << "        <radius>.5</radius><length>1.0</length>"
    << "      </cylinder>"
    << "    </geometry>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << "      <cylinder>"
    << "        <radius>.5</radius><length>1.0</length>"
    << "      </cylinder>"
    << "    </geometry>"
    << "  </visual>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (!this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnSphere(const std::string &_name,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    bool _wait, bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='geom'>"
    << "    <geometry>"
    << "      <sphere><radius>.5</radius></sphere>"
    << "    </geometry>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << "      <sphere><radius>.5</radius></sphere>"
    << "    </geometry>"
    << "  </visual>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (_wait && !this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnSphere(const std::string &_name,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    const math::Vector3 &_cog, double _radius,
    bool _wait, bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <inertial>"
    << "    <pose>" << _cog << " 0 0 0</pose>"
    << "  </inertial>"
    << "  <collision name ='geom'>"
    << "    <geometry>"
    << "      <sphere><radius>" << _radius << "</radius></sphere>"
    << "    </geometry>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << "      <sphere><radius>" << _radius << "</radius></sphere>"
    << "    </geometry>"
    << "  </visual>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (_wait && !this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnBox(const std::string &_name,
    const math::Vector3 &_size, const math::Vector3 &_pos,
    const math::Vector3 &_rpy, bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='geom'>"
    << "    <geometry>"
    << "      <box><size>" << _size << "</size></box>"
    << "    </geometry>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << "      <box><size>" << _size << "</size></box>"
    << "    </geometry>"
    << "  </visual>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (!this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnTrimesh(const std::string &_name,
    const std::string &_modelPath, const math::Vector3 &_scale,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "  <collision name ='geom'>"
    << "    <geometry>"
    << "      <mesh>"
    << "        <uri>" << _modelPath << "</uri>"
    << "        <scale>" << _scale << "</scale>"
    << "      </mesh>"
    << "    </geometry>"
    << "  </collision>"
    << "  <visual name ='visual'>"
    << "    <geometry>"
    << "      <mesh><uri>" << _modelPath << "</uri></mesh>"
    << "    </geometry>"
    << "  </visual>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (!this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnEmptyLink(const std::string &_name,
    const math::Vector3 &_pos, const math::Vector3 &_rpy,
    bool _static)
{
  msgs::Factory msg;
  std::ostringstream newModelStr;

  newModelStr << "<sdf version='" << SDF_VERSION << "'>"
    << "<model name ='" << _name << "'>"
    << "<static>" << _static << "</static>"
    << "<pose>" << _pos << " " << _rpy << "</pose>"
    << "<link name ='body'>"
    << "</link>"
    << "</model>"
    << "</sdf>";

  msg.set_sdf(newModelStr.str());
  this->factoryPub->Publish(msg);

  // Wait for the entity to spawn
  while (!this->HasEntity(_name))
    common::Time::MSleep(100);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnModel(const std::string &_filename)
{
  msgs::Factory msg;
  msg.set_sdf_filename(_filename);
  this->factoryPub->Publish(msg);
}

/////////////////////////////////////////////////
void ServerFixture::SpawnSDF(const std::string &_sdf)
{
  msgs::Factory msg;
  msg.set_sdf(_sdf);
  this->factoryPub->Publish(msg);

  // The code above sends a message, but it will take some time
  // before the message is processed.
  //
  // The code below parses the sdf string to find a model name,
  // then this function will block until that model
  // has been processed and recognized by the Server Fixture.
  sdf::SDF sdfParsed;
  sdfParsed.SetFromString(_sdf);
  // Check that sdf contains a model
  if (sdfParsed.root->HasElement("model"))
  {
    // Timeout of 30 seconds (3000 * 10 ms)
    int waitCount = 0, maxWaitCount = 3000;
    sdf::ElementPtr model = sdfParsed.root->GetElement("model");
    std::string name = model->Get<std::string>("name");
    while (!this->HasEntity(name) && ++waitCount < maxWaitCount)
      common::Time::MSleep(100);
    ASSERT_LT(waitCount, maxWaitCount);
  }
}

/////////////////////////////////////////////////
void ServerFixture::LoadPlugin(const std::string &_filename,
                const std::string &_name)
{
  // Get the first world...we assume it the only one running
  physics::WorldPtr world = physics::get_world();
  world->LoadPlugin(_filename, _name, sdf::ElementPtr());
}

/////////////////////////////////////////////////
physics::ModelPtr ServerFixture::GetModel(const std::string &_name)
{
  // Get the first world...we assume it the only one running
  physics::WorldPtr world = physics::get_world();
  return world->GetModel(_name);
}

/////////////////////////////////////////////////
void ServerFixture::RemoveModel(const std::string &_name)
{
  msgs::Request *msg = msgs::CreateRequest("entity_delete", _name);
  this->requestPub->Publish(*msg);
  delete msg;
}

/////////////////////////////////////////////////
void ServerFixture::RemovePlugin(const std::string &_name)
{
  // Get the first world...we assume it the only one running
  physics::WorldPtr world = physics::get_world();
  world->RemovePlugin(_name);
}

/////////////////////////////////////////////////
void ServerFixture::GetMemInfo(double &_resident, double &_share)
{
#ifdef __linux__
  int totalSize, residentPages, sharePages;
  totalSize = residentPages = sharePages = 0;

  std::ifstream buffer("/proc/self/statm");
  buffer >> totalSize >> residentPages >> sharePages;
  buffer.close();

  // in case x86-64 is configured to use 2MB pages
  int64_t pageSizeKb = sysconf(_SC_PAGE_SIZE) / 1024;

  _resident = residentPages * pageSizeKb;
  _share = sharePages * pageSizeKb;
#elif __MACH__
  // /proc is only available on Linux
  // for OSX, use task_info to get resident and virtual memory
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(),
                                TASK_BASIC_INFO,
                                (task_info_t)&t_info,
                                &t_info_count))
  {
    gzerr << "failure calling task_info\n";
    return;
  }
  _resident = static_cast<double>(t_info.resident_size/1024);
  _share = static_cast<double>(t_info.virtual_size/1024);
#else
  gzerr << "Unsupported architecture\n";
  return;
#endif
}
