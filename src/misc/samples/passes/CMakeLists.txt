add_library(common INTERFACE)
target_sources(common INTERFACE
	
)	
target_include_directories(common INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_executable(sinewave
	sinewave.cpp
	deviceprog.cu
)

target_include_directories(sinewave PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sinewave krr_lib common)

add_executable(triangle
	triangle.cpp
)
target_include_directories(triangle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(triangle krr_lib common)

add_executable(box
	box.cpp
)
target_include_directories(box PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(box krr_lib common)

add_executable(frame
	framebuffer.cpp
	deviceprog.cu
)
target_include_directories(frame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(frame krr_lib common)