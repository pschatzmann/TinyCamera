# Desktop test suite: exercises the library against the deterministic
# test-pattern source (no webcam needed). Run with: ctest --test-dir build
add_executable(test_desktop test_desktop.cpp)
target_link_libraries(test_desktop PRIVATE TinyCamera)
add_test(NAME test_desktop COMMAND test_desktop)

if(TARGET TinyJPEG)
    add_executable(test_desktop_jpeg test_desktop_jpeg.cpp)
    target_link_libraries(test_desktop_jpeg PRIVATE TinyCamera)
    add_test(NAME test_desktop_jpeg COMMAND test_desktop_jpeg)
endif()
