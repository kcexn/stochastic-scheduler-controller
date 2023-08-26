#include "init.hpp"
#include <boost/context/fiber.hpp>
#include <fstream>
#include <unistd.h>
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
            value_.env().emplace(key, val);
        }
      }

    Http::Response handle( Request& req ){
        Http::Response res = {};
        if ( req.value().binary() ){
            // binary files need to be tar.gz filse.
            std::string filename("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/archive.zip");
            int status=0;

            // Base64 decode using gnu coreutils base64
            int downstream[2] = {};
            int upstream[2] = {};

            //syscall return two pipes.
            if (pipe(downstream) == -1){
                perror("Downstream pipe failed to open.");
            }
            if (pipe(upstream) == -1){
                perror("Upstream pip failed to open.");
            }
            pid_t pid = fork();
            if (pid == 0){
                if( close(downstream[1]) == -1 ){
                    perror("closing the downstream write in the child process failed.");
                }
                if ( close(upstream[0]) == -1 ){
                    perror("closing the upstream read in the child process failed.");
                }
                if (dup2(downstream[0], STDIN_FILENO) == -1){
                    perror("Failed to map the downstream read to STDIN.");
                }
                if (dup2(upstream[1], STDOUT_FILENO) == -1){
                    perror("Failed to map the upstream write to STDOUT.");
                }
                std::vector<const char*> argv{"/usr/bin/base64", "-d", NULL};
                execve("/usr/bin/base64", const_cast<char* const*>(argv.data()), environ);
                exit(1);
            } else {
                if ( close(upstream[1]) == -1 ){
                    perror("Parent closing the upstream write failed.");
                }
                if ( close(downstream[0]) == -1 ){
                    perror("Parent closing the downstream read failed.");
                }
                int length = write(downstream[1], req.value().code().data(), req.value().code().size());
                if ( length == -1 ){
                    perror ("Write base64 encoded text to /usr/bin/base64 failed.");
                }
                if (close(downstream[1]) == -1){
                    perror("Parent closing the downstream write failed.");
                }
                std::vector<char> binary(req.value().code().size());
                length = read(upstream[0], binary.data(), binary.size());
                if ( length == -1 ){
                    perror("Read decoded binary file from /usr/bin/base64 failed.");
                }
                if ( close(upstream[0]) == -1){
                    perror("Parent closing the read pipe failed.");
                }
                binary.resize(length);
                std::ofstream file(filename);
                file.write(binary.data(), binary.size());
                // file.close();
                waitpid(pid, &status, 0);
                #ifdef DEBUG
                std::cout << "Base64 exit status: " << status << std::endl;
                #endif
            }

            // Tar extract files.
            pid = fork();
            if ( pid == 0 ){
                std::vector<const char*> argv{"/usr/bin/tar", "-C", "/workspaces/whisk-controller-dev/action-runtimes/python3/functions/", "-xf", "/workspaces/whisk-controller-dev/action-runtimes/python3/functions/archive.zip", NULL};
                execve("/usr/bin/tar", const_cast<char* const*>(argv.data()), environ);
                exit(1);               
            } else {
                waitpid(pid, &status, 0);
                #ifdef DEBUG
                std::cout << "tar exit status: " << status << std::endl;
                #endif
            }

            // Remove the compressed archive.
            if (remove(filename.c_str()) == -1){
                perror("file was not removed.");
            }

            #ifdef DEBUG
            std::cout << "Export Environment Variables." << std::endl;
            #endif

            for ( auto pair: req.value().env() ){
                std::string envvar(pair.first);
                envvar.append("=");
                envvar.append(pair.second);
                #ifdef DEBUG
                std::cout << "Envvar: " << envvar << std::endl;
                #endif
                if ( putenv(const_cast<char*>(envvar.c_str())) != 0 ){
                    perror("Exporting environment variable failed.");
                }
            }

            res = {
                .status_code = "200",
                .status_message = "OK",
                .connection = "close",
                .content_length = 0,
                .body = ""
            };
        } else {
            std::string code(req.value().code());
            std::size_t pos = 0;
            while ( (pos = code.find("\\n", pos)) != std::string::npos ){
                code.replace(pos, 2, "\n");
                ++pos;
            }

            std::string filename("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/fn_000.py");
            std::ofstream file(filename);
            file << code;
            res = {
                .status_code = "200",
                .status_message = "OK",
                .connection = "close",
                .content_length = 0,
                .body = ""
            };
        }
        return res;
    }
}// namespace init
}// namespace resources
}// namespace controller

