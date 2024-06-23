#include "log.hpp"
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <deque>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

static char8_t const* const REL_LOG_DIR    = u8"./Log/";
static char8_t const* const LOG_FILE_NAME  = u8"log.txt";

namespace l
{

enum enLogType
{
    ER,
    INFO,
    PLAIN
};
struct LoIn
{
    enLogType type;
    std::thread::id thread_id;
    boost::interprocess::ipcdetail::OS_process_id_t proc_id;
    std::u8string file_name;
    std::string message;
};

class LogThread
{
public:
    LogThread();
    ~LogThread();
    void putInfo(LoIn & info);
private:
    /**
     * @brief mark working thread for stop. It will quit when all logs have written
     */
    void stop();
    void handle_data(LoIn && info);
    void run();
    bool m_work;
    std::jthread m_thread;
    std::condition_variable m_cv;
    mutable std::mutex m_mutex;
    std::deque<LoIn> m_q;
};

std::shared_mutex g_mut_lg;
LogThread* g_log_thread = nullptr;
void hm1(std::string & message,
         std::u8string & fileName, l::LoIn & ret);
void handle_dataPlain(std::string && message, std::u8string && fileName);

inline
LogThread::LogThread():
    m_work(true),
    m_thread(&LogThread::run, this)
{}
inline
LogThread::~LogThread()
{
    stop();
}

inline void LogThread::putInfo(LoIn & info)
{
    {
        std::lock_guard<std::mutex> const l(m_mutex);
        if(!m_work)
            return;
        m_q.emplace_back(std::move(info));
    }
    m_cv.notify_all();
}

void LogThread::run()
{
    try
    {
        while(true)
        {
            std::unique_lock<std::mutex> l(m_mutex);
            m_cv.wait(l, [&]() -> bool
            {
                return !m_q.empty() || !m_work;
            });
            if(!m_work && m_q.empty())
            {
                break;
            }
            if(m_q.empty() == false)
            {
                std::deque<LoIn> qc;
                qc.swap(m_q);
                l.unlock();
                for(auto &dat:qc)
                {
                    handle_data(std::move(dat));
                }
            }
        }
    }
    catch (...)
    {}
}
inline void LogThread::stop()
{
    {
        std::lock_guard<std::mutex> const l(m_mutex);
        m_work =  false;
    }
    m_cv.notify_all();
    if(m_thread.joinable())
        m_thread.join();
}

void Log(std::string message, std::u8string fileName)
{
    std::shared_lock<std::shared_mutex> l(g_mut_lg);
    if(g_log_thread)
    {
        LoIn in;
        hm1(message, fileName, in);
        in.type = l::enLogType::INFO;
        g_log_thread->putInfo(in);
    }
}

void Log(std::string message)
{
    Log(std::move(message), LOG_FILE_NAME);
}
void LogPlain(std::string message)
{
    LogPlain(std::move(message), LOG_FILE_NAME);
}
void LogPlain(std::string message, std::u8string fileName)
{
    std::shared_lock<std::shared_mutex> l(g_mut_lg);
    if(g_log_thread)
    {
        LoIn in;
        in.file_name = std::move(fileName);
        in.message = std::move(message);
        in.type = l::enLogType::PLAIN;
        g_log_thread->putInfo(in);
    }
}
void LogEr(std::string message, std::source_location location)
{
    LogEr(std::move(message), LOG_FILE_NAME, std::move(location));
}
void LogEr(std::string message, std::u8string fileName, std::source_location location)
{
    std::shared_lock<std::shared_mutex> l(g_mut_lg);
    if(g_log_thread)
    {
        std::string mnew;
        mnew+= std::string("file: ") +
                location.file_name() +
                ":" + std::to_string(location.line()) + ":\n" +
                message;
        LoIn in;
        hm1(mnew, fileName, in);
        in.type = l::enLogType::ER;
        g_log_thread->putInfo(in);
    }
}
void exit()
{
    std::lock_guard<std::shared_mutex> l(g_mut_lg);
    delete g_log_thread;
    g_log_thread = nullptr;
}
void init()
{
    std::lock_guard<std::shared_mutex> l(g_mut_lg);
    if(!g_log_thread)
    {
        std::filesystem::path const REL_DIR = std::filesystem::u8path(REL_LOG_DIR).make_preferred();
        if(std::filesystem::exists(REL_DIR) == false)
        {
            std::filesystem::create_directory(REL_DIR);
        }
        g_log_thread = new LogThread();
    }
}
void LogThread::handle_data(l::LoIn && info)
{
    if(info.type == l::enLogType::PLAIN)
    {
        handle_dataPlain(std::move(info.message), std::move(info.file_name));
    }
    else
    {
        //**inhance a message
        std::string all_mess;
        {
            std::string th_id;
            {
                std::ostringstream ss;
                ss<<info.thread_id;
                th_id = ss.str();
            }
            std::string proc_id;
            {
                std::ostringstream ss;
                ss<<info.proc_id;
                proc_id = ss.str();
            }
            std::string time;
            {
                std::ostringstream ss;
                std::time_t t = std::time(nullptr);
                std::tm tm = *std::localtime(&t);
                ss << std::put_time(&tm, "%H:%M:%S %d/%m/%Y");
                time = ss.str();
            }
            if(info.type == l::enLogType::ER)
            {
                all_mess+="error ";
            }
            all_mess+=time +" "+ proc_id+ " " + th_id+ " " + info.message;
        }
        //**
        handle_dataPlain(std::move(all_mess), std::move(info.file_name));
    }
}
void handle_dataPlain(std::string && message, std::u8string && fileName)
{
    std::filesystem::path const LOG_FILE = std::filesystem::u8path(REL_LOG_DIR + fileName).make_preferred();
    std::ofstream file(LOG_FILE, std::ios::out | std::ios::app);
    boost::interprocess::file_lock fl(LOG_FILE.native().c_str());
    {
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> const e_lock(fl);
        file<<message<<std::endl;
#ifndef NDEBUG
        std::cout<<message<<std::endl;
#endif
    }
}
inline void hm1(std::string & message,
                std::u8string & fileName, LoIn & in)
{
    in.thread_id = std::this_thread::get_id();
    in.proc_id = boost::interprocess::ipcdetail::get_current_process_id();
    in.file_name = std::move(fileName);
    in.message = std::move(message);
}
}//namespace l
