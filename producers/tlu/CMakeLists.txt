FIND_PACKAGE(ZestSC1 REQUIRED)
find_package(LIBUSB REQUIRED)
find_package(TLUFirmware REQUIRED)

add_definitions(${TLUFIRMWARE_DEFINITIONS})

INCLUDE_DIRECTORIES( include ${ZESTSC1_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})

add_executable(TLUControl.exe src/TLUControl.cxx src/TLUController.cc src/TLU_USB.cc src/USBTracer.cc src/TLUAddresses1.cc src/TLUAddresses2.cc src/win_uSleep.cc)
add_executable(TLUProducer.exe src/TLUProducer.cxx src/TLUController.cc src/TLU_USB.cc src/USBTracer.cc src/TLUAddresses1.cc src/TLUAddresses2.cc src/win_uSleep.cc)
add_executable(TLUReset.exe src/TLUReset.cxx src/TLUController.cc src/TLU_USB.cc src/USBTracer.cc src/TLUAddresses1.cc src/TLUAddresses2.cc src/win_uSleep.cc)

target_link_libraries(TLUControl.exe   EUDAQ ${LIBUSB_LIBRARIES} ${ZESTSC1_LIBRARIES})
target_link_libraries(TLUProducer.exe   EUDAQ ${LIBUSB_LIBRARIES} ${ZESTSC1_LIBRARIES})
target_link_libraries(TLUReset.exe   EUDAQ ${LIBUSB_LIBRARIES} ${ZESTSC1_LIBRARIES})

INSTALL(TARGETS TLUControl.exe TLUProducer.exe TLUReset.exe
  RUNTIME DESTINATION bin COMPONENT TLU
  LIBRARY DESTINATION lib COMPONENT TLU
  ARCHIVE DESTINATION lib COMPONENT TLU)
  
INSTALL(DIRECTORY ${TLUFIRMWARE_PATH} DESTINATION "extern" COMPONENT TLU)
