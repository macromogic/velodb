#include "common/exception.hpp"

#include <fmt/format.h>

#include <sstream>

namespace velodb {

TracedException::TracedException(const std::string& message)
    : message_(message)
{
    capture_stack_trace(32);
    format_full_message();
}

TracedException::TracedException(const std::string& message, size_t trace_depth)
    : message_(message)
{
    capture_stack_trace(trace_depth);
    format_full_message();
}

const char* TracedException::what() const noexcept
{
    return full_message_.c_str();
}

const std::string& TracedException::message() const noexcept
{
    return message_;
}

const std::string& TracedException::stack_trace() const noexcept
{
    return stack_trace_;
}

void TracedException::capture_stack_trace(size_t trace_depth)
{
    // Capture the stack trace
    stack_trace_obj_.load_here(trace_depth);
    stack_trace_obj_.skip_n_firsts(5);

    // Format the stack trace
    backward::Printer printer;
    printer.snippet = false;
    std::ostringstream oss;

    // Print the full stack trace (backward-cpp will handle formatting)
    printer.print(stack_trace_obj_, oss);

    stack_trace_ = oss.str();
}

void TracedException::format_full_message()
{
    if (!stack_trace_.empty()) {
        full_message_ = fmt::format("{}\n\n{}", message_, stack_trace_);
    } else {
        full_message_ = message_;
    }
}

} // namespace velodb
