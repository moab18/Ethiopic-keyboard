# CMake generated Testfile for 
# Source directory: /home/moab/sources/ethiopic-keyboard/tests
# Build directory: /home/moab/sources/ethiopic-keyboard/build-ibus-test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_mapping "/home/moab/sources/ethiopic-keyboard/build-ibus-test/test_mapping")
set_tests_properties(test_mapping PROPERTIES  WORKING_DIRECTORY "/home/moab/sources/ethiopic-keyboard/tests/../.." _BACKTRACE_TRIPLES "/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;60;add_test;/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;0;")
add_test(test_engine "/home/moab/sources/ethiopic-keyboard/build-ibus-test/test_engine")
set_tests_properties(test_engine PROPERTIES  WORKING_DIRECTORY "/home/moab/sources/ethiopic-keyboard/tests/../.." _BACKTRACE_TRIPLES "/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;60;add_test;/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;0;")
add_test(test_features "/home/moab/sources/ethiopic-keyboard/build-ibus-test/test_features")
set_tests_properties(test_features PROPERTIES  WORKING_DIRECTORY "/home/moab/sources/ethiopic-keyboard/tests/../.." _BACKTRACE_TRIPLES "/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;60;add_test;/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;0;")
add_test(test_ibus_engine "/home/moab/sources/ethiopic-keyboard/build-ibus-test/test_ibus_engine")
set_tests_properties(test_ibus_engine PROPERTIES  WORKING_DIRECTORY "/home/moab/sources/ethiopic-keyboard/tests/../.." _BACKTRACE_TRIPLES "/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;60;add_test;/home/moab/sources/ethiopic-keyboard/tests/CMakeLists.txt;0;")
subdirs("libethio")
