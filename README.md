# Source files
set(FOSS_SOURCES
    main.cpp
    game/AppDelegate.cpp
    game/GameManager.cpp
    game/GameScene.cpp
)

set(COCOS2D_X_ROOT "${COCOS2D_X_ROOT}" CACHE PATH "Path to the Cocos2d-x source tree")
if(NOT COCOS2D_X_ROOT)
    set(COCOS2D_X_ROOT "${CMAKE_SOURCE_DIR}/third_party/cocos2d-x")
endif()

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
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${COCOS2D_X_ROOT}"
        "${COCOS2D_X_ROOT}/cocos"
        "${COCOS2D_X_ROOT}/external"
        "${COCOS2D_X_ROOT}/extensions"
    )
endif()

# Output
set_target_properties(FreeOpenSourceSyndicate PROPERTIES
    VERSION 0.1.0
)
