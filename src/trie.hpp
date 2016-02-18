#ifndef KAR_TRIE_HPP
#define KAR_TRIE_HPP

#include <map>
#include <utility>
#include <string>
#include <vector>
#include <numeric>
#include <memory>
#include "ordered_bounded_queue.hpp"

namespace kar
{

struct trie
{
    std::map<char, std::unique_ptr<trie>> children;
    typedef decltype(children) children_t;

    // equal to "" unless word completed here
    std::string word;
    bool completed { false };

    trie()
    : children(std::map<char, std::unique_ptr<trie>>())
    {}

    void clear()
    {
        children.clear();
    }

    void insert(const std::string & w)
    {
        const std::string word { std::string("$") + w };

        trie * n { this };
        for (std::size_t i = 0; i < word.length(); ++i) {
            const char c { word[i] };
            if (n->children.find(c) == n->children.end()) {
                n->children[c] = std::unique_ptr<trie>(new trie());
            }

            n = n->children[c].get();
        }

        n->word = word;
        n->completed = true;
    }

    void search_impl(
        trie * tree,
        ordered_bounded_queue<unsigned, std::string> & obqueue,
        const char ch,
        const std::vector<unsigned> & last_row,
        const std::string & word,
        const unsigned insert_cost,
        const unsigned delete_cost,
        const unsigned replace_cost,
        const unsigned max_cost
    ) {
        std::vector<unsigned> current_row(last_row.size());
        current_row[0] = last_row[0] + 1;

        for (std::size_t i = 1; i < last_row.size(); ++i) {
            const unsigned m_insert  { (current_row[i-1] + 1) * insert_cost };
            const unsigned m_delete  { (last_row[i]      + 1) * delete_cost };
            const unsigned m_replace { (word[i-1] == ch)
                ? last_row[i-1]
                : ((last_row[i-1] + 1) * replace_cost) };

            current_row[i] = std::min({
                m_insert,
                m_delete,
                m_replace
            });
        }

        // there is a word here
        if(tree->completed) {
            if(current_row[last_row.size() - 1] < max_cost) {
                obqueue.push(current_row[last_row.size() - 1], tree->word);
            }
        }


        // if any entries in the row are less than the maximum cost, then 
        // recursively search each branch of the trie
        if(*min_element(current_row.begin(), current_row.end()) < max_cost) {
            for(auto & it : tree->children) {
                search_impl(
                    it.second.get(),
                    obqueue,
                    it.first,
                    current_row,
                    word,
                    insert_cost,
                    delete_cost,
                    replace_cost,
                    max_cost
                );
            }
        }
    }

    std::vector<std::pair<unsigned, std::string>> search(
        const std::string & w,
        const std::size_t amnt,
        const unsigned insert_cost,
        const unsigned delete_cost,
        const unsigned replace_cost,
        const unsigned max_cost
    ) {
        const std::string word { std::string("$") + w };

        std::vector<unsigned> current_row(word.size() + 1);
        std::iota(current_row.begin(), current_row.end(), 0);

        current_row[word.size()] = word.size();
        ordered_bounded_queue<unsigned, std::string> obqueue(amnt);

        // ever letter in word in children we search
        for (std::size_t i = 0 ; i < word.size(); ++i) {
            if (children.find(word[i]) != children.end()) {
                search_impl(
                    children[word[i]].get(),
                    obqueue,
                    word[i],
                    current_row,
                    word,
                    insert_cost,
                    delete_cost,
                    replace_cost,
                    max_cost
                );
            }
        }

        return obqueue.data;
    }
};

}

#endif

