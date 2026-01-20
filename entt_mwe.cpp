// Minimal Working Example: Using entt as Single Source of Truth
//
// Demonstrates how CORAL can use entt's meta system to register types,
// constructors, methods, and base classes WITHOUT parallel data structures.
//
// Compile: g++ -std=c++17 entt_mwe.cpp -I./entt/src -o entt_mwe && ./entt_mwe

#include <entt/entt.hpp>
#include <functional>
#include <iostream>
#include <memory>

// =============================================================================
// Example Types (same as CORAL needs)
// =============================================================================

struct Point {
  double x = 0.0;
  double y = 0.0;
  Point() { std::cout << "  Point() constructed\n"; }
  Point(double x_, double y_) : x(x_), y(y_) {
    std::cout << "  Point(" << x_ << ", " << y_ << ") constructed\n";
  }
};

struct Calculator {
  int value = 0;
  void set_value(int v) { value = v; std::cout << "  set_value(" << v << ")\n"; }
  int add(int x) { value += x; return value; }
  int get_value() const { return value; }
};

struct Shape {
  virtual ~Shape() = default;
  virtual double area() const = 0;
};

struct Rectangle : public Shape {
  double width = 1.0;
  double height = 1.0;
  Rectangle() = default;
  double area() const override { return width * height; }
};

// =============================================================================
// Lazy Evaluation (CORAL's core need)
// =============================================================================

// Simple lazy wrapper - executor would be stored in entt properties in CORAL
template<typename T>
struct LazyNode {
  std::shared_ptr<T> value;
  std::function<void()> executor;
  bool is_ready = false;

  void operator()() {
    if (!is_ready && executor) {
      std::cout << "  [LAZY] Executing constructor\n";
      executor();
      is_ready = true;
    }
  }

  T& get() {
    if (!is_ready) (*this)();
    return *value;
  }
};

// =============================================================================
// Registration (exactly like CORAL does)
// =============================================================================

void register_all_types() {
  std::cout << "=== Registering Types with entt ===\n\n";

  // 1. Elementary types (int, double, etc.)
  std::cout << "1. Elementary types:\n";
  entt::meta_factory<int>{}
    .type(entt::hashed_string{"int"}.value(), "int");
  std::cout << "  ✓ int\n";

  entt::meta_factory<double>{}
    .type(entt::hashed_string{"double"}.value(), "double");
  std::cout << "  ✓ double\n";

  // 2. Regular type with default constructor
  std::cout << "\n2. Regular type:\n";
  entt::meta_factory<Point>{}
    .type(entt::hashed_string{"Point"}.value(), "Point");
  std::cout << "  ✓ Point\n";

  // 3. Constructor with arguments
  std::cout << "\n3. Constructor with arguments:\n";
  entt::meta_factory<Point>{}
    .ctor<double, double>();  // Register Point(double, double)
  std::cout << "  ✓ Point(double, double)\n";
  std::cout << "  (In CORAL: store arg names {\"x\", \"y\"} as entt property)\n";

  // 4. Abstract type
  std::cout << "\n4. Abstract type:\n";
  entt::meta_factory<Shape>{}
    .type(entt::hashed_string{"Shape"}.value(), "Shape");
  std::cout << "  ✓ Shape (abstract base)\n";

  // 5. Derived type with base class relationship
  std::cout << "\n5. Derived type with inheritance:\n";
  entt::meta_factory<Rectangle>{}
    .type(entt::hashed_string{"Rectangle"}.value(), "Rectangle")
    .base<Shape>();  // Register inheritance - entt tracks this!
  std::cout << "  ✓ Rectangle : Shape\n";

  // 6. Class with methods
  std::cout << "\n6. Class with methods:\n";
  entt::meta_factory<Calculator>{}
    .type(entt::hashed_string{"Calculator"}.value(), "Calculator")
    .func<&Calculator::set_value>("set_value")
    .func<&Calculator::add>("add")
    .func<&Calculator::get_value>("get_value");
  std::cout << "  ✓ Calculator with 3 methods\n";
}

// =============================================================================
// Introspection (query what's registered)
// =============================================================================

void show_registered_types() {
  std::cout << "\n=== Querying Registered Types ===\n";

  for (auto [id, type] : entt::resolve()) {
    std::cout << "\n" << type.name() << ":";

    // Show base classes (entt tracks inheritance!)
    bool has_base = false;
    for (auto [base_id, base_type] : type.base()) {
      if (!has_base) std::cout << "\n  base: ";
      std::cout << base_type.name() << " ";
      has_base = true;
    }

    // Show methods (entt stores these!)
    bool has_func = false;
    for (auto [func_id, func] : type.func()) {
      if (!has_func) std::cout << "\n  methods: ";
      std::cout << func.name() << " ";
      has_func = true;
    }
    std::cout << "\n";
  }
}

// =============================================================================
// Main
// =============================================================================

int main() {
  std::cout << "=== entt MWE: Single Source of Truth ===\n\n";
  std::cout << "This demonstrates how CORAL can use entt's meta system\n";
  std::cout << "to store ALL type information WITHOUT parallel structures.\n\n";

  // Register everything
  register_all_types();

  // Show what's stored in entt
  show_registered_types();

  // -------------------------------------------------------------------------
  // Demonstrate lazy evaluation
  // -------------------------------------------------------------------------
  std::cout << "\n=== Lazy Evaluation Demo ===\n";

  std::cout << "\nStep 1: Create lazy Point (not constructed yet)\n";
  LazyNode<Point> lazy_point;
  lazy_point.executor = [&lazy_point]() {
    lazy_point.value = std::make_shared<Point>();
  };
  std::cout << "  is_ready = " << lazy_point.is_ready << "\n";

  std::cout << "\nStep 2: Access Point (triggers lazy construction)\n";
  auto& pt = lazy_point.get();
  std::cout << "  Point value: (" << pt.x << ", " << pt.y << ")\n";
  std::cout << "  is_ready = " << lazy_point.is_ready << "\n";

  std::cout << "\nStep 3: Access again (uses cached value)\n";
  auto& pt2 = lazy_point.get();
  std::cout << "  Point value: (" << pt2.x << ", " << pt2.y << ")\n";
  std::cout << "  (No construction this time!)\n";

  // -------------------------------------------------------------------------
  // Demonstrate type lookup
  // -------------------------------------------------------------------------
  std::cout << "\n=== Type Lookup Demo ===\n";

  std::cout << "\nLooking up 'Rectangle' type:\n";
  auto rect_type = entt::resolve<Rectangle>();
  std::cout << "  Name: " << rect_type.name() << "\n";

  std::cout << "  Base classes: ";
  int base_count = 0;
  for (auto [base_id, base_type] : rect_type.base()) {
    std::cout << base_type.name() << " ";
    base_count++;
  }
  std::cout << "(" << base_count << " found)\n";

  // -------------------------------------------------------------------------
  // Demonstrate polymorphism
  // -------------------------------------------------------------------------
  std::cout << "\n=== Polymorphism Demo ===\n";

  Rectangle rect;
  rect.width = 4.0;
  rect.height = 5.0;

  std::cout << "Rectangle: " << rect.width << " x " << rect.height << "\n";
  std::cout << "Area (direct call): " << rect.area() << "\n";

  Shape* shape_ptr = &rect;
  std::cout << "Area (via Shape* base): " << shape_ptr->area() << "\n";
  std::cout << "(Base class relationship registered with entt!)\n";

  // -------------------------------------------------------------------------
  // Key Insights
  // -------------------------------------------------------------------------
  std::cout << "\n=== Key Insights for CORAL ===\n\n";

  std::cout << "✓ entt::meta_factory stores:\n";
  std::cout << "  - Type names and IDs\n";
  std::cout << "  - Constructors (with argument types)\n";
  std::cout << "  - Methods/functions\n";
  std::cout << "  - Base class relationships\n";
  std::cout << "  - Custom properties (via .data() / .prop())\n\n";

  std::cout << "✓ Can query everything:\n";
  std::cout << "  - entt::resolve() iterates all types\n";
  std::cout << "  - type.base() gets base classes\n";
  std::cout << "  - type.func() gets methods\n";
  std::cout << "  - type.construct(...) invokes constructors\n\n";

  std::cout << "✓ For CORAL implementation:\n";
  std::cout << "  1. Register types with entt::meta_factory (as shown above)\n";
  std::cout << "  2. Store executors as entt properties\n";
  std::cout << "  3. Store arg names as entt properties\n";
  std::cout << "  4. Generate JSON by iterating entt::resolve()\n";
  std::cout << "  5. Remove parallel 'initializers' map entirely\n\n";

  std::cout << "Result: ~500 lines removed, single source of truth!\n";

  std::cout << "\n=== Done ===\n";
  return 0;
}
