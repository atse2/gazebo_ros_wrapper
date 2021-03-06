include_directories (
  ${ODE_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
  ${OGRE_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIR}
)

link_directories(
  ${ogre_library_dirs}
  ${Boost_LIBRARY_DIRS}
  ${ODE_LIBRARY_DIRS}
)

# Disable the player test for now, until we figure out a better way to test them
#if (INCLUDE_PLAYER)
#  add_subdirectory(player)
#endif()

if(HAVE_BULLET)
  include_directories ( ${BULLET_INCLUDE_DIRS} )
  link_directories ( ${BULLET_LIBRARY_DIRS} )
endif()
if(HAVE_DART)
  include_directories ( ${DART_INCLUDE_DIRS} )
  link_directories ( ${DART_LIBRARY_DIRS} )
endif()

set(tests
  bandwidth.cc
  contact_sensor.cc
  factory.cc
  file_handling.cc
  gripper.cc
  imu.cc
  laser.cc
  nondefault_world.cc
  physics.cc
  physics_friction.cc
  physics_msgs.cc
  physics_thread_safe.cc
  pioneer2dx.cc
  transport.cc
  server_fixture.cc
  speed.cc
  stress_spawn_models.cc
  #state_log.cc
  surface_properties.cc
  transceiver.cc
  world.cc
  )

if (MANPAGES_SUPPORT)
  set (tests ${tests}
	      manpages.cc)
endif()

gz_build_tests(${tests})

if (ENABLE_TESTS_COMPILATION)
  # Increase timeout, to account for model download time.
  set_tests_properties(${TEST_TYPE}_factory PROPERTIES TIMEOUT 500)
  set_tests_properties(${TEST_TYPE}_physics PROPERTIES TIMEOUT 500)
endif()

set(display_tests
)

# Build the display tests (need extra sources to compile)
gz_build_display_tests(${display_tests})

set(dri_tests
  camera_sensor.cc
  multicamera_sensor.cc
  gpu_laser.cc
  heightmap.cc
  ogre_log.cc
  pr2.cc
  projector.cc
  rendering_sensor.cc
  speed_pr2.cc
)

gz_build_dri_tests(${dri_tests})


if (ENABLE_TESTS_COMPILATION AND VALID_DRI_DISPLAY)
  # Increase timeout, to account for model download time.
  set_tests_properties(${TEST_TYPE}_pr2 PROPERTIES TIMEOUT 500)

  # Increase timeout, to account for model download time.
  set_tests_properties(${TEST_TYPE}_pioneer2dx PROPERTIES TIMEOUT 500)
endif()
