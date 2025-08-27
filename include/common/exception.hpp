#pragma once

#include <exception>
#include <memory>
#include <string>

namespace velodb {

class TracedException : public std::exception {
public:
    explicit TracedException(const std::string& message);
    TracedException(const std::string& message, size_t trace_depth);

    const char* what() const noexcept override;
    const std::string& message() const noexcept;

    const std::string& stack_trace() const noexcept;

private:
    void capture_stack_trace(size_t trace_depth);
    void format_full_message();

    std::string message_;
    std::string stack_trace_;
    mutable std::string full_message_;
};

class DatabaseError : public TracedException {
public:
    explicit DatabaseError(const std::string& message)
        : TracedException("Database Error: " + message)
    {
    }

    DatabaseError(const std::string& message, size_t trace_depth)
        : TracedException("Database Error: " + message, trace_depth)
    {
    }
};

class SchemaError : public TracedException {
public:
    explicit SchemaError(const std::string& message)
        : TracedException("Schema Error: " + message)
    {
    }

    SchemaError(const std::string& message, size_t trace_depth)
        : TracedException("Schema Error: " + message, trace_depth)
    {
    }
};

class ExecutionError : public TracedException {
public:
    explicit ExecutionError(const std::string& message)
        : TracedException("Execution Error: " + message)
    {
    }

    ExecutionError(const std::string& message, size_t trace_depth)
        : TracedException("Execution Error: " + message, trace_depth)
    {
    }
};

class TypeError : public TracedException {
public:
    explicit TypeError(const std::string& message)
        : TracedException("Type Error: " + message)
    {
    }

    TypeError(const std::string& message, size_t trace_depth)
        : TracedException("Type Error: " + message, trace_depth)
    {
    }
};

class CatalogError : public TracedException {
public:
    explicit CatalogError(const std::string& message)
        : TracedException("Catalog Error: " + message)
    {
    }

    CatalogError(const std::string& message, size_t trace_depth)
        : TracedException("Catalog Error: " + message, trace_depth)
    {
    }
};

} // namespace velodb

#define VELODB_THROW(ExceptionType, message) throw ExceptionType(message)
#define VELODB_THROW_DEPTH(ExceptionType, message, depth) throw ExceptionType(message, depth)
