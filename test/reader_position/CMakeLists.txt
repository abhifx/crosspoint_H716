add_executable(reader_position_test
  ReaderPositionTest.cpp
)

target_link_libraries(reader_position_test PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(reader_position_test)
