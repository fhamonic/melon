
namespace fhamonic {
namespace melon {

// TODO requires members
template <typename Algo>
struct node_search_span {
    struct end_iterator {};
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = decltype(std::declval<Algo>().processNextNode());
        using reference = value_type const &;
        using pointer = value_type *;

        iterator(Algo & alg) : algorithm(alg) {}
        iterator & operator++() noexcept {
            node = algorithm.processNextNode();
            return *this;
        }
        friend bool operator==(const iterator & it, end_iterator) noexcept {
            return it.algorithm.emptyQueue();
        }
        reference operator*() const noexcept { return node; }

    private:
        Algo & algorithm;

    public:
        value_type node;
    };

    iterator begin() {
        iterator it{algorithm};
        if(!algorithm.emptyQueue()) ++it;
        return ++it;
    }
    end_iterator end() noexcept { return {}; }

    node_search_span(node_search_span const &) = delete;

public:
    explicit node_search_span(Algo & alg) : algorithm(alg) {}
    Algo & algorithm;
};

}  // namespace melon
}  // namespace fhamonic