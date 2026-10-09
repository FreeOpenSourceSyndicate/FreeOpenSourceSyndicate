# Source files
set(FOSS_SOURCES
    main.cpp
)

# Cocos2d-x root can be provided by external build configuration.
# This project is structured to support integrating the engine in the next step.
set(COCOS2D_X_ROOT "${COCOS2D_X_ROOT}" CACHE PATH "Path to the Cocos2d-x source tree")

if(NOT EXISTS "${COCOS2D_X_ROOT}/CMakeLists.txt")
    message(WARNING
        "Cocos2d-x root not found at '${COCOS2D_X_ROOT}'.\n"
        "Run ./scripts/fetch_cocos2dx.sh before building."
    )
endif()

# Create executable
add_executable(FreeOpenSourceSyndicate ${FOSS_SOURCES})

# Add engine include dirs if available
if(EXISTS "${COCOS2D_X_ROOT}/CMakeLists.txt")
    target_include_directories(FreeOpenSourceSyndicate PRIVATE
        "${COCOS2D_X_ROOT}"
        "${COCOS2D_X_ROOT}/cocos"
        "${COCOS2D_X_ROOT}/external"
    )
endif()

# Output
set_target_properties(FreeOpenSourceSyndicate PROPERTIES
    VERSION 0.1.0
)
