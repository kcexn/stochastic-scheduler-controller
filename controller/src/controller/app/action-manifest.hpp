#ifndef ACTION_MANIFEST_HPP
#define ACTION_MANIFEST_HPP
#include <boost/json.hpp>
#include "action-relation.hpp"

namespace controller{
namespace app{
    class ActionManifest
    {
    public:
        ActionManifest();
        void emplace(const std::string& key, const boost::json::object& manifest);
        std::shared_ptr<Relation> next(const std::string& key, const std::size_t& idx);

        // Reexport the std::vector interface.
        std::vector<std::shared_ptr<Relation> >::iterator begin();
        std::vector<std::shared_ptr<Relation> >::iterator end();
        std::vector<std::shared_ptr<Relation> >::const_iterator cbegin();
        std::vector<std::shared_ptr<Relation> >::const_iterator cend();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rbegin();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rend();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crbegin();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crend();

        std::shared_ptr<Relation>& operator[](std::vector<std::shared_ptr<Relation> >::size_type pos);

        void push_back(const std::shared_ptr<Relation>& relation) { index_.push_back(relation); return; }
        void push_back(std::shared_ptr<Relation>&& relation) { index_.push_back(relation); return; }

        std::vector<std::shared_ptr<Relation> >::size_type size(){ return index_.size(); }
    private:
        std::vector<std::shared_ptr<Relation> > index_;
    };
}//namespace app
}//namespace controller
#endif