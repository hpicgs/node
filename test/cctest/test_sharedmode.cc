//#include "node_internals.h"
//#include "libplatform/libplatform.h"

#include <string>
#include "gtest/gtest.h"
//#include "node_test_fixture.h"
#include "node_lib.h"

// class SharedModeTest : public NodeTestFixture { };

TEST(SharedModeTest, MyFirstTest) {
  EXPECT_EQ(nullptr, node::internal::environment());
  EXPECT_EQ(nullptr, node::internal::isolate());
  node::Initialize();
  EXPECT_NE(nullptr, node::internal::environment());  
  EXPECT_NE(nullptr, node::internal::isolate());
}
