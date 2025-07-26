# Add sources
target_sources(${PROJECT_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_params.h
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_extractor.h
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_impl.h
               ${CMAKE_CURRENT_SOURCE_DIR}/HashSIFT.h
               ${CMAKE_CURRENT_SOURCE_DIR}/PatchSIFT.h
               ${CMAKE_CURRENT_SOURCE_DIR}/HashSiftWeights256.h
               ${CMAKE_CURRENT_SOURCE_DIR}/HashSiftWeights512.h
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_params.cc
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_extractor.cc
               ${CMAKE_CURRENT_SOURCE_DIR}/orb_impl.cc
               ${CMAKE_CURRENT_SOURCE_DIR}/HashSIFT.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/PatchSIFT.cpp)

# Install headers
file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
install(FILES ${HEADERS}
        DESTINATION ${STELLA_VSLAM_INCLUDE_INSTALL_DIR}/feature)
