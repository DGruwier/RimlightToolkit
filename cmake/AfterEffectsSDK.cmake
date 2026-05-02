function(rtk_find_aftereffects_sdk)
  if(NOT AE_SDK_ROOT)
    message(FATAL_ERROR "Set AE_SDK_ROOT to the Adobe After Effects SDK root.")
  endif()

  find_path(AE_SDK_INCLUDE_DIR
    NAMES AE_Effect.h
    PATHS
      "${AE_SDK_ROOT}/Examples/Headers"
      "${AE_SDK_ROOT}/Headers"
      "${AE_SDK_ROOT}/SDKExamplesHeaders"
    NO_DEFAULT_PATH)

  if(NOT AE_SDK_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find AE_Effect.h under AE_SDK_ROOT=${AE_SDK_ROOT}.")
  endif()

  set(AE_SDK_INCLUDE_DIR "${AE_SDK_INCLUDE_DIR}" PARENT_SCOPE)
endfunction()
