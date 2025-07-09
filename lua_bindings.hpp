#ifndef LUA_LUACALLSFROMCPP
#define LUA_LUACALLSFROMCPP

#include <lua.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <type_traits>
#include <stdexcept>
#include <optional>
#include <variant>
#include <format>

#define LUA_TINTEGER 200
#define LUA_BASIC_TYPES lua_Integer, lua_Number, bool, std::string, std::nullopt_t
#define lua_get_type(name, L, index) \
int name = lua_type(L, index); \
    do { \
    if (name == LUA_TNUMBER) { \
        if (lua_isinteger(L, index)) { \
            name = LUA_TINTEGER; \
        } \
    } } while (false)
using LuaType = std::variant<LUA_BASIC_TYPES,
std::optional<std::vector<std::variant<LUA_BASIC_TYPES>>>,
std::optional<std::unordered_map<std::variant<LUA_BASIC_TYPES>, std::variant<LUA_BASIC_TYPES>>>
>;
using BasicLuaType = std::variant<LUA_BASIC_TYPES>;

template<size_t N>
using String = std::array<char, N>;

struct BasicLuaTypeHasher {
    size_t operator()(const BasicLuaType& value) const {
        return std::visit([](const auto& v) -> size_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullopt_t>) {
                throw std::invalid_argument("Cannot hash std::nullopt_t");
            } else {
                return std::hash<T>{}(v);
            }
        }, value);
    }
};

class LuaFunctionCaller {
private:

template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
struct MapTypesExtractor;

template<typename Key, typename Value, typename Hash, typename KeyEqual, typename Allocator>
struct MapTypesExtractor<std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>> {
    using key_type = Key;
    using mapped_type = Value;
};
// Helper to check if a type is std::unordered_map
template<typename T>
struct is_unordered_map : std::false_type {};

template<typename K, typename V>
struct is_unordered_map<std::unordered_map<K, V>> : std::true_type {};

// Helper to check if a type is std::vector
template<typename T>
struct is_vector : std::false_type {};

template<typename T>
struct is_vector<std::vector<T>> : std::true_type {};

template <typename T>
struct is_array : std::false_type {};

template <typename T, size_t N>
struct is_array<std::array<T, N>> : std::true_type {};

template <typename T>
struct is_array_size_pair : std::false_type {};

// Specialization for std::pair<std::array<T, N>, M>
template <typename T, size_t N, typename M>
struct is_array_size_pair<std::pair<std::array<T, N>, M>> 
    : std::is_integral<M> {};


template <typename T>
struct is_string : std::false_type {};

template <size_t N>
struct is_string<std::array<char, N>> : std::true_type {};
    
template <typename T>
struct is_string_bool_pair : std::false_type {};

// Specialization for std::pair<std::array<T, N>, M>
template <size_t N>
struct is_string_bool_pair<std::pair<std::array<char, N>, bool>> 
    : std::true_type {};

template<typename T>
struct is_BasicLuaType : std::false_type {};
    
static bool isList(lua_State* L, int index) {
    // Normalize the index to handle negative indices
    index = lua_absindex(L, index);

    // Check if the value is a table
    if (!lua_istable(L, index)) {
        return false;
    }

    // Get the length of the table
    lua_len(L, index);
    lua_Integer tableLength = lua_tointeger(L, -1);
    lua_pop(L, 1);

    // If length is 0, it can be considered a list (empty list)
    if (tableLength == 0) {
        return true;
    }

    // Check for consecutive integer keys starting from 1
    lua_Integer maxKey = 0;
    lua_pushnil(L);  // Start with the first key (nil)
    while (lua_next(L, index)) {
        // Check if the key is an integer
        if (!lua_isinteger(L, -2)) {
            lua_pop(L, 2);
            return false;
        }

        lua_Integer key = lua_tointeger(L, -2);

        // Check if key is greater than 0
        if (key < 1) {
            lua_pop(L, 2);
            return false;
        }

        // Keep track of the largest key
        if (key > maxKey) {
            maxKey = key;
        }

        lua_pop(L, 1);  // Pop the value, keep the key for the next iteration
    }

    // Ensure that maxKey matches table length (allowing nil values in the middle)
    return maxKey == tableLength;
}


static bool isDict(lua_State* L, int index) {
    // Normalize the index to handle negative indices
    index = lua_absindex(L, index);

    // Check if the value is a table
    if (!lua_istable(L, index)) {
        return false;
    }

    // Check for nil keys or other invalid key types
    lua_pushnil(L);  // Push nil to start the iteration
    while (lua_next(L, index)) {
        // Check key type
        int keyType = lua_type(L, -2);
        
        // Reject nil keys
        if (keyType == LUA_TNIL) {
            lua_pop(L, 2);
            return false;
        }

        lua_pop(L, 1);  // Pop value, keep the key for next iteration
    }

    // If we reach here, the table passes all checks
    return true;
}

/*
template<typename T>
void checkType(lua_State* L, const char* fn, int type) {
    if constexpr (std::is_same_v<T, BasicLuaType> ) {
        switch (type) {
            case LUA_TNIL:
            case LUA_TINTEGER:
            case LUA_TNUMBER:
            case LUA_TBOOLEAN:
            case LUA_TSTRING:
                break;
            default:
                throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}, expected BasicLuaType", fn));
        }
    }
    else if constexpr (is_optional<T>::value) {
        switch (type) {
            case LUA_TNIL:
                break;
            default:
                using ValueType = typename T::value_type;
            checkType<ValueType>(L, fn, type);
            return;
        }
    }
    else if constexpr (std::is_integral_v<T>) {
        switch (type) {
            case LUA_TNIL:
                throw std::runtime_error(std::format("Unexpected nil {}, expected an integer", fn));
            case LUA_TINTEGER:
            case LUA_TNUMBER:
            case LUA_TBOOLEAN:
                break;
            case LUA_TSTRING:
                throw std::runtime_error(std::format("Unexpected string {}, expected an integer", fn));
            default:
                throw std::runtime_error(std::format("Unexpected non-BasicLuaType type {}", fn));
        }
    }
    else if constexpr (std::is_floating_point_v<T>) {
        switch (type) {
            case LUA_TNIL:
                throw std::runtime_error(std::format("Unexpected nil {}, expected a float", fn));
            case LUA_TINTEGER:
            case LUA_TNUMBER:
                break;
            case LUA_TBOOLEAN:
                throw std::runtime_error(std::format("Unexpected float {}, expected a float", fn));
            case LUA_TSTRING:
                throw std::runtime_error(std::format("Unexpected string {}, expected a float", fn));
            default:
                throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}, expected a float", fn));
        }
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        if (type != LUA_TSTRING) {
            throw std::runtime_error(std::format("Unexpected non-string type {}", fn));
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        switch (type) {
            case LUA_TNIL:
                throw std::runtime_error(std::format("Unexpected nil {}, expected a bool", fn));
            case LUA_TINTEGER:
                break;
            case LUA_TNUMBER:
                throw std::runtime_error(std::format("Unexpected float {}, expected a bool", fn));
            case LUA_TBOOLEAN:
                break;
            case LUA_TSTRING:
                throw std::runtime_error(std::format("Unexpected string {}, expected a bool", fn));
            default:
                throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}, expected a bool", fn));
        }
    }
}*/

// Function to read from Lua stack
public:
template<typename T>
static T readFromLuaStack(lua_State* L, const char* fn, int index) {
    if constexpr (std::is_same_v<T, BasicLuaType>) {
    lua_get_type(type, L, index);
    switch (type) {
        case LUA_TNIL:
            return std::nullopt;
        case LUA_TINTEGER:
            return lua_tointeger(L, index);
        case LUA_TNUMBER:
            return lua_tonumber(L, index);
        case LUA_TBOOLEAN:
            return static_cast<bool>(lua_toboolean(L, index));
        case LUA_TSTRING:
            return std::string(lua_tostring(L, index));
        default:
            throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}", fn));
    }
} else if constexpr (is_optional<T>::value) {
        if (lua_isnil(L, index)) {
            return std::nullopt;  // Return std::nullopt if nil
        } else {
            using ValueType = typename T::value_type;
            return readFromLuaStack<ValueType>(L, fn, index);  // Read normally
        }
    } else if constexpr (std::is_integral_v<T>) {
        lua_get_type(type, L, index);
    switch (type) {
        case LUA_TNIL:
            throw std::runtime_error(std::format("Unexpected nil {}, expected an integer", fn));
        case LUA_TINTEGER:
            return static_cast<T>(lua_tointeger(L, index));
        case LUA_TNUMBER:
            return static_cast<T>(lua_tonumber(L, index));
        case LUA_TBOOLEAN:
            return static_cast<T>(lua_toboolean(L, index));
        case LUA_TSTRING:
            throw std::runtime_error(std::format("Unexpected string {}, expected an integer", fn));
        default:
            throw std::runtime_error(std::format("Unexpected non-BasicLuaType type {}, expected an integer", fn));
    }
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_get_type(type, L, index);
    switch (type) {
        case LUA_TNIL:
            throw std::runtime_error(std::format("Unexpected nil {}, expected a float", fn));
        case LUA_TINTEGER:
            return static_cast<T>(lua_tointeger(L, index));
        case LUA_TNUMBER:
            return static_cast<T>(lua_tonumber(L, index));
        case LUA_TBOOLEAN:
            throw std::runtime_error(std::format("Unexpected bool {}, expected a float", fn));
        case LUA_TSTRING:
            throw std::runtime_error(std::format("Unexpected string {}, expected a float", fn));
        default:
            throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}, expected a float", fn));
    }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (lua_type(L, index) != LUA_TSTRING) {
            throw std::runtime_error(std::format("Unexpected non-string type {}", fn));
        }
        return std::string(lua_tostring(L, index));
    } else if constexpr (is_string<T>::value) {
        if (lua_type(L, index) != LUA_TSTRING) {
            throw std::runtime_error(std::format("Unexpected non-string type {}", fn));
        }
        T result;
        snprintf(result.data(), result.size(), "%s", lua_tostring(L, index));
        return result;
    } else if constexpr (is_string_bool_pair<T>::value) {
        if (lua_type(L, index) != LUA_TSTRING) {
            throw std::runtime_error(std::format("Unexpected non-string type {}", fn));
        }
        T result;
        result.second = true;
        auto spaceNeeded = snprintf(result.first.data(), result.first.size(), "%s", lua_tostring(L, index));
        if (spaceNeeded >= result.first.size()) {
            result.second = false;
        }
        return result;
    } else if constexpr (std::is_same_v<T, bool>) {
        lua_get_type(type, L, index);
    switch (type) {
        case LUA_TNIL:
            throw std::runtime_error(std::format("Unexpected nil {}, expected a bool", fn));
        case LUA_TINTEGER:
            return static_cast<T>(lua_tointeger(L, index));
        case LUA_TNUMBER:
            throw std::runtime_error(std::format("Unexpected float {}, expected a bool", fn));
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index);
        case LUA_TSTRING:
            throw std::runtime_error(std::format("Unexpected string {}, expected a bool", fn));
        default:
            throw std::runtime_error(std::format("Unexpected non-BasicLuaType {}, expected a bool", fn));
    }
    } else if constexpr (is_vector<T>::value) {
        if (!isList(L, index) ) {
            throw std::runtime_error(std::format("Unexpected non-list type {}, expected a list", fn));
        }
        using ValueType = typename T::value_type;
        T result;
        luaL_checktype(L, index, LUA_TTABLE);
        int len = lua_rawlen(L, index);
        result.reserve(len);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, index, i);
            result.push_back(readFromLuaStack<ValueType>(L, fn, -1));
            lua_pop(L, 1);
        }
        return std::move(result);
    } else if constexpr (is_array_size_pair<T>::value) {
        if (!isList(L, index) ) {
            throw std::runtime_error(std::format("Unexpected non-list type {}, expected a list", fn));
        }

        T result;
        using ArrayType = decltype(result.first);
        using ValueType = typename ArrayType::value_type;
        constexpr size_t size = result.first.size();
        
        luaL_checktype(L, index, LUA_TTABLE);
        int len = lua_rawlen(L, index);
        for (int i = 1; i <= len; ++i) {
            if (i > size) {
                throw std::runtime_error(std::format("Array buffer overflow {}", fn));
            }
            lua_rawgeti(L, index, i);
            result.first[i-1] = (readFromLuaStack<ValueType>(L, fn, -1));
            lua_pop(L, 1);
        }
        result.second = len;
        return result;
    } else if constexpr (is_unordered_map<T>::value) {
        if (!isDict(L, index)) {
            throw std::runtime_error(std::format("Unexpected non-dict type {}, expected a dict", fn));
        }
        using KeyType = typename MapTypesExtractor<T>::key_type;
        using MappedType = typename MapTypesExtractor<T>::mapped_type;
    
        // First, count the number of elements to reserve space
        int tableIndex = lua_absindex(L, index);
        lua_pushnil(L);  // First key
        int count = 0;
        while (lua_next(L, tableIndex) != 0) {
            if (lua_type(L, -1) != LUA_TNIL) {
                count++;
            }
            lua_pop(L, 1);  // Remove value, keep key for next iteration
        }
    
        // Create the result map with reserved space
        T result;
        result.reserve(count);  // Preallocate space
    
        // Reset the iterator
        lua_pushnil(L);  // First key
        while (lua_next(L, tableIndex) != 0) {
            if (lua_type(L, -1) != LUA_TNIL) {
                lua_get_type(keytype, L, -2);
                lua_get_type(valtype, L, -1);

                auto key = readFromLuaStack<KeyType>(L, fn, -2);
                auto value = readFromLuaStack<MappedType>(L, fn, -1);
                result[key] = value;
            }
            lua_pop(L, 1);  // Remove value, keep key for next iteration
        }
        return std::move(result);
    }

    
    // Fallback for unsupported types
    throw std::runtime_error("Unsupported Lua to C++ type");
}

// Function to push to Lua stack
template<typename T>
static void pushToLuaStack(lua_State* L, const T& value) {
    if constexpr (std::is_same_v<T, BasicLuaType>) {
        std::visit([L](auto&& arg) {
            return pushToLuaStack(L, arg);
        }, value);
    }
    else if constexpr (std::is_same_v<T, std::nullopt_t>) {
        lua_pushnil(L);
    }
    else if constexpr (is_optional<T>::value) {
        if (value) {
            pushToLuaStack(L, *value);  // Push the contained value
        } else {
            lua_pushnil(L);  // Push nil if the optional is empty
        }
    }
    else if constexpr (is_unordered_map<T>::value) {
        lua_createtable(L, 0, value.size());
        for (const auto& [k, v] : value) {
            pushToLuaStack(L, k);
            pushToLuaStack(L, v);  // Push the value
            lua_settable(L, -3);  // Set the key-value pair in the table
        }
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            lua_pushinteger(L, static_cast<lua_Integer>(value));
        } else {
            lua_pushinteger(L, static_cast<lua_Unsigned>(value));
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, static_cast<lua_Number>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        lua_pushstring(L, value.c_str());
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        lua_pushstring(L, value.data());
    } else if constexpr (std::is_same_v<T, const char*>) {
        lua_pushstring(L, value);
    } else if constexpr (std::is_same_v<T, bool>) {
        lua_pushboolean(L, value);
    } else if constexpr (is_vector<T>::value || is_array<T>::value) {
        lua_createtable(L, value.size(), 0);
        for (size_t i = 0; i < value.size(); ++i) {
            pushToLuaStack(L, value[i]);  // Pass Lua state and value
            lua_rawseti(L, -2, i + 1);  // Set the value in the table
        }
    } else if (is_array_size_pair<T>::value) {
        lua_createtable(L, value.second, 0);
        for (size_t i = 0; i < value.second; ++i) {
            pushToLuaStack(L, value.first[i]);  // Pass Lua state and value
            lua_rawseti(L, -2, i + 1);  // Set the value in the table
        }
    }
    else {
        throw std::runtime_error("Unsupported C++ to Lua type");
    }
}


private:


    // Helper to call a Lua function with multiple return values
    template<typename... ReturnTypes>
    static std::tuple<ReturnTypes...> processMultiReturn(lua_State* L, const char* fn, int numReturns) {
        return processMultiReturnImpl<ReturnTypes...>(L, numReturns, fn, std::index_sequence_for<ReturnTypes...>{});
    }

    template<typename... ReturnTypes, size_t... Is>
    static std::tuple<ReturnTypes...> processMultiReturnImpl(lua_State* L, int numReturns, const char* fn, std::index_sequence<Is...>) {
        return std::make_tuple(readFromLuaStack<ReturnTypes>(L, fn, -(numReturns - Is))...);
    }

public:
    // Call a Lua function with no return value
    template<typename... Args>
    static void callVoid(lua_State* L, std::string_view functionName, Args... args) {
        // Get the function from the global table
        lua_getglobal(L, functionName.data());

        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);  // Remove the invalid function from the stack
            throw std::runtime_error("Function '" + std::string(functionName) + "' is not a valid Lua function.");
        }

        // Push arguments
        (pushToLuaStack(L, args), ...);

        // Call the function
        if (lua_pcall(L, sizeof...(Args), 0, 0) != LUA_OK) {
            throw std::runtime_error(lua_tostring(L, -1));
        }
    }
    
    // Call a Lua function with a single return value
    template<typename ReturnType, typename... Args>
    static ReturnType call(lua_State* L, std::string_view functionName, Args... args) {
        // Get the function from the global table
        lua_getglobal(L, functionName.data());
        
        if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);  // Remove the invalid function from the stack
        throw std::runtime_error("Function '" + std::string(functionName) + "' is not a valid Lua function.");
    }
        
        // Push arguments
        (pushToLuaStack(L, args), ...);

        // Call the function
        if (lua_pcall(L, sizeof...(Args), 1, 0) != LUA_OK) {
            throw std::runtime_error(lua_tostring(L, -1));
        }

        // Read and return the result
        char debugstr[255];
        snprintf(debugstr, sizeof(debugstr), "returned by %s()", functionName.data());
        ReturnType result = readFromLuaStack<ReturnType>(L, debugstr, -1);
        lua_pop(L, 1);
        return result;
    }

    // Call a Lua function with multiple return values
    template<typename... ReturnTypes, typename... Args>
    static std::tuple<ReturnTypes...> callMultiReturn(lua_State* L, std::string_view functionName, Args... args) {
        // Get the function from the global table
        lua_getglobal(L, functionName.data());
        
if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);  // Remove the invalid function from the stack
        throw std::runtime_error("Function '" + std::string(functionName) + "' is not a valid Lua function.");
    }

        
        // Push arguments
        (pushToLuaStack(L, args), ...);

        // Call the function
        constexpr int numReturns = sizeof...(ReturnTypes);
        if (lua_pcall(L, sizeof...(Args), numReturns, 0) != LUA_OK) {
            throw std::runtime_error(lua_tostring(L, -1));
        }

        // Process multiple return values
        char debugstr[255];
        snprintf(debugstr, sizeof(debugstr), "returned by %s()", functionName.data());
        auto result = processMultiReturn<ReturnTypes...>(L, debugstr, numReturns);
        lua_pop(L, numReturns);
        return result;
    }
};

// Function to determine if there are multiple return types
template<typename... ReturnTypes>
constexpr bool hasMultipleReturnTypes() {
    return sizeof...(ReturnTypes) > 1;
}


template<typename... ReturnTypes>
struct is_void_only {
    static constexpr bool value = (sizeof...(ReturnTypes) == 1 && std::is_same_v<std::tuple_element_t<0, std::tuple<ReturnTypes...>>, void>);
};

// Unified CallLuaFunction
template<typename... ReturnTypes, typename... Args>
auto CallLuaFunction(lua_State* L, std::string_view functionName, Args... args) {
    if constexpr (hasMultipleReturnTypes<ReturnTypes...>()) {
        return LuaFunctionCaller::callMultiReturn<ReturnTypes...>(L, functionName, args...);
    } else if constexpr (is_void_only<ReturnTypes...>::value) {
        LuaFunctionCaller::callVoid(L, functionName, args...); // Assume this function exists
    } else {
        static_assert(sizeof...(ReturnTypes) == 1, "Must have exactly one return type if not multiple");
        return LuaFunctionCaller::call<std::tuple_element_t<0, std::tuple<ReturnTypes...>>>(L, functionName, args...);
    }
}


#endif