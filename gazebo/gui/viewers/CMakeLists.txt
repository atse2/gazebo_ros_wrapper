include (${gazebo_cmake_dir}/GazeboUtils.cmake)
include ( ${QT_USE_FILE} )


set (sources
  ImageView.cc
  ImagesView.cc
  LaserView.cc
  TopicView.cc
  TextView.cc
  ViewFactory.cc
)

set (headers
  ViewFactory.hh
)

set (qt_headers
  ImageView.hh
  ImagesView.hh
  LaserView.hh
  TextView.hh
  TopicView.hh
)

set (qt_tests
  ImagesView_TEST.cc
)

# Generate executables for each of the QT unit tests
gz_build_qt_tests(${qt_tests})

set (resources ../resources.qrc)
QT4_WRAP_CPP(headers_MOC ${qt_headers})
QT4_ADD_RESOURCES(resources_RCC ${resources})

include_directories(${CMAKE_CURRENT_BINARY_DIR})

gz_add_library(gazebo_gui_viewers ${sources}
  ${headers_MOC} ${headers} ${resources_RCC})

target_link_libraries(gazebo_gui_viewers
  gazebo_common
  gazebo_transport
  gazebo_rendering
  gazebo_msgs)

gz_install_library(gazebo_gui_viewers)
