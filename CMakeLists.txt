cmake_minimum_required (VERSION 3.8)
project (vulkan_minimal_graphics)

find_package(Vulkan)

# get rid of annoying MSVC warnings.
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set (CMAKE_CXX_STANDARD 17)

include_directories(${Vulkan_INCLUDE_DIR})

set(ALL_LIBS  ${Vulkan_LIBRARY} )

if(WIN32)
  link_directories(${ADDITIONAL_LIBRARY_DIRS})
else()
  find_package(glfw3 REQUIRED)
  include_directories(${GLFW_INCLUDE_DIRS})
  set(ALL_LIBS ${ALL_LIBS} ${GLFW_LIBRARIES} )
endif()

#uncomment this to detect broken memory problems via gcc sanitizers
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer -fsanitize=leak -fsanitize=undefined -fsanitize=bounds-strict")

add_executable(vulkan_minimal_graphics src/main.cpp 
                                       src/vk_utils.h src/vk_utils.cpp 
                                       src/vk_geom.h src/vk_geom.cpp
                                       src/vk_copy.h src/vk_copy.cpp 
                                       src/cmesh.h src/cmesh.cpp
                                       src/cmesh_vsgf.h src/cmesh_vsgf.cpp 
                                       src/vk_texture.h src/vk_texture.cpp
                                       src/vk_quad.h src/vk_quad.cpp
                                       src/vk_program.h src/vk_program.cpp 
                                       src/vk_graphics_pipeline.h src/vk_graphics_pipeline.cpp
                                       src/Bitmap.h src/Bitmap.cpp)

set_target_properties(vulkan_minimal_graphics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_link_libraries(vulkan_minimal_graphics ${ALL_LIBS} ${GLFW_LIBRARIES} glfw)
