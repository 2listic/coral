#include <iostream>

#include "coral.h"

namespace coral
{
  std::map<std::string, NodeObjectInitializer> NodeObject::initializers;

  tf::Executor NodeObject::executor;
  tf::Taskflow NodeObject::taskflow;
} // namespace coral