#ifndef LOG_HPP
#define LOG_HPP
#include <string>
#include <stdexcept>
#include <source_location>

namespace l
{
/**
 * @brief Before calling any of these. Call init.
 */
void init();
/**
 * @brief Before app shutdown call exit. This blocks until all logs written.
 */
void exit();
void Log(std::string message);
void Log(std::string message, std::u8string fileName);
/**
 * @brief LogPlain Write a message to a file without extra debug information.
 */
void LogPlain(std::string message);
void LogPlain(std::string message, std::u8string fileName);
/**
 * @brief Like Log function but writes extra debug information and prints "error".
 */
void LogEr(std::string message, std::source_location location = std::source_location::current());
void LogEr(std::string message, std::u8string fileName, std::source_location location = std::source_location::current());

}// namespace l

//log error, and throw std::runtime_error
#define LThrow(message, ...) \
    l::LogEr(message __VA_OPT__(,) __VA_ARGS__);\
    throw std::runtime_error(message);

#endif // LOG_HPP

