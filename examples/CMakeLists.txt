# Desktop builds of the example sketches (see the root CMakeLists.txt).
# STM32DcmiCapture is not listed: it drives an STM32 board's DCMI
# peripheral and onboard LCD, so it only makes sense on that hardware.
add_subdirectory(SimpleCapture)
add_subdirectory(ConvertToBmp)
add_subdirectory(ScaledCapture)
add_subdirectory(CaptureBenchmark)
add_subdirectory(DesktopCapture)
if(TARGET TinyJPEG)
    add_subdirectory(ConvertSoftware)
endif()
