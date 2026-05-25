add_executable(beat_sync       beat_sync/main.cpp)
add_executable(cdj_monitor     cdj_status_monitor/main.cpp)
add_executable(virtual_cdj_ex  virtual_cdj/main.cpp)

target_link_libraries(beat_sync      PRIVATE prodjlink)
target_link_libraries(cdj_monitor    PRIVATE prodjlink)
target_link_libraries(virtual_cdj_ex PRIVATE prodjlink)
