#include "init.hpp"
#include "../../app/execution-context.hpp"
#include <boost/context/fiber.hpp>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <fcntl.h>

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

    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::init);
        boost::context::fiber f{
            [&, req, ctx_ptr](boost::context::fiber&& g){
                g = std::move(g).resume();
                if ( setenv("__OW_ACTION_ENTRY_POINT", req.value().main().c_str(), 1) == -1 ){
                    perror("Setting the action entry point environment variable faield.");
                }
                const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
                std::filesystem::path path;
                if (__OW_ACTIONS == nullptr){
                    throw "Environment variable __OW_ACTIONS not defined.";
                }
                path = std::filesystem::path(__OW_ACTIONS);
                if ( req.value().binary() ){
                    path /= "archive.tgz";
                    // Pipe File Descriptors
                    int downstream[2] = {};
                    int upstream[2] = {};

                    base64extract(path.string(), downstream, upstream, req);
                    tar_extract(path.string(), downstream, upstream);
                    std::filesystem::remove(path);
                    for ( auto pair: req.value().env() ){
                        if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0 ){
                            perror("Exporting environment variable failed.");
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
                    // Default file entrypoint is called main. i.e.: "main.lua", "main.js", "main.py".
                    std::string filename("main");
                    std::string ext;
                    if (__OW_ACTION_EXT == nullptr){
                        throw "Environment variable __OW_ACTION_EXT not defined.";
                    }
                    filename.append(".");
                    filename.append(__OW_ACTION_EXT);
                    path /= std::filesystem::path(filename);
                    std::ofstream file(path.string());
                    file << code;
                }
                return std::move(g);
            }
        };
        ctx_ptr->thread_controls().emplace_back();
        ctx_ptr->thread_controls().back().f() = std::move(f).resume();
        return ctx_ptr;
    }

    void base64extract(const std::string& filename, int pipefd_down[2], int pipefd_up[2], const Request& req){
        int wstatus = 0;
        //syscall return two pipes.
        if (pipe(pipefd_down) == -1){
            perror("Downstream pipe failed to open.");
        }
        if (pipe(pipefd_up) == -1){
            perror("Upstream pip failed to open.");
        }
        pid_t pid = fork();
        if (pid == 0){
            if( close(pipefd_down[1]) == -1 ){
                perror("closing the downstream write in the child process failed.");
            }
            if ( close(pipefd_up[0]) == -1 ){
                perror("closing the upstream read in the child process failed.");
            }
            if (close(pipefd_up[1]) == -1){
                perror("closing the upstream write in the child process faild.");
            }
            if (dup2(pipefd_down[0], STDIN_FILENO) == -1){
                perror("Failed to map the downstream read to STDIN.");
            }
            int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC);
            if (dup2(fd, STDOUT_FILENO) == -1){
                perror("Failed to map the filepath to STDOUT.");
            }
            std::vector<const char*> argv{"/usr/bin/base64", "-d", NULL};
            execve("/usr/bin/base64", const_cast<char* const*>(argv.data()), environ);
            exit(1);
        }
        if ( close(pipefd_up[1]) == -1 ){
            perror("Parent closing the upstream write failed.");
        }
        if(close(pipefd_up[0]) == -1){
            perror("Parent closing the upstream read failed.");
        }
        if ( close(pipefd_down[0]) == -1 ){
            perror("Parent closing the downstream read failed.");
        }
        int length = write(pipefd_down[1], req.value().code().data(), req.value().code().size());
        if ( length == -1 ){
            perror ("Write base64 encoded text to /usr/bin/base64 failed.");
        }
        if (close(pipefd_down[1]) == -1){
            perror("Parent closing the downstream write failed.");
        }
        // Block until base64 is done, but don't care about the return value since
        // the clean up will be handled first by trapping SIGCHLD.
        waitpid(pid,&wstatus,0);
    }

    void tar_extract(const std::string& filename, int pipefd_down[2], int pipefd_up[2]){
        int status = 0;
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        std::string fn_path;
        if ( __OW_ACTIONS != nullptr ){
            fn_path = std::string(__OW_ACTIONS);
        } else {
            throw "__OW_ACTIONS environment variable has not been defined.";
        }
        pid_t pid = fork();
        if ( pid == 0 ){
            std::vector<const char*> argv{"/usr/bin/tar", "-C", fn_path.c_str(), "-xf", filename.c_str(), NULL};
            execve("/usr/bin/tar", const_cast<char* const*>(argv.data()), environ);
            exit(1);               
        }
        // block until tar has completed, but don't really care about the return status.
        // since the waitpid will otherwise be handled by trapping SIGCHLD.
        waitpid(pid, &status, 0);
    }
}// namespace init
}// namespace resources
}// namespace controller