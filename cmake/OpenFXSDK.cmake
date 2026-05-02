include(FetchContent)

function(rtk_find_openfx_sdk)
  if(OPENFX_SDK_ROOT)
    find_path(OPENFX_INCLUDE_DIR
      NAMES ofxImageEffect.h
      PATHS
        "${OPENFX_SDK_ROOT}/include"
        "${OPENFX_SDK_ROOT}/include/ofx"
        "${OPENFX_SDK_ROOT}"
      NO_DEFAULT_PATH)
  endif()

  if(NOT OPENFX_INCLUDE_DIR AND RTK_FETCH_OPENFX)
    FetchContent_Declare(
      openfx
      GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
      GIT_TAG OFX_Release_1.5.1)
    FetchContent_GetProperties(openfx)
    if(NOT openfx_POPULATED)
      FetchContent_Populate(openfx)
    endif()
    find_path(OPENFX_INCLUDE_DIR
      NAMES ofxImageEffect.h
      PATHS
        "${openfx_SOURCE_DIR}/include"
        "${openfx_SOURCE_DIR}/include/ofx"
        "${openfx_SOURCE_DIR}"
      NO_DEFAULT_PATH)
  endif()

  if(NOT OPENFX_INCLUDE_DIR)
    message(FATAL_ERROR "Set OPENFX_SDK_ROOT or enable RTK_FETCH_OPENFX=ON.")
  endif()

  set(OPENFX_INCLUDE_DIR "${OPENFX_INCLUDE_DIR}" PARENT_SCOPE)
endfunction()
