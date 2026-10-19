// Proves the build, GoogleTest and CI wiring work (issue #2). Remove once real tests exist.
#include <gtest/gtest.h>

TEST(Smoke, BuildAndTestHarnessWork) { EXPECT_EQ(__cplusplus >= 201703L, true); }
