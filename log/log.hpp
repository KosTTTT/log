﻿#ifndef LOG_HPP
#define LOG_HPP
#include <string>

namespace l
{
/**
 * @brief Before calling any of these. Call init. Can't call init after calling "exit"
 */
void init();
/**
 * @brief Before app shutdown call exit. This blocks until all logs written.
 */
void exit();
void Log(std::string const & message);
void Log(std::string const & message, std::u8string const & fileName);
/**
 * @brief use LOG_ERR instead
 */
void LogEr(std::string const & message);
/**
 * @brief use LOG_ERR instead
 */
void LogEr(std::string const & message, std::u8string const & fileName);

}// namespace l

#define LOG_ERR(message, ...) l::LogEr(std::string{"file: "} + \
    __FILE__ + \
    ":" + std::to_string(__LINE__) + "\n" + \
    message __VA_OPT__(,) __VA_ARGS__);
#endif // LOG_HPP