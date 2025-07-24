#pragma once

#include <exception>
#include <memory>
#include <sstream>
#include <string>

#include "backward.hpp"

namespace velodb {

/**
 * @brief Base exception class that captures stack trace at throw site
 * 
 * This class provides automatic stack trace capture when exceptions are thrown,
 * making debugging much easier by preserving the context of where the exception
 * originated.
 */
class TracedException : public std::exception {
public:
    /**
     * @brief Construct exception with message and automatic stack trace
     * @param message Error message describing what went wrong
     */
    explicit TracedException(const std::string& message);
    
    /**
     * @brief Construct exception with message and custom stack trace depth
     * @param message Error message describing what went wrong
     * @param trace_depth Number of stack frames to capture (default: 32)
     */
    TracedException(const std::string& message, size_t trace_depth);
    
    /**
     * @brief Get the error message including stack trace
     * @return Complete error message with stack trace
     */
    const char* what() const noexcept override;
    
    /**
     * @brief Get just the original error message without stack trace
     * @return Original error message
     */
    const std::string& message() const noexcept;
    
    /**
     * @brief Get the formatted stack trace
     * @return Stack trace as string
     */
    const std::string& stack_trace() const noexcept;

private:
    void capture_stack_trace(size_t trace_depth);
    void format_full_message();
    
    std::string message_;
    std::string stack_trace_;
    mutable std::string full_message_;
    backward::StackTrace stack_trace_obj_;
};

/**
 * @brief Database-specific runtime error with stack trace
 */
class DatabaseError : public TracedException {
public:
    explicit DatabaseError(const std::string& message) 
        : TracedException("Database Error: " + message) {}
    
    DatabaseError(const std::string& message, size_t trace_depth)
        : TracedException("Database Error: " + message, trace_depth) {}
};

/**
 * @brief Schema validation error with stack trace
 */
class SchemaError : public TracedException {
public:
    explicit SchemaError(const std::string& message) 
        : TracedException("Schema Error: " + message) {}
    
    SchemaError(const std::string& message, size_t trace_depth)
        : TracedException("Schema Error: " + message, trace_depth) {}
};

/**
 * @brief Query execution error with stack trace
 */
class ExecutionError : public TracedException {
public:
    explicit ExecutionError(const std::string& message) 
        : TracedException("Execution Error: " + message) {}
    
    ExecutionError(const std::string& message, size_t trace_depth)
        : TracedException("Execution Error: " + message, trace_depth) {}
};

/**
 * @brief Type system error with stack trace
 */
class TypeError : public TracedException {
public:
    explicit TypeError(const std::string& message) 
        : TracedException("Type Error: " + message) {}
    
    TypeError(const std::string& message, size_t trace_depth)
        : TracedException("Type Error: " + message, trace_depth) {}
};

/**
 * @brief Catalog operation error with stack trace
 */
class CatalogError : public TracedException {
public:
    explicit CatalogError(const std::string& message) 
        : TracedException("Catalog Error: " + message) {}
    
    CatalogError(const std::string& message, size_t trace_depth)
        : TracedException("Catalog Error: " + message, trace_depth) {}
};

} // namespace velodb

/**
 * @brief Convenience macro for throwing traced exceptions
 * 
 * Usage: VELODB_THROW(DatabaseError, "Something went wrong");
 */
#define VELODB_THROW(ExceptionType, message) \
    throw ExceptionType(message)

/**
 * @brief Convenience macro for throwing traced exceptions with custom trace depth
 * 
 * Usage: VELODB_THROW_DEPTH(DatabaseError, "Something went wrong", 64);
 */
#define VELODB_THROW_DEPTH(ExceptionType, message, depth) \
    throw ExceptionType(message, depth)
