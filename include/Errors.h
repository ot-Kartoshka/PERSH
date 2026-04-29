#pragma once
#include <string_view>

enum class Error {
    FileNotFound,
    VariantNotFound,
    SequenceTooShort,
    InvalidArgument,
};

std::string_view error_to_string(Error err);