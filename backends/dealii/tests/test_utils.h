#ifndef GTESTS_TEST_UTILS_H
#define GTESTS_TEST_UTILS_H

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace coral_test
{
  /**
   * @brief RAII class for managing test output directories.
   *
   * This class creates a dedicated output directory for each test and
   * automatically cleans it up based on test success/failure.
   *
   * Creation policy:
   * - If directory exists: clear all files inside it
   * - If directory doesn't exist: create it
   *
   * Destruction policy (can be controlled via environment variables):
   * - Test passed: remove the directory (unless CORAL_KEEP_SUCCEEDED_OUTPUT is
   * set)
   * - Test failed: keep the directory (unless CORAL_DELETE_FAILED_OUTPUT is
   * set)
   * - If the test directory is removed and the base directory becomes empty,
   * the base directory is also removed
   *
   * Environment variables:
   * - CORAL_KEEP_SUCCEEDED_OUTPUT: If set, keep output directories for passed
   * tests
   * - CORAL_DELETE_FAILED_OUTPUT: If set, delete output directories for failed
   * tests
   *
   * Usage:
   * @code
   * TEST(MyTest, Example) {
   *   ScopedTestOutputDir output_dir("MyTest_Example");
   *   // Use output_dir.path() to get the directory path
   *   network.set_touch_file_base_path(output_dir.path());
   *   // ...
   *   // Directory is automatically cleaned up if test passes
   * }
   *
   * // Run with custom cleanup behavior:
   * // CORAL_KEEP_SUCCEEDED_OUTPUT=1 ./test_binary
   * // CORAL_DELETE_FAILED_OUTPUT=1 ./test_binary
   * @endcode
   */
  class ScopedTestOutputDir
  {
  public:
    /**
     * @brief Construct a test output directory.
     * @param test_name Name of the test (typically "TestSuite_TestName")
     * @param base_dir Base directory for test outputs (default:
     * "./test_output")
     */
    explicit ScopedTestOutputDir(
      const std::string           &test_name,
      const std::filesystem::path &base_dir = "./test_output")
      : base_dir_(base_dir)
      , dir_path_(base_dir / test_name)
    {
      // Creation policy: if exists, clear contents; if not, create
      if (std::filesystem::exists(dir_path_))
        {
          // Remove all files inside the directory
          for (const auto &entry :
               std::filesystem::directory_iterator(dir_path_))
            {
              std::filesystem::remove_all(entry.path());
            }
        }
      else
        {
          std::filesystem::create_directories(dir_path_);
        }
    }

    /**
     * @brief Destructor - removes directory based on test result and
     * environment variables.
     */
    ~ScopedTestOutputDir()
    {
      // Check environment variables for custom behavior
      bool keep_succeeded =
        std::getenv("CORAL_KEEP_SUCCEEDED_OUTPUT") != nullptr;
      bool delete_failed = std::getenv("CORAL_DELETE_FAILED_OUTPUT") != nullptr;

      // Get test result
      const testing::TestInfo *test_info =
        testing::UnitTest::GetInstance()->current_test_info();

      if (test_info)
        {
          bool test_failed   = test_info->result()->Failed();
          bool should_delete = false;

          if (test_failed)
            {
              // Test failed: delete only if CORAL_DELETE_FAILED_OUTPUT is set
              should_delete = delete_failed;
            }
          else
            {
              // Test passed: delete unless CORAL_KEEP_SUCCEEDED_OUTPUT is set
              should_delete = !keep_succeeded;
            }

          if (should_delete && std::filesystem::exists(dir_path_))
            {
              std::filesystem::remove_all(dir_path_);

              // Check if base_dir is now empty and remove it if so
              if (std::filesystem::exists(base_dir_) &&
                  std::filesystem::is_empty(base_dir_))
                {
                  std::filesystem::remove(base_dir_);
                }
            }
        }
    }

    // Delete copy constructor and assignment operator
    ScopedTestOutputDir(const ScopedTestOutputDir &) = delete;
    ScopedTestOutputDir &
    operator=(const ScopedTestOutputDir &) = delete;

    // Allow move operations
    ScopedTestOutputDir(ScopedTestOutputDir &&) = default;
    ScopedTestOutputDir &
    operator=(ScopedTestOutputDir &&) = default;

    /**
     * @brief Get the path to the test output directory.
     */
    const std::filesystem::path &
    path() const
    {
      return dir_path_;
    }

    /**
     * @brief Implicit conversion to filesystem::path for convenience.
     */
    operator const std::filesystem::path &() const
    {
      return dir_path_;
    }

  private:
    std::filesystem::path base_dir_;
    std::filesystem::path dir_path_;
  };

} // namespace coral_test

static void
mark_long_test()
{
  if (std::getenv("CORAL_SKIP_LONG_TEST") != nullptr)
    {
      std::cout << "Skip On" << std::endl;
      GTEST_SKIP() << "Long test and `CORAL_SKIP_LONG_TEST` enable. Skipping.";
    }
}

#endif // GTESTS_TEST_UTILS_H
