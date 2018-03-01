# This script is read by ctest_start before project is configured
# and by CTest module to prepare DartConfiguration.tcl
set (CTEST_PROJECT_NAME "cdns")
set (CTEST_NIGHTLY_START_TIME "01:02:03 UTC")

# set (CTEST_DROP_METHOD "") # https
set (CTEST_DROP_SITE "") # my.cdash.org
# set (CTEST_DROP_LOCATION "/submit.php?project=${CTEST_PROJECT_NAME}")
# set (CTEST_DROP_SITE_CDASH TRUE)
# set (CTEST_TEST_TIMEOUT 300) # seconds
