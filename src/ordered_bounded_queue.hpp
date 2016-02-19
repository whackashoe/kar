#ifndef KAR_ORDERED_BOUNDED_QUEUE_HPP
#define KAR_ORDERED_BOUNDED_QUEUE_HPP

#include <vector>
#include <limits>
#include <utility>

namespace kar
{

template <typename Comp, typename T>
struct ordered_bounded_queue
{
    std::vector<std::pair<Comp, T>> data;
    Comp min;
    Comp max;
    Comp size;

    ordered_bounded_queue(Comp size)
    : min(std::numeric_limits<Comp>::max())
    , max(std::numeric_limits<Comp>::max())
    , size(size)
    {}

    void push(const Comp distance, const T & s)
    {
        if(data.size() < size || distance < max) {
            if(data.size() == 0) {
                data.push_back(std::make_pair(distance, s));
                min = distance;
                max = distance;
            } else {
                for(auto it = data.begin(); it != data.end(); ++it) {
                    if(distance <= (*it).first) {
                        data.insert(it, std::make_pair(distance, T(std::begin(s) + 1, std::end(s))));
                        break;
                    }
                }

                if(data.size() > size) {
                    data.pop_back();
                }

                min = data[0].first;
                max = data[data.size() - 1].first;
            }
        }
    }
};

}

#endif

