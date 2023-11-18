#ifndef ACTION_RELATION_HPP
#define ACTION_RELATION_HPP
#include <filesystem>
#include <vector>
#include <mutex>

namespace controller{
namespace app{
    class Relation
    {
    public:
        Relation();
        Relation(const std::string& key, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies);
        Relation( std::string&& key, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies);

        Relation(const std::string& key, const std::string& value, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies);
        Relation(std::string&& key, std::string&& value, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies);

        const std::string& key() const { return kvp_.first; }
        std::string& acquire_value() { mtx_.lock(); return kvp_.second; }
        void release_value() { mtx_.unlock(); }
        std::size_t depth() const { return depth_; }
        const std::filesystem::path& path() const { return path_; }

        // Reexport the std::vector interface.
        std::vector<std::shared_ptr<Relation> >::iterator begin() { return dependencies_.begin(); }
        std::vector<std::shared_ptr<Relation> >::iterator end() { return dependencies_.end(); }
        std::vector<std::shared_ptr<Relation> >::const_iterator cbegin() { return dependencies_.cbegin(); }
        std::vector<std::shared_ptr<Relation> >::const_iterator cend() { return dependencies_.cend(); }
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rbegin() { return dependencies_.rbegin(); }
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rend() { return dependencies_.rend(); }
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crbegin() { return dependencies_.crbegin(); }
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crend() { return dependencies_.crend(); }

        std::shared_ptr<Relation>& operator[]( std::vector<std::shared_ptr<Relation> >::size_type pos ){ return dependencies_[pos]; }
        std::size_t size(){ return dependencies_.size(); }
    private:
        std::pair<std::string, std::string> kvp_;
        std::vector<std::shared_ptr<Relation> > dependencies_;
        std::size_t depth_;
        std::filesystem::path path_;
        std::mutex mtx_;
    };
}//namespace app
}//namesapce controller
#endif