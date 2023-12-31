#include "execution-context.hpp"
#include <filesystem>
#include <fstream>
#include <boost/json.hpp>
#include "action-relation.hpp"
#include <charconv>

#ifdef OW_PROFILE
#include <iostream>
#include <ctime>
#endif

namespace controller{
namespace app{
    // Execution Context
    ExecutionContext::ExecutionContext(ExecutionContext::Init)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        execution_context_idx_array_{0},
        route_{controller::resources::Routes::INIT}
    {
        #ifdef OW_PROFILE
        start_ = std::chrono::steady_clock::now();
        #endif
    }

    ExecutionContext::ExecutionContext(ExecutionContext::Run, const std::map<std::string, std::string>& env)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        execution_context_idx_array_{0},
        route_{controller::resources::Routes::RUN},
        env_(env)
    {
        #ifdef OW_PROFILE
        start_ = std::chrono::steady_clock::now();
        #endif
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            std::cerr << "execution-context.cpp:26:__OW_ACTIONS not defined." << std::endl;
            throw "This shouldn't happen.";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            boost::json::object manifest;
            try{
                manifest = tmp.as_object();
            } catch(std::invalid_argument& e){
                std::cerr << "execution-context.cpp:52:tmp is not an object:" << boost::json::serialize(tmp) << std::endl;
                throw e;
            }
            if (ec){
                std::cerr << "execution-context.cpp:37:boost json parse failed." << std::endl;
                throw "This shouldn't happen.";
            }
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
                std::cerr << "execution-context.cpp:42:action-manifest.json can't be empty." << std::endl;
                throw "This shouldn't happen.";
            }
            /* If the action manifest contains an __OW_NUM_CONCURRENCY key, set the manifest concurrency to that value, otherwise set it to 1.*/
            // Subsequently erase the __OW_NUM_CONCURRENCY KEY from the ingested manifest to ensure that the subsequent logic continues to work.
            if(manifest.contains("__OW_NUM_CONCURRENCY")){
                if(manifest["__OW_NUM_CONCURRENCY"].is_int64()){
                    manifest_.concurrency() = manifest["__OW_NUM_CONCURRENCY"].get_int64();
                } else if (manifest["__OW_NUM_CONCURRENCY"].is_uint64()){
                    manifest_.concurrency() = manifest["__OW_NUM_CONCURRENCY"].get_uint64();
                } else {
                    std::cerr << "Manifest __OW_NUM_CONCURRENCY is too large, or is not an integer." << std::endl;
                }
                manifest.erase("__OW_NUM_CONCURRENCY");
            }

            // Loop through manifest.json until manifest_ contains the same number of keys.
            while(manifest_.size() < manifest.size()){
                // Find a key that isn't in the manifest_ yet.
                auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
                    auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
                        return rel->key() == kvp.key();
                    });
                    return (tmp != manifest_.end()) ? false : true;
                });
                // Insert it into the manifest, using a recursive tree traversal method.
                std::string key(it->key());
                manifest_.emplace(key, manifest); 
            }
            // Reverse lexicographically sort the manifest.
            std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
                return a->depth() > b->depth();
            });
        } else {
            const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
            if ( __OW_ACTION_EXT == nullptr ){
                std::cerr << "execution-context.cpp:78:__OW_ACTION_EXT envvar is not defined." << std::endl;
                throw "this shouldn't happen.";
            }
            // By default the file is called "main" + __OW_ACTION_EXT.
            // e.g. "main.lua", or "main.py", or "main.js".
            std::string filename("main");
            filename.append(".");
            filename.append(__OW_ACTION_EXT);
            std::filesystem::path fn_path(action_path / filename);

            const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
            std::string entrypoint;
            if ( __OW_ACTION_ENTRY_POINT == nullptr ){
                // By default, the entry point is called main.
                entrypoint = std::string("main");
            } else {
                entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
            }
            // By default, the entry point has no dependencies.
            manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
        }

        // Create a temporary directory for scripting convenience in /tmp/ACTIVATION_ID
        std::filesystem::path tmp_dir("/tmp");
        std::string __OW_ACTIVATION_ID = env.at("__OW_ACTIVATION_ID");
        tmp_dir /= __OW_ACTIVATION_ID;
        std::error_code err;
        if(!std::filesystem::create_directory(tmp_dir, err)){
            if(err){
                std::cerr << "execution-context.cpp:108:failed to create directory at /tmp/ACTIVATION_ID" << std::endl;
                throw err;
            }
        }
		std::stringstream ss;
		ss << execution_context_id_;
		env_["__OW_EXECUTION_CONTEXT_ID"] = ss.str();
        sync_counter_.store(manifest_.size(), std::memory_order::memory_order_relaxed);
    }

    ExecutionContext::ExecutionContext(ExecutionContext::Run, const UUID::Uuid& uuid, std::size_t idx, const std::vector<std::string>& peers, const std::map<std::string, std::string>& env)
      : execution_context_id_(uuid),
        execution_context_idx_stack_{idx},
        execution_context_idx_array_{idx},
        route_{controller::resources::Routes::RUN},
        env_(env)
    {
        #ifdef OW_PROFILE
        start_ = std::chrono::steady_clock::now();
        #endif
        /* Construct the peer table */
        for(auto& peer: peers){
            std::size_t pos = peer.find(':');
            std::string inet_addr_a = peer.substr(0,pos);
            std::string inet_port_a = peer.substr(pos+1, std::string::npos);

            std::uint16_t port;
            std::from_chars_result fcres = std::from_chars(inet_port_a.data(), inet_port_a.data()+inet_port_a.size(), port, 10);
            if(fcres.ec != std::errc()){
                std::cerr << "execution-context.cpp:116:std::from_chars failed:" << std::make_error_code(fcres.ec).message() << std::endl;
                throw "This shouldn't be possible";
            }

            struct sockaddr_in paddr;
            paddr.sin_family = AF_INET;
            paddr.sin_port = htons(port);
            int ec = inet_aton(inet_addr_a.c_str(), &paddr.sin_addr);
            if(ec == -1){
                std::cerr << "execution-context.cpp:125:inet_aton failed." << std::endl;
                throw "this shouldn't be possible";
            }

            server::Remote peer_inet_addr;
            peer_inet_addr.ipv4_addr = {
                SOCK_SEQPACKET,
                IPPROTO_SCTP,
                paddr
            };
            peers_.push_back(peer_inet_addr);
        }

        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            std::cerr << "execution-context.cpp:140:__OW_ACTIONS envvar not defined." << std::endl;
            throw "this shouldn't happen.";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            if (ec){
                std::cerr << "execution-context.cpp:150:" << ec.message() << std::endl;
                throw "boost json parse failed.";
            }
            boost::json::object manifest;
            try{
                manifest = tmp.as_object();
            }catch(std::system_error& e){
                std::cerr << "execution-context.cpp:196:tmp is not an object:" << boost::json::serialize(tmp) << std::endl;
                throw e;
            }
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
                std::cerr << "The action-manifest.json file can not be empty." << std::endl;
                throw "action-manifest.json can't be empty.";
            }
            /* If the action manifest contains an __OW_NUM_CONCURRENCY key, set the manifest concurrency to that value, otherwise set it to 1.*/
            // Subsequently erase the __OW_NUM_CONCURRENCY KEY from the ingested manifest to ensure that the subsequent logic continues to work.
            if(manifest.contains("__OW_NUM_CONCURRENCY")){
                if(manifest["__OW_NUM_CONCURRENCY"].is_int64()){
                    manifest_.concurrency() = manifest["__OW_NUM_CONCURRENCY"].get_int64();
                } else if (manifest["__OW_NUM_CONCURRENCY"].is_uint64()){
                    manifest_.concurrency() = manifest["__OW_NUM_CONCURRENCY"].get_uint64();
                } else {
                    std::cerr << "Manifest __OW_NUM_CONCURRENCY is too large, or is not an integer." << std::endl;
                    manifest_.concurrency() = 1;
                }
                manifest.erase("__OW_NUM_CONCURRENCY");
            } else {
                manifest_.concurrency() = 1;
            }
            // Loop through manifest.json until manifest_ contains the same number of keys.
            while(manifest_.size() < manifest.size()){
                // Find a key that isn't in the manifest_ yet.
                auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
                    auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
                        return rel->key() == kvp.key();
                    });
                    return (tmp != manifest_.end()) ? false : true;
                });
                // Insert it into the manifest, using a recursive tree traversal method.
                std::string key(it->key());
                manifest_.emplace(key, manifest);
            }
            // Reverse lexicographically sort the manifest.
            std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
                return a->depth() > b->depth();
            });
        } else {
            const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
            if ( __OW_ACTION_EXT == nullptr ){
                std::cerr << "execution-context.cpp:194:__OW_ACTION_EXT envvar is not defined." << std::endl;
                throw "Environment variable __OW_ACTION_EXT is not defined!";
            }
            // By default the file is called "main" + __OW_ACTION_EXT.
            // e.g. "main.lua", or "main.py", or "main.js".
            std::string filename("main");
            filename.append(".");
            filename.append(__OW_ACTION_EXT);
            std::filesystem::path fn_path(action_path / filename);

            const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
            std::string entrypoint;
            if ( __OW_ACTION_ENTRY_POINT == nullptr ){
                // By default, the entry point is called main.
                entrypoint = std::string("main");
            } else {
                entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
            }
            // By default, the entry point has no dependencies.
            manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
        }

        // Create a temporary directory for scripting convenience in /tmp/ACTIVATION_ID
        std::filesystem::path tmp_dir("/tmp");
        std::string __OW_ACTIVATION_ID = env.at("__OW_ACTIVATION_ID");
        tmp_dir /= __OW_ACTIVATION_ID;
        std::error_code err;
        if(!std::filesystem::create_directory(tmp_dir, err)){
            if(err){
                std::cerr << "execution-context.cpp:108:failed to create directory at /tmp/ACTIVATION_ID" << std::endl;
                throw err;
            }
        }
		std::stringstream ss;
		ss << execution_context_id_;
		env_["__OW_EXECUTION_CONTEXT_ID"] = ss.str();
        sync_counter_.store(manifest_.size(), std::memory_order::memory_order_relaxed);
    }

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs){
        return lhs.execution_context_id() == rhs.execution_context_id();
    }

    bool ExecutionContext::is_stopped() {
        for(auto& thread_control: thread_controls_){
            if (!(thread_control.is_stopped())){
                return false;
            }
        }
        return true;
    }

    void ExecutionContext::merge_peer_addresses(const std::vector<std::string>& remote_peers){
        for(auto& rpeer: remote_peers){
            std::size_t pos = rpeer.find(':', 0);
            if(pos == std::string::npos){
                std::cerr << "execution-context.cpp:234:can't find ':' in rpeer." << std::endl;
                throw "This should never happen.";
            }
            std::string rpip = rpeer.substr(0, pos);
            std::string rpport = rpeer.substr(pos+1, std::string::npos);
            std::uint16_t rpp;
            std::from_chars_result fcres = std::from_chars(rpport.data(), rpport.data()+rpport.size(), rpp, 10);
            if(fcres.ec != std::errc()){
                std::cerr << "execution-context.cpp:242:std::from_chars failed." << std::endl;
                throw "This should never happen.";
            }
            struct sockaddr_in raddr = {};
            raddr.sin_family = AF_INET;
            raddr.sin_port = htons(rpp);
            int ec = inet_aton(rpip.c_str(), &raddr.sin_addr);
            if(ec == 0){
                std::cerr << "execution-context.cpp:251:inet_aton failed." << std::endl;
                throw "This should never happen.";
            }
            auto it = std::find_if(peers_.cbegin(), peers_.cend(), [&](const auto& p){
                return (p.ipv4_addr.address.sin_port == raddr.sin_port && p.ipv4_addr.address.sin_addr.s_addr == raddr.sin_addr.s_addr);
            });
            if(it == peers_.cend()){
                server::Remote nrpeer;
                nrpeer.ipv4_addr = {
                    SOCK_SEQPACKET,
                    IPPROTO_SCTP,
                    raddr
                };
                peers_.push_back(std::move(nrpeer));
            }
        }
        return;
    }

    std::size_t ExecutionContext::pop_execution_idx() {
        if (execution_context_idx_stack_.empty()){
            std::cerr << "execution-context.cpp:272:execution context idx stack shouldn't be empty when calling pop." << std::endl;
            throw "Execution Context idx stack shouldn't be empty when calling pop.";
        }
        std::size_t idx = execution_context_idx_stack_.back();
        execution_context_idx_stack_.pop_back();
        return idx;
    }

    void ExecutionContext::push_execution_idx(std::size_t idx){
        execution_context_idx_stack_.push_back(idx);
        return;
    }

    ExecutionContext::~ExecutionContext() {
        // Cleanup the temporary directory associated to this execution context.
        if(route_ == controller::resources::Routes::RUN){
            std::string __OW_ACTIVATION_ID = env_.at("__OW_ACTIVATION_ID");
            std::filesystem::path tmp_dir("/tmp");
            tmp_dir /= __OW_ACTIVATION_ID;
            std::error_code err;
            std::filesystem::remove_all(tmp_dir, err);
            if(err){
                std::cerr << "execution-context.cpp:320:failed to remove temporary directory at /tmp/" << __OW_ACTIVATION_ID << std::endl;
            }
        }
    }
} //namespace app
} //namespace controller