#include "../include/Errors.h"

std::string_view error_to_string(Error err) {
    switch (err) {
    case Error::FileNotFound:     return "Файл не знайдено за вказаним шляхом.";
    case Error::VariantNotFound:  return "Вказаний варіант відсутній у файлі.";
    case Error::SequenceTooShort: return "Послідовність замала — потрібно більше знаків гами.";
    case Error::InvalidArgument:  return "Некоректний аргумент.";
    default:                      return "Невідома помилка.";
    }
}