#include "init.hpp"
#include <boost/context/fiber.hpp>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>

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
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>();
        boost::context::fiber f{
            [&, req, ctx_ptr](boost::context::fiber&& g){
                if ( req.value().binary() ){
                    std::filesystem::path archive("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/archive.tgz");
                    // Pipe File Descriptors
                    int downstream[2] = {};
                    int upstream[2] = {};

                    base64extract(archive.string(), downstream, upstream, req);
                    tar_extract(archive.string(), downstream, upstream);
                    std::filesystem::remove(archive);
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
                    std::filesystem::path function_path("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/fn_000.py");
                    std::ofstream file(function_path.string());
                    file << code;
                }
                return std::move(g);         
            }
        };
        ctx_ptr->fiber() = std::move(f);
        return ctx_ptr;
    }

    void base64extract(const std::string& filename, int pipefd_down[2], int pipefd_up[2], const Request& req){
        int status = 0;
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
            if (dup2(pipefd_down[0], STDIN_FILENO) == -1){
                perror("Failed to map the downstream read to STDIN.");
            }
            if (dup2(pipefd_up[1], STDOUT_FILENO) == -1){
                perror("Failed to map the upstream write to STDOUT.");
            }
            std::vector<const char*> argv{"/usr/bin/base64", "-d", NULL};
            execve("/usr/bin/base64", const_cast<char* const*>(argv.data()), environ);
            exit(1);
        } else {
            if ( close(pipefd_up[1]) == -1 ){
                perror("Parent closing the upstream write failed.");
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
            std::vector<char> binary(req.value().code().size());
            length = read(pipefd_up[0], binary.data(), binary.size());
            if ( length == -1 ){
                perror("Read decoded binary file from /usr/bin/base64 failed.");
            }
            if ( close(pipefd_up[0]) == -1){
                perror("Parent closing the read pipe failed.");
            }
            binary.resize(length);
            std::ofstream file(filename);
            file.write(binary.data(), binary.size());
            waitpid(pid, &status, 0);
            if ( status != 0 ){
                std::stringstream ss;
                ss << "base64 decode failed with status code: " << status << std::endl;
                throw ss.str();
            }
        }
    }

    void tar_extract(const std::string& filename, int pipefd_down[2], int pipefd_up[2]){
        int status = 0;
        std::string functions_path("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/");
        pid_t pid = fork();
        if ( pid == 0 ){
            std::vector<const char*> argv{"/usr/bin/tar", "-C", static_cast<const char*>(functions_path.c_str()), "-xf", static_cast<const char*>(filename.c_str()), NULL};
            execve("/usr/bin/tar", const_cast<char* const*>(argv.data()), environ);
            exit(1);               
        } else {
            waitpid(pid, &status, 0);
            if ( status != 0 ){
                std::stringstream ss;
                ss << "tar extraction failed with status code: " << status << std::endl;
                throw ss.str();
            }

        }
    }
}// namespace init
}// namespace resources
}// namespace controller