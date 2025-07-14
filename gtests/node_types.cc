#include <gtest/gtest.h>

#include "coral.h"

TEST(ConnectionTypeTest, ConstDouble)
{
  EXPECT_EQ(coral::connection_type<const double>(),
            coral::ConnectionType::input);
}

TEST(ConnectionTypeTest, Double)
{
  EXPECT_EQ(coral::connection_type<double>(), coral::ConnectionType::input);
}

TEST(ConnectionTypeTest, ConstDoubleRef)
{
  EXPECT_EQ(coral::connection_type<const double &>(),
            coral::ConnectionType::input);
}

TEST(ConnectionTypeTest, DoubleRef)
{
  EXPECT_EQ(coral::connection_type<double &>(),
            coral::ConnectionType::pass_through);
}
