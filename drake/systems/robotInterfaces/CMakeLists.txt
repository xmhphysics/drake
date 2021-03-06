add_library_with_exports(LIB_NAME drakeSide SOURCE_FILES Side.cpp)
drake_install_headers(Side.h)
pods_install_libraries(drakeSide)
pods_install_pkg_config_file(drake-side
  LIBS -ldrakeSide
  CFLAGS
  REQUIRES
  VERSION 0.0.1)

if(lcm_FOUND)
  add_library_with_exports(LIB_NAME drakeQPLocomotionPlan SOURCE_FILES QPLocomotionPlan.cpp BodyMotionData.cpp) # TODO: move BodyMotionData to a better place
  target_link_libraries(drakeQPLocomotionPlan drakeSplineGeneration drakeRBM drakeLCMUtil drakeSide drakeConvexHull drakeAtlasUtil drakeZMPUtil lcm)
  pods_install_libraries(drakeQPLocomotionPlan)
  drake_install_headers(QPLocomotionPlan.h BodyMotionData.h)
  pods_install_pkg_config_file(drake-qp-locomotion-plan
    LIBS -ldrakeQPLocomotionPlan
    CFLAGS -I\${prefix}/include/lcmtypes
    REQUIRES lcm
    VERSION 0.0.1)
endif()

# ==== Below this line is MATLAB-specific code ====

if(Matlab_FOUND)
  drake_add_mex(footstepCollocationConstraintsMex footstepCollocationConstraintsMex.cpp)

  drake_find_package(octomap CONFIG)

  if(octomap_FOUND)
    drake_add_mex(octomapWrapper octomapWrapper.cpp $<TARGET_OBJECTS:drakeUtil>)
    target_link_libraries(octomapWrapper drakeMexUtil octomap)
  endif()

  if(lcm_FOUND AND NOT WIN32)
    drake_add_mex(constructQPLocomotionPlanmex constructQPLocomotionPlanmex.cpp $<TARGET_OBJECTS:drakeUtil>)
    target_link_libraries(constructQPLocomotionPlanmex drakeQPLocomotionPlan drakeZMPUtil drakeMexUtil)

    add_subdirectory(@QPLocomotionPlanCPPWrapper)
  endif()
endif()

add_subdirectory(calibration/test)
add_subdirectory(test)

# ==== Below this line is C++ and MATLAB shared code ====

