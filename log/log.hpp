﻿#ifndef LOG_HPP
#define LOG_HPP
#include <string>
#include <stdexcept>
#include <source_location>
#include <filesystem>

namespace l
{

inline constexpr char8_t const* const REL_LOG_DIR    = u8"./Log/";
inline constexpr char8_t const* const LOG_FILE_NAME  = u8"log.txt";

struct FileInfo
{
    std::u8string fileName;
    std::filesystem::path path;
};

/**
 * @brief Before calling any of these. Call init.
 */
void init();
/**
 * @brief Before app shutdown call exit. This blocks until all logs written.
 */
void exit();
void Log(std::string const& message, FileInfo const& fileInfo = {.fileName = LOG_FILE_NAME, .path = REL_LOG_DIR});
/**
 * @brief Write a message to a file without extra debug information.
 */
void LogPlain(std::string const& message, FileInfo const& fileInfo = {.fileName = LOG_FILE_NAME, .path = REL_LOG_DIR});
/**
 * @brief Like Log function but writes extra debug information and prints "error".
 */
void LogEr(std::string const& message, FileInfo const& fileInfo = {.fileName = LOG_FILE_NAME, .path = REL_LOG_DIR},
           std::source_location location = std::source_location::current());
/**
 * @brief Like LogEr but prints "warning"
 */
void LogWarn(std::string const& message, FileInfo const& fileInfo = {.fileName = LOG_FILE_NAME, .path = REL_LOG_DIR},
           std::source_location location = std::source_location::current());

}// namespace l

//log error, and throw std::runtime_error
#define LThrow(message, ...) {\
    l::LogEr(message __VA_OPT__(,) __VA_ARGS__);\
    throw std::runtime_error(message);}

#endif // LOG_HPP

