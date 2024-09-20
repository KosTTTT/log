#include "log.hpp"
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
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
    std::filesystem::path path;
    std::string message;
};

class LogThread
{
public:
    LogThread();
    ~LogThread();
    void putInfo(LoIn & info);
private:
    void handle_data(LoIn && info);
    void run();
    std::condition_variable_any m_cv;
    mutable std::mutex m_mutex;
    std::deque<LoIn> m_q;
    std::jthread m_thread;
};

constinit std::shared_mutex g_mut_lg;
constinit LogThread* g_log_thread = nullptr;
void hm1(std::string & message,
         std::u8string & fileName, std::filesystem::path & path, l::LoIn & ret);
void handle_dataPlain(std::string && message, std::u8string && fileName, std::filesystem::path && path);

inline
LogThread::LogThread()
{
    m_thread = std::jthread(&LogThread::run, this);
}

inline
LogThread::~LogThread()
{
    m_thread.request_stop();
    if(m_thread.joinable())
        m_thread.join();
}

inline void LogThread::putInfo(LoIn & info)
{
    if(m_thread.get_stop_token().stop_requested())
        return;
    {
        std::lock_guard<std::mutex> const l(m_mutex);
        m_q.emplace_back(std::move(info));
    }
    m_cv.notify_all();
}

void LogThread::run()
{
    try
    {
        std::stop_token stoken = this->m_thread.get_stop_token();
        while(true)
        {
            std::unique_lock<std::mutex> l(m_mutex);
            m_cv.wait(l, stoken, [&]()
                {
                    return !m_q.empty();
                });
            if(stoken.stop_requested() && m_q.empty())
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
    {
        std::cerr<<"Error: LogThread unexpected exception."<<std::endl;
    }
}

void Log(std::string message, FileInfo fileInfo)
{
    std::shared_lock<std::shared_mutex> l(g_mut_lg);
    if(g_log_thread)
    {
        LoIn in;
        hm1(message, fileInfo.fileName, fileInfo.path, in);
        in.type = l::enLogType::INFO;
        g_log_thread->putInfo(in);
    }
}

void LogPlain(std::string message, FileInfo fileInfo)
{
    std::shared_lock<std::shared_mutex> l(g_mut_lg);
    if(g_log_thread)
    {
        LoIn in;
        in.file_name = std::move(fileInfo.fileName);
        in.message = std::move(message);
        in.path = std::move(fileInfo.path);
        in.type = l::enLogType::PLAIN;
        g_log_thread->putInfo(in);
    }
}

void LogEr(std::string message, FileInfo fileInfo, std::source_location location)
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
        hm1(mnew, fileInfo.fileName, fileInfo.path, in);
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
        std::filesystem::path const REL_DIR = REL_LOG_DIR;
        if(std::filesystem::exists(REL_DIR) == false)
        {
            if(!std::filesystem::create_directories(REL_DIR))
            {
                throw std::runtime_error("Could not create directory " + REL_DIR.string());
            }
        }
        g_log_thread = new LogThread();
    }
}
void LogThread::handle_data(l::LoIn && info)
{
    if(info.type == l::enLogType::PLAIN)
    {
        handle_dataPlain(std::move(info.message), std::move(info.file_name), std::move(info.path));
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
        handle_dataPlain(std::move(all_mess), std::move(info.file_name), std::move(info.path));
    }
}
void handle_dataPlain(std::string && message, std::u8string && fileName, std::filesystem::path && path)
{
    if(fileName.empty())
        fileName = LOG_FILE_NAME;
    if(path.empty())
        path = std::filesystem::path(REL_LOG_DIR);
    if(!std::filesystem::exists(path))
    {
        bool success;
        try
        {
            success = std::filesystem::create_directories(path);
        }
        catch (...)
        {
            success = false;
        }
        if(!success)
        {
            LogEr("Could not create directory " + path.string());
            return;
        }
    }
    path/=fileName;
    path.make_preferred();
    std::ofstream file(path, std::ios::out | std::ios::app);
    if(!file)
    {
        LogEr("Could not open a file " + path.string());
        return;
    }
    boost::interprocess::file_lock fl(path.native().c_str());
    {
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> const e_lock(fl);
        file<<message<<std::endl;
#ifndef NDEBUG
        std::cout<<message<<std::endl;
#endif
    }
}
inline void hm1(std::string & message,
                std::u8string & fileName, std::filesystem::path & path, LoIn & in)
{
    in.thread_id = std::this_thread::get_id();
    in.proc_id = boost::interprocess::ipcdetail::get_current_process_id();
    in.file_name = std::move(fileName);
    in.path = std::move(path);
    in.message = std::move(message);
}
}//namespace l
