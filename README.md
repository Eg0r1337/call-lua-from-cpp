# call-lua-from-cpp

A C++ template library for seamless integration between C++ and Lua, providing type-safe function calls and automatic type conversion between C++ and Lua types.

## ⚠️ Development Status

**This library is currently in development and is NOT production ready.** It contains several known issues including memory safety concerns, incomplete error handling, and API inconsistencies. Use at your own risk!!!

## Features

- **Type-safe Lua function calls** from C++
- **Automatic type conversion** between C++ and Lua types
- **Support for multiple return values** from Lua functions
- **Template-based API** with compile-time type checking
- **Support for complex types** including vectors, maps, and optional types
- **Custom string array types** with overflow detection


## Supported Types

### Basic Types

- `lua_Integer` / `lua_Number`
- `bool`
- `std::string`
- `std::nullopt_t`


### Container Types

- `std::vector<T>`
- `std::unordered_map<K, V>`
- `std::optional<T>`
- `std::array<char, N>` (fixed-size strings)


### Special Types

- `BasicLuaType` - Variant of all basic Lua types
- `LuaType` - Extended variant including containers
- `String<N>` - Fixed-size character arrays


## Installation

1. Include the header in your project:
```cpp
#include "lua_bindings.hpp"
```

2. Link against Lua libraries:
```cmake
find_package(Lua REQUIRED)
target_link_libraries(your_target ${LUA_LIBRARIES})
target_include_directories(your_target PRIVATE ${LUA_INCLUDE_DIR})
```


## Usage Examples

### Basic Function Calls

```cpp
#include <lua.hpp>
#include "lua_calls_from_cpp.hpp"

int main() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // Load a Lua script
    luaL_dostring(L, R"(
        function add(a, b)
            return a + b
        end
        
        function greet(name)
            return "Hello, " .. name .. "!"
        end
        
        function get_info()
            return "John", 25, true
        end
    )");
    
    // Call function with single return value
    int result = CallLuaFunction<int>(L, "add", 5, 3);
    std::cout << "5 + 3 = " << result << std::endl; // Output: 5 + 3 = 8
    
    // Call function returning string
    std::string greeting = CallLuaFunction<std::string>(L, "greet", "World");
    std::cout << greeting << std::endl; // Output: Hello, World!
    
    lua_close(L);
    return 0;
}
```


### Multiple Return Values

```cpp
// Function returning multiple values
auto [name, age, active] = CallLuaFunction<std::string, int, bool>(L, "get_info");
std::cout << "Name: " << name << ", Age: " << age << ", Active: " << active << std::endl;
```


### Working with Containers

```cpp
// Lua script with table operations
luaL_dostring(L, R"(
    function get_numbers()
        return {1, 2, 3, 4, 5}
    end
    
    function get_person()
        return {
            name = "Alice",
            age = 30,
            city = "New York"
        }
    end
    
    function sum_array(arr)
        local total = 0
        for i, v in ipairs(arr) do
            total = total + v
        end
        return total
    end
)");

// Get vector from Lua
std::vector<int> numbers = CallLuaFunction<std::vector<int>>(L, "get_numbers");
for (int num : numbers) {
    std::cout << num << " ";
}
std::cout << std::endl;

// Get map from Lua
std::unordered_map<std::string, BasicLuaType> person = 
    CallLuaFunction<std::unordered_map<std::string, BasicLuaType>>(L, "get_person");

// Pass vector to Lua
std::vector<int> input = {10, 20, 30, 40};
int sum = CallLuaFunction<int>(L, "sum_array", input);
std::cout << "Sum: " << sum << std::endl; // Output: Sum: 100
```


### Optional Types

```cpp
luaL_dostring(L, R"(
    function maybe_return(should_return)
        if should_return then
            return 42
        else
            return nil
        end
    end
)");

// Handle optional return values
std::optional<int> maybe_value = CallLuaFunction<std::optional<int>>(L, "maybe_return", true);
if (maybe_value) {
    std::cout << "Got value: " << *maybe_value << std::endl;
} else {
    std::cout << "Got nil" << std::endl;
}
```


### Void Functions

```cpp
luaL_dostring(L, R"(
    function print_message(msg)
        print("Lua says: " .. msg)
    end
)");

// Call function with no return value
CallLuaFunction<void>(L, "print_message", "Hello from C++!");
```


### Fixed-Size Strings

```cpp
luaL_dostring(L, R"(
    function get_short_string()
        return "Hi!"
    end
    
    function get_long_string()
        return "This is a very long string that might not fit"
    end
)");

// Using fixed-size string arrays
String<10> short_str = CallLuaFunction<String<10>>(L, "get_short_string");
std::cout << "Short: " << short_str.data() << std::endl;

// Using string with overflow detection
auto [long_str, success] = CallLuaFunction<std::pair<String<20>, bool>>(L, "get_long_string");
if (success) {
    std::cout << "String: " << long_str.data() << std::endl;
} else {
    std::cout << "String was truncated!" << std::endl;
}
```


## API Reference

### CallLuaFunction

The main template function for calling Lua functions:

```cpp
template<typename... ReturnTypes, typename... Args>
auto CallLuaFunction(lua_State* L, std::string_view functionName, Args... args)
```

**Parameters:**

- `L` - Lua state pointer
- `functionName` - Name of the Lua function to call
- `args...` - Arguments to pass to the Lua function

**Return Value:**

- Single return type: Returns the value directly
- Multiple return types: Returns `std::tuple<ReturnTypes...>`
- Void: Returns nothing


## Error Handling

The library throws `std::runtime_error` exceptions for various error conditions:

- Invalid function names
- Type conversion errors
- Lua runtime errors
- Buffer overflows (for fixed-size strings)

```cpp
try {
    int result = CallLuaFunction<int>(L, "nonexistent_function");
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```


## Known Limitations

- **Memory Safety**: Stack cleanup issues in error scenarios
- **Type System**: Incomplete type validation and edge case handling
- **Performance**: Suboptimal memory allocation patterns
- **Error Messages**: Generic error messages with limited debugging context
- **Thread Safety**: Not thread-safe, requires external synchronization


## Requirements

- C++20 or later
- Lua 5.3+ development libraries
- Standard library support for `<variant>`, `<optional>`, and `<format>`


## Building

```bash
# Example CMake configuration
cmake_minimum_required(VERSION 3.12)
project(MyLuaProject)

set(CMAKE_CXX_STANDARD 20)

find_package(Lua REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app ${LUA_LIBRARIES})
target_include_directories(my_app PRIVATE ${LUA_INCLUDE_DIR})
```


## Contributing

This library is in early development. Contributions are welcome, particularly:

- Memory safety improvements
- Better error handling
- Performance optimizations
- Additional type support
- Comprehensive testing