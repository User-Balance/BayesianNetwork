/**
* @file adjacency_list.hpp
* @brief Implementation of Adjacency List using in network class
* @author godai_0519
* @date 09/09/2016
*/

#ifndef BAYESIAN_NETWORKS_NETWORK_ADJACENCY_LIST_HPP
#define BAYESIAN_NETWORKS_NETWORK_ADJACENCY_LIST_HPP

#include <algorithm> //find_if
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>
#include <bayesian/network/component.hpp>
#include <bayesian/network/traits.hpp>

namespace bn {

//! Implement class of adjacency list for bn::network.
class adjacency_list {
public:
    using this_type = adjacency_list;
    using node_ptr = component::node_ptr;
    using arc_ptr = component::arc_ptr;
    using node_const_ptr = traits::add_const_shared_t<component::node_ptr>;
    using arc_const_ptr = traits::add_const_shared_t<component::arc_ptr>;

    using stored_node_type = std::list<node_ptr>;
    using stored_arc_type = std::list<arc_ptr>;

    using endpoint_dictionary_type =
        std::unordered_map<
            arc_const_ptr,
            std::pair<
                node_const_ptr,
                node_const_ptr>>;

    using adjacency_type =
        std::unordered_map<
            node_const_ptr,
            std::list<
                std::pair<
                    node_const_ptr,
                    arc_const_ptr>>>;

    //! (Default ctor) Initialize nodes and arcs as empty.
    adjacency_list() = default;

    //! (Copy ctor) Initialize nodes and arcs by copying a parameter.
    /*!        The parameter will "not" be destroyed (still valid). */
    adjacency_list(this_type const&) = default;

    //! (Move ctor) Initialize nodes and arcs by moving a parameter.
    /*!        The parameter will be destroyed. */
    adjacency_list(this_type&&) = default;

    //! (Default dtor)
    virtual ~adjacency_list() = default;

    //! (Copy operator=) Initialize nodes and arcs by copying a parameter.
    /*! The parameter will "not" be destroyed (still valid). */
    this_type& operator=(this_type const& rhs)
    {
        *this = adjacency_list{ rhs };
        return *this;
    }

    //! (Move operator=) Initialize nodes and arcs by copying a parameter.
    /*! The parameter will be destroyed. */
    this_type& operator=(this_type&&) = default;

    //! Register a node into network.
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   node: Target node.
        @return      The node which is identical with the parameter.
    **/
    node_ptr add_node(node_ptr const& node)
    {
        stored_node_.push_back(node);
        try
        {
            adjacency_[node].clear();
        }
        catch(std::exception const&)
        {
            stored_node_.pop_back();
            throw;
        }

        return node;
    }

    //! Remove the argument node from network.
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   node: Target node.
        @return      true if the node was successfully removed; otherwise false.
    **/
    bool remove_node(node_ptr const& node) noexcept
    {
        auto it = std::find(stored_node_.crbegin(), stored_node_.crend(), node);
        if(it == stored_node_.crend()) return false; // Not registered

        // Backup for Strong Guarantee
        auto backup_stored_arc = stored_arc_;
        auto backup_endpoint_dic = endpoint_dic_;
        auto backup_adjacency = adjacency_;

        // Delete arcs connecting to the node
        bool is_error = false;
        for(auto const& arc : backup_stored_arc)
        {
            auto const it = endpoint_dic_.find(arc);
            if(it == endpoint_dic_.cend())
            {
                is_error = true;
                break;
            }
            if(it->second.first == node || it->second.second == node)
            {
                if(!this->remove_arc(arc))
                {
                    is_error = true;
                    break;
                }
            }
        }

        // Restore
        if(is_error)
        {
            stored_arc_ = std::move(backup_stored_arc);
            endpoint_dic_ = std::move(backup_endpoint_dic);
            adjacency_ = std::move(backup_adjacency);
            return false;
        }

        // Success
        adjacency_.erase(node);
        stored_node_.erase((++it).base());
        return true;
    }

    //! Register an arc into network.
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   arc: an arc.
        @param[in]   from: a node which is a start point of the arc.
        @param[in]   to: a node which is an end point of the arc.
        @return      The arc which is identical with the parameter.
    **/
    arc_ptr add_arc(arc_ptr const& arc, node_ptr const& from, node_ptr const& to)
    {
        auto node_pair = std::make_pair(from, to);
        auto arc_pair = std::make_pair(to, arc);

        stored_arc_.push_back(arc);
        try
        {
            endpoint_dic_[arc] = std::move(node_pair);
            try
            {
                adjacency_[from].push_back(std::move(arc_pair));
            }
            catch(std::exception const&)
            {
                endpoint_dic_.erase(arc);
                throw;
            }
        }
        catch(std::exception const&)
        {
            stored_arc_.pop_back();
            throw;
        }

        return arc;
    }

    //! Remove the argument arc from network.
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   arc: Target arc.
        @return      true if the node was successfully removed; otherwise false.
    **/
    bool remove_arc(arc_ptr const& arc) noexcept
    {
        auto it = endpoint_dic_.find(arc);
        if(it == endpoint_dic_.cend()) return false; // Not registered

        return remove_arc(arc, it->second.first, it->second.second);
    }

    //! Remove the argument arc from network.
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   from: a node which is a start point of the arc wanted to remove.
        @param[in]   to: a node which is an end point of the arc wanted to remove.
        @return      true if the node was successfully removed; otherwise false.
    **/
    bool remove_arc(node_ptr const& from, node_ptr const& to) noexcept
    {
        auto endpoint_it = std::find_if(
            endpoint_dic_.cbegin(), endpoint_dic_.cend(),
            [&from, &to](auto const& elem){
                        return elem.second.first == from && elem.second.second == to;
                    });
        if(endpoint_it == endpoint_dic_.cend()) return false; // Not registered

        return remove_arc(std::const_pointer_cast<component::arc>(endpoint_it->first), from, to);
    }

    //! Check whether two nodes are adjacent by a arc in the network.
    /*! No-throw guarantee: never throws exceptions.

        @param[in]   from: a node which is a start point.
        @param[in]   to: a node which is an end point.
        @return      true if from and to are adjacent; otherwise false.
    **/
    arc_ptr is_adjacent(node_ptr const& from, node_ptr const& to) const noexcept
    {
        auto const endpoint_it = std::find_if(
            endpoint_dic_.cbegin(), endpoint_dic_.cend(),
            [&from, &to](auto const& elem) {
                return elem.second.first == from && elem.second.second == to;
            });
        if(endpoint_it == endpoint_dic_.cend()) return nullptr; // Not registered

        return std::const_pointer_cast<component::arc>(endpoint_it->first);
    }

    //! Check whether a node and an arc are connected in the network.
    /*! No-throw guarantee: never throws exceptions.

        @param[in]   node: a node which is a start or end point.
        @param[in]   arc: an arc.
        @return      > 0 : the node is a start (parent) node of the arc;
                     < 0 : the node is an end (child) node of the arc;
                     = 0 : the node does not connect to the arc.
    **/
    int is_connect(node_ptr const& node, arc_ptr const& arc) const noexcept
    {
        auto const endpoint_it = endpoint_dic_.find(arc);
        if(endpoint_it == endpoint_dic_.cend()) return 0;

        return endpoint_it->second.first  == node ?  1 :
               endpoint_it->second.second == node ? -1 :
                                                     0 ;
    }

    //! Obtain a source (parent) node of an arc.
    /*! No-throw guarantee: never throws exceptions.

        @param[in]   arc: an arc
        @return      a source node pointer if it is exist; otherwise false
    **/
    node_ptr source(arc_ptr const& arc) const noexcept
    {
        auto it = endpoint_dic_.find(arc);
        if(it == endpoint_dic_.cend()) return nullptr;

        return std::const_pointer_cast<component::node>(it->second.first);
    }

    //! Obtain a target (child) node of an arc.
    /*! No-throw guarantee: never throws exceptions.

        @param[in]   arc: an arc.
        @return      a target node pointer if it is exist; otherwise false.
    **/
    node_ptr target(arc_ptr const& arc) const noexcept
    {
        auto it = endpoint_dic_.find(arc);
        if (it == endpoint_dic_.cend()) return nullptr;

        return std::const_pointer_cast<component::node>(it->second.second);
    }

    //! Obtain a list of all nodes which are parent of the child (argument).
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   child: a node which is an end point.
        @return      a list of all nodes.
    **/
    std::vector<node_ptr> parent_nodes(node_ptr const& child) const
    {
        std::vector<node_ptr> nodes;
        nodes.reserve(stored_node_.size());

        for(auto const& endpoint : endpoint_dic_)
        {
            if(endpoint.second.second == child)
                nodes.push_back(std::const_pointer_cast<component::node>(endpoint.second.first));
        }

        nodes.shrink_to_fit();
        return nodes;
    }

    //! Obtain a list of all nodes which are child of the parent (argument).
    /*! Strong guarantee: if an exception is thrown, there are no changes in the network.

        @param[in]   parent: a node which is an start point.
        @return      a list of all nodes.
    **/
    std::vector<node_ptr> child_nodes(node_ptr const& parent) const
    {
        std::vector<node_ptr> nodes;
        nodes.reserve(stored_node_.size());

        for(auto const& adjacent : adjacency_.at(parent))
            nodes.push_back(std::const_pointer_cast<component::node>(adjacent.first));

        nodes.shrink_to_fit();
        return nodes;
    }

    //! Obtain all node pointers.
    std::vector<node_ptr> all_node() const
    {
        return std::vector<node_ptr>(stored_node_.cbegin(), stored_node_.cend());
    }

    //! Obtain all arc pointers.
    std::vector<arc_ptr> all_arc() const
    {
        return std::vector<arc_ptr>(stored_arc_.cbegin(), stored_arc_.cend());
    }

private:
    bool remove_arc(arc_ptr const& arc, node_const_ptr const& from, node_const_ptr const& to) noexcept
    {
        auto arc_it = std::find(stored_arc_.crbegin(), stored_arc_.crend(), arc);
        if(arc_it == stored_arc_.crend()) return false; // Not registered

        auto from_it = adjacency_.find(from);
        if(from_it == adjacency_.cend()) return false; // Not registered

        auto& adjacency = from_it->second;
        auto adjacency_it = std::find(adjacency.cbegin(), adjacency.cend(), std::pair<node_const_ptr, arc_const_ptr>(to, arc));
        if (adjacency_it == adjacency.cend()) //? May I remove this 'if' statement?
            return false; // Not registered

        adjacency.erase(adjacency_it);
        endpoint_dic_.erase(arc);
        stored_arc_.erase((++arc_it).base());
        return true;
    }

    stored_node_type stored_node_;
    stored_arc_type stored_arc_;
    endpoint_dictionary_type endpoint_dic_;
    adjacency_type adjacency_;
};

} // namespace bn

#endif // BAYESIAN_NETWORKS_NETWORK_ADJACENCY_LIST_HPP
