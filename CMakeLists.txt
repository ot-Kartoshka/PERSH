# CMakeList.txt: проект CMake для Lab4; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#
cmake_minimum_required (VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Lab4")

# Добавьте источник в исполняемый файл этого проекта.
add_executable (Lab4 "main.cpp" "include/MathUtils.h" "src/MathUtils.cpp" "include/Attack.h" "src/Attack.cpp" "include/Geffe.h" "include/LRZ.h" "include/Errors.h" "src/Geffe.cpp" "src/LRZ.cpp" "src/Errors.cpp")

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
