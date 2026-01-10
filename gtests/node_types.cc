#include <gtest/gtest.h>

#include "coral.h"

TEST(ConnectionType, ConstDouble)
{
  EXPECT_EQ(coral::connection_type<const double>(),
            coral::ConnectionType::input);
}

TEST(ConnectionType, Double)
{
  EXPECT_EQ(coral::connection_type<double>(), coral::ConnectionType::input);
}

TEST(ConnectionType, ConstDoubleRef)
{
  EXPECT_EQ(coral::connection_type<const double &>(),
            coral::ConnectionType::input);
}

TEST(ConnectionType, DoubleRef)
{
  EXPECT_EQ(coral::connection_type<double &>(),
            coral::ConnectionType::pass_through);
}
