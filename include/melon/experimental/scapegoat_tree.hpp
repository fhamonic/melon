#ifndef MELON_SCAPEGOAT_TREE_HPP
#define MELON_SCAPEGOAT_TREE_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>
#include <vector>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <float ALPHA, typename _Entry,
          typename _KeyComparator = std::less<_Entry>,
          input_mapping<_Entry> _EntryKeyMap = views::identity_map>
    requires std::strict_weak_order<_KeyComparator,
                                    mapped_value_t<_EntryKeyMap, _Entry>,
                                    mapped_value_t<_EntryKeyMap, _Entry>>
class scapegoat_tree {
public:
    using key_type = mapped_value_t<_EntryPriorityMap, _Entry>;
    // using mapped_type = V;
    using value_type = _Entry;
    // using size_type = std::size_t;
    // using difference_type = std::ptrdiff_t;

    // using reference = value_type;
    // using const_reference = const value_type;

    // using iterator = mapped_type *;
    // using const_iterator = const mapped_type *;
private:
    using node_idx_type = unsigned int;
    static constexpr node_idx_type INVALID_NODE =
        std::numeric_limits<node_idx_type>::max();
    struct node_type {
        node_idx_type left_child;
        node_idx_type right_child;
        node_idx_type parent;
        _Entry entry;
    };

private:
    std::vector<node_type> _nodes;

    node_idx_type _first_node;
    node_idx_type _first_free_node;
    node_idx_type _root;
    std::size_t _num_nodes;

    [[no_unique_address]] _KeyComparator _key_cmp;
    [[no_unique_address]] _EntryKeyMap _entry_key_map;

private:
    template <typename T>
    [[nodiscard]] constexpr node_idx_type new_node(T && v) noexcept {
        node_idx_type new_node;
        if(_first_free_node == INVALID_NODE) {
            new_node = static_cast<node_idx_type>(_nodes.size());
            _nodes.emplace_back(INVALID_NODE, _first_node, INVALID_NODE,
                                std::forward<T>(v));
        } else {
            new_node = _first_free_node;
            _first_free_node = _nodes[_first_free_node].right_child;
            _nodes[new_node] = {INVALID_NODE, _first_node, INVALID_NODE,
                                std::forward<T>(v)};
        }
        if(_first_node != INVALID_NODE) {
            _nodes[_first_node].left_child = new_node;
        }
        _first_node = new_node;
        ++_num_nodes;
        return new_node;
    }

    constexpr void delete_node(const node_idx_type n) noexcept {
        node_type & ns = _nodes[n];
        if(ns.right_child != INVALID_NODE) {
            _nodes[ns.right_child].left_child = ns.left_child;
        }
        if(ns.left_child != INVALID_NODE) {
            _nodes[ns.left_child].right_child = ns.right_child;
        } else {
            _first_node = ns.right_child;
        }
        ns.right_child = _first_free_node;
        _first_free_node = n;
        if constexpr(requires { ns.entry.~_Entry(); }) ns.entry.~_Entry();
        --_num_nodes;
    }

public:
    [[nodiscard]] constexpr scapegoat_tree() noexcept
        : _first_node(INVALID_NODE)
        , _first_free_node(INVALID_NODE)
        , _num_nodes(0) {};

    scapegoat_tree(const scapegoat_tree & other) = default;
    [[nodiscard]] constexpr scapegoat_tree(scapegoat_tree &&) = default;

    scapegoat_tree & operator=(const scapegoat_tree &) = delete;
    scapegoat_tree & operator=(scapegoat_tree &&) = default;

    constexpr std::size_t size() const { return _num_nodes; }
    // constexpr void clear() const {}

    template <typename CMP>
    node_idx_type find(const key_type & k, const CMP & cmp) {
        node_idx_type n = _root;
        while(n != INVALID_NODE) {
            const node_type & ns = _nodes[n];
            const key_type & other_key = _entry_key_map[ns.entry];
            if(cmp(k, other_key)) {
                n = ns.left;
                continue;
            }
            if(cmp(other_key, k)) {
                n = ns.right;
                continue;
            }
            break;
        }
        return n;
    }
    node_idx_type find(const key_type & k) { return find(k, _key_cmp); }

    node_idx_type erase(node_idx_type n) {
        node_type & ns = _nodes[n];
        if(ns.left_child != INVALID_NODE) {
            node_idx_type * child_ptr = &ns.left_child;
            while(_nodes[*child_ptr].right_child != INVALID_NODE) {
                child_ptr = &_nodes[*child_ptr].right_child;
            }
            node_idx_type u = *child_ptr;
            *child_ptr = INVALID_NODE;
            node_type & us = _nodes[u];
            us.left_child = ns.left_child;
            us.right_child = ns.right_child;
            us.parent = ns.parent;
            _nodes[us.left_child].parent = u;
        } else if(ns.right_child != INVALID_NODE) {
            node_idx_type * child_ptr = &ns.right_child;
            while(_nodes[*child_ptr].left_child != INVALID_NODE) {
                child_ptr = &_nodes[*child_ptr].left_child;
            }
            node_idx_type u = *child_ptr;
            *child_ptr = INVALID_NODE;
            node_type & us = _nodes[u];
            us.right_child = ns.right_child;
            us.left_child = ns.left_child;
            us.parent = ns.parent;
            _nodes[us.right_child].parent = u;
        } else {
            if(ns.parent == INVALID_NODE) _root = INVALID_NODE;
            if(_nodes[ns.parent].left_child == n) {_nodes[ns.parent].left_child = INVALID_NODE;}
            else {_nodes[ns.parent].right_child = INVALID_NODE;}
        }
        delete_node(n);
    }
    node_idx_type erase(const key_type & k) { erase(find(k)); }

    bool need_balancing(int height) {
        return height > std::log(_num_nodes) * std::log(ALPHA);
    }

    int insert(cosnt value_type & v) {
        const key_type & uk = _entry_key_map[v];
        node_idx_type u = new_node();
        const node_type & us = _nodes[u];
        node_idx_type w = _root;
        if(w == INVALID_NODE) {
            _root = u;
            return 0;
        }
        int d = 0;
        for(;;) {
            const node_type & ws = _nodes[w];
            const key_type & wk = _entry_key_map[ws];
            if(key_cmp(uk, wk)) {
                if(wk.left == INVALID_NODE) {
                    ws.left = u;
                    us.parent = w;
                    break;
                }
                w = ws.left;
                ++d;
                continue;
            }
            if(key_cmp(wk, uk)) {
                if(ws.right == INVALID_NODE) {
                    ws.right = u;
                    us.parent = w;
                    break;
                }
                w = ws.right;
                ++d;
                continue;
            }
            return -1;
        }
        return d;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_SCAPEGOAT_TREE_HPP