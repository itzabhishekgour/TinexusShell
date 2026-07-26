# Getting Started with Tinexus Developer SDK

## Building an Application with Tinexus SDK

### 1. CMake Integration
Add `tinexus-sdk` to your `CMakeLists.txt`:

```cmake
find_package(TinexusPlatform REQUIRED)

add_executable(my_tinexus_app main.cpp)
target_link_libraries(my_tinexus_app PRIVATE tinexus-sdk)
```

### 2. C++ Application Code

```cpp
#include <tinexus/client.hpp>
#include <iostream>

int main() {
    tinexus::Client client;
    if (client.connect().is_ok()) {
        auto search_res = client.search().query("firefox");
        if (search_res.is_ok()) {
            for (const auto& item : search_res.value()) {
                std::cout << "Match: " << item.title() << "\n";
            }
        }
    }
    return 0;
}
```
