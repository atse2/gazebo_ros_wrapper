include (${gazebo_cmake_dir}/GazeboUtils.cmake)

if (HAVE_OPENAL)
  include_directories(${OPENAL_INCLUDE_DIR})
endif()


if (HAVE_GTS)
  include_directories(${gts_INCLUDE_DIRS})
  link_directories(${gts_LIBRARY_DIRS})
  add_definitions(${gts_CFLAGS})
endif()

set (sources
  Animation.cc
  Assert.cc
  AudioDecoder.cc
  Base64.cc
  BVHLoader.cc
  ColladaLoader.cc
  Color.cc
  CommonIface.cc
  Console.cc
  Event.cc
  Events.cc
  Exception.cc
  Image.cc
  KeyFrame.cc
  Material.cc
  Mesh.cc
  MeshLoader.cc
  MeshManager.cc
  ModelDatabase.cc
  PID.cc
  SkeletonAnimation.cc
  Skeleton.cc
  SphericalCoordinates.cc
  STLLoader.cc
  SystemPaths.cc
  Time.cc
  Timer.cc
  Video.cc
)

set (headers
  Animation.hh
  Assert.hh
  AudioDecoder.hh
  Base64.hh
  BVHLoader.hh
  ColladaLoader.hh
  CommonIface.hh
  CommonTypes.hh
  Color.hh
  Console.hh
  Event.hh
  Events.hh
  Exception.hh
  Image.hh
  KeyEvent.hh
  KeyFrame.hh
  Material.hh
  Mesh.hh
  MeshLoader.hh
  MeshManager.hh
  ModelDatabase.hh
  MouseEvent.hh
  PID.hh
  Plugin.hh
  SkeletonAnimation.hh
  Skeleton.hh
  SingletonT.hh
  SphericalCoordinates.hh
  STLLoader.hh
  SystemPaths.hh
  Time.hh
  Timer.hh
  UpdateInfo.hh
  Video.hh
 )

if (HAVE_GTS)
set (sources
  ${sources}
  MeshCSG.cc
)
set (headers
  ${headers}
  MeshCSG.hh
)
endif()

set (gtest_sources
  Animation_TEST.cc
  ColladaLoader_TEST.cc
  Color_TEST.cc
  CommonIface_TEST.cc
  Console_TEST.cc
  Exception_TEST.cc
  Material_TEST.cc
  Mesh_TEST.cc
  Image_TEST.cc
  SphericalCoordinates_TEST.cc
  SystemPaths_TEST.cc
  Time_TEST.cc
)

if (HAVE_FFMPEG)
  set (gtest_sources ${gtest_sources}
                     AudioDecoder_TEST.cc)
endif()

gz_build_tests(${gtest_sources})

set (common_headers "" CACHE INTERNAL "common headers" FORCE)
foreach (hdr ${headers})
  APPEND_TO_CACHED_STRING(common_headers
    "Common Headers" "#include \"gazebo/common/${hdr}\"\n")
endforeach()
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/common.hh.in ${CMAKE_CURRENT_BINARY_DIR}/common.hh )

gz_add_library(gazebo_common ${sources})

set_property(
  SOURCE SystemPaths.cc SystemPaths_TEST.cc ModelDatabase.cc
  PROPERTY COMPILE_DEFINITIONS
  GAZEBO_PLUGIN_PATH="${GAZEBO_PLUGIN_PATH}"
  GAZEBO_RESOURCE_PATH="${GAZEBO_RESOURCE_PATH}"
  GAZEBO_MODEL_DATABASE_URI="${GAZEBO_MODEL_DATABASE_URI}"
  OGRE_RESOURCE_PATH="${OGRE_RESOURCE_PATH}"
)

target_link_libraries(gazebo_common
  gazebo_math
  ${libdl_library}
  ${libtool_library}
  ${Boost_LIBRARIES}
  ${freeimage_LIBRARIES}
  ${tinyxml_LIBRARIES}
  ${libavcodec_LIBRARIES}
  ${libavformat_LIBRARIES}
  ${CURL_LIBRARIES}
  ${libswscale_LIBRARIES}
  ${libtar_LIBRARIES}
  ${TBB_LIBRARIES}
  ${SDF_LIBRARIES}
  pthread
)

if (NOT APPLE)
  # rt is used for clock_gettime, which is not available on apple
  target_link_libraries(gazebo_common rt)
endif()

if (HAVE_GTS)
  target_link_libraries(gazebo_common ${gts_LIBRARIES})
endif()

if (HAVE_OPENAL)
  target_link_libraries(gazebo_common ${OPENAL_LIBRARY})
endif()

gz_install_library(gazebo_common)
gz_install_includes("common" ${headers} ${CMAKE_CURRENT_BINARY_DIR}/common.hh)
