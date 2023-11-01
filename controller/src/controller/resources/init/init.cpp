#include "init.hpp"
#include "../../app/execution-context.hpp"
#include <boost/context/fiber.hpp>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <fcntl.h>

static void base64extract(const std::string& filename, const controller::resources::init::Request& req){
    int downstream[2] = {};
    int len = 0;
    std::size_t bytes_written = 0;
    //syscall return two pipes.
    if (pipe(downstream) == -1){
        std::cerr << "init.cpp:14:pipe(downstream) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    pid_t pid = fork();
    if (pid == 0){
        if( close(downstream[1]) == -1 ){
            std::cerr << "init.cpp:21:close(downstream[1]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        if (dup2(downstream[0], STDIN_FILENO) == -1){
            std::cerr << "init.cpp:25:dup2(downstream[0], STDIN) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC);
        if(fd == -1){
            std::cerr << "init.cpp:30:open() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        if (dup2(fd, STDOUT_FILENO) == -1){
            std::cerr << "init.cpp:34:dup2(fd, STDOUT) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        std::vector<const char*> argv{"/usr/bin/base64", "-d", nullptr};
        execve("/usr/bin/base64", const_cast<char* const*>(argv.data()), environ);
        exit(1);
    }
    if ( close(downstream[0]) == -1 ){
        std::cerr << "init.cpp:42:close(downstream[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    do{
        len = write(downstream[1], req.value().code().data() + bytes_written, req.value().code().size() - bytes_written);
        if(len <= 0){
            switch(errno)
            {
                case 0:
                    break;
                case EINTR:
                    break;
                default:
                    std::cerr << "init.cpp:55:write() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else {
            bytes_written += len;
        }
    }while(bytes_written < req.value().code().size());
    if (close(downstream[1]) == -1){
        std::cerr << "init.cpp:63:close() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    waitpid(pid,nullptr,0);
}

static void tar_extract(const std::string& filename){
    const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
    std::string fn_path;
    if ( __OW_ACTIONS != nullptr ){
        fn_path = std::string(__OW_ACTIONS);
    } else {
        std::cerr << "init.cpp:75:__OW_ACTIONS envvar not defiend." << std::endl;
        throw "what?";
    }
    std::vector<const char*> argv{"/usr/bin/tar", "-C", fn_path.c_str(), "-xf", filename.c_str(), nullptr};
    pid_t pid = fork();
    switch(pid)
    {
        case 0:
            execve("/usr/bin/tar", const_cast<char* const*>(argv.data()), environ);
            exit(1);
        case -1:
            std::cerr << "init.cpp:86:fork() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        default:
            break;
    }
    waitpid(pid, nullptr, 0);
}

static void initialize(controller::resources::init::Request& req){
    if ( setenv("__OW_ACTION_ENTRY_POINT", req.value().main().c_str(), 1) == -1 ){
        std::cerr << "init.cpp:96:setenv() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
    }
    const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
    std::filesystem::path path;
    if (__OW_ACTIONS == nullptr){
        std::cerr << "init.cpp:101:__OW_ACTIONS envvar not defined." << std::endl;
        throw "Environment variable __OW_ACTIONS not defined.";
    }
    path = std::filesystem::path(__OW_ACTIONS);
    if ( req.value().binary() ){
        path /= "archive.tgz";
        base64extract(path.string(), req);
        tar_extract(path.string());
        std::filesystem::remove(path);
        for ( auto pair: req.value().env() ){
            if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) == -1){
                std::cerr << "init.cpp:112:setenv() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            }
        }
    } else {
        std::string code(req.value().code());
        std::size_t pos = 0;
        while ( (pos = code.find("\\n", pos)) != std::string::npos ){
            code.replace(pos, 2, "\n");
            ++pos;
        }
        const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
        if (__OW_ACTION_EXT == nullptr){
            std::cerr << "init.cpp:124:__OW_ACTION_EXT envvar not defined." << std::endl;
            throw "Environment variable __OW_ACTION_EXT not defined.";
        }
        // Default file entrypoint is called main. i.e.: "main.lua", "main.js", "main.py".
        std::string filename("main");
        std::string ext;
        filename.append(".");
        filename.append(__OW_ACTION_EXT);
        path /= std::filesystem::path(filename);
        std::ofstream file(path.string());
        file << code;
    }
    return;
}

namespace controller{
namespace resources{
namespace init{
    Request::Request(boost::json::object obj)
      : value_(
            obj["value"].as_object()["name"].as_string(),
            obj["value"].as_object()["main"].as_string(),
            obj["value"].as_object()["code"].as_string(),
            obj["value"].as_object()["binary"].as_bool()
        )
        {
            for ( auto kvp: obj["value"].as_object()["env"].as_object() ){
                std::string key(kvp.key());
                std::string val;
                if ( kvp.value().is_string() ){
                    val = std::string( kvp.value().get_string() );
                } else if ( kvp.value().is_int64() ){
                    val = std::to_string( kvp.value().get_int64() );
                } else if ( kvp.value().is_uint64() ){
                    val = std::to_string( kvp.value().get_uint64() );
                } else if ( kvp.value().is_double() ){
                    val = std::to_string( kvp.value().get_double() );
                }
                value_.env_emplace(key, val);
            }
        }
    
    void Request::run(){
        initialize(*this);
    }

    std::shared_ptr<controller::app::ExecutionContext> handle(){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::init);
        return ctx_ptr;
    }
    
}// namespace init
}// namespace resources
}// namespace controller