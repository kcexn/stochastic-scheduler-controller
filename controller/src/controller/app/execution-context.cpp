#include "execution-context.hpp"
#include <filesystem>
#include <fstream>
#include <boost/json.hpp>
#include "action-relation.hpp"
#include <charconv>

namespace controller{
namespace app{
    // Execution Context
    ExecutionContext::ExecutionContext(ExecutionContext::Init init)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        execution_context_idx_array_{0},
        route_{controller::resources::Routes::INIT}
    {}

    ExecutionContext::ExecutionContext(ExecutionContext::Run run)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        execution_context_idx_array_{0},
        route_{controller::resources::Routes::RUN}
    {
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            throw "Environment variable __OW_ACTIONS not defined!";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            boost::json::object& manifest = tmp.as_object();
            if (ec){
                throw "boost json parse failed.";
            }
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
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
    }

    ExecutionContext::ExecutionContext(ExecutionContext::Run run, const UUID::Uuid& uuid, std::size_t idx, const std::vector<std::string>& peers)
      : execution_context_id_(uuid),
        execution_context_idx_stack_{idx},
        execution_context_idx_array_{idx},
        route_{controller::resources::Routes::RUN}
    {
        /* Construct the peer table */
        for(auto& peer: peers){
            std::size_t pos = peer.find(':');
            std::string inet_addr_a = peer.substr(0,pos);
            std::string inet_port_a = peer.substr(pos+1, std::string::npos);

            std::uint16_t port;
            std::from_chars_result fcres = std::from_chars(inet_port_a.data(), inet_port_a.data()+inet_port_a.size(), port, 10);
            if(fcres.ec != std::errc()){
                std::cerr << std::make_error_code(fcres.ec).message() << std::endl;
                throw "This shouldn't be possible";
            }

            struct sockaddr_in paddr;
            paddr.sin_family = AF_INET;
            paddr.sin_port = htons(port);
            int ec = inet_aton(inet_addr_a.c_str(), &paddr.sin_addr);
            if(ec == -1){
                perror("inet_aton");
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
            throw "Environment variable __OW_ACTIONS not defined!";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            if (ec){
                std::cerr << ec.message() << std::endl;
                throw "boost json parse failed.";
            }
            boost::json::object& manifest = tmp.as_object();
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
                throw "This should never happen.";
            }
            std::string rpip = rpeer.substr(0, pos);
            std::string rpport = rpeer.substr(pos+1, std::string::npos);
            std::uint16_t rpp;
            std::from_chars_result fcres = std::from_chars(rpport.data(), rpport.data()+rpport.size(), rpp, 10);
            if(fcres.ec != std::errc()){
                throw "This should never happen.";
            }
            struct sockaddr_in raddr ={
                AF_INET,
                htons(rpp)
            };
            int ec = inet_aton(rpip.c_str(), &raddr.sin_addr);
            if(ec == 0){
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
} //namespace app
} //namespace controller