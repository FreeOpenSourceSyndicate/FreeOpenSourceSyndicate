# Source files
set(FOSS_SOURCES
    main.cpp
)

# Create executable
add_executable(FreeOpenSourceSyndicate ${FOSS_SOURCES})

# Cocos2d-x configuration
# NOTE: Cocos2d-x will be fetched/integrated in the next phase
# For now, placeholder CMake configuration

# Include directories
target_include_directories(FreeOpenSourceSyndicate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Output
set_target_properties(FreeOpenSourceSyndicate PROPERTIES
    VERSION 0.1.0
)
