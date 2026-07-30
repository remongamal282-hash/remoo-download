#ifndef REMO_COMMON_RESULT_H
#define REMO_COMMON_RESULT_H

#include <string>
#include <variant>
#include <optional>

namespace remo {
namespace common {

enum class ErrorCode {
    None = 0,
    NetworkError,
    DiskError,
    IntegrityError,
    ServerError,
    InternalError,
    InvalidArgument,
    NotFound,
    PermissionDenied,
    Timeout,
    AlreadyExists,
    NotImplemented
};

struct RemoError {
    ErrorCode code = ErrorCode::None;
    std::string message;
    std::string context;

    RemoError() = default;
    RemoError(ErrorCode c, std::string m, std::string ctx = "")
        : code(c), message(std::move(m)), context(std::move(ctx)) {}

    bool operator==(const RemoError& other) const {
        return code == other.code && message == other.message;
    }
    bool operator!=(const RemoError& other) const {
        return !(*this == other);
    }
    explicit operator bool() const {
        return code != ErrorCode::None;
    }
};

template <typename T>
using Result = std::variant<T, RemoError>;

template <typename T>
bool isSuccess(const Result<T>& result) {
    return std::holds_alternative<T>(result);
}

template <typename T>
bool isFailure(const Result<T>& result) {
    return std::holds_alternative<RemoError>(result);
}

template <typename T>
T unwrap(const Result<T>& result) {
    return std::get<T>(result);
}

template <typename T>
const RemoError& unwrapError(const Result<T>& result) {
    return std::get<RemoError>(result);
}

template <typename T>
std::optional<T> tryUnwrap(const Result<T>& result) {
    if (std::holds_alternative<T>(result)) {
        return std::get<T>(result);
    }
    return std::nullopt;
}

} // namespace common
} // namespace remo

#endif // REMO_COMMON_RESULT_H