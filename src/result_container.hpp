#ifndef KAR_RESULTCONTAINER_HPP
#define KAR_RESULTCONTAINER_HPP

#include <string>
#include <vector>
#include <utility>
#include <cassert>

namespace kar
{

enum class result_type { NONE, STRING, UNSIGNED };

struct result_container
{
    result_type type;
    void * data;

    result_container(const result_container & rc)
    : type(rc.type)
    , data(rc.data)
    {}

    result_container()
    : type(result_type::NONE)
    , data(nullptr)
    {}

    result_container(const std::string & res)
    : type(result_type::STRING)
    , data(static_cast<void*>(new std::string(res)))
    {}

    result_container(const unsigned res)
    : type(result_type::UNSIGNED)
    , data(static_cast<void*>(new unsigned(res)))
    {}

    operator std::string()
    {
        assert(type == result_type::STRING);
        return *static_cast<std::string*>(data);
    }

    operator std::string() const
    {
        assert(type == result_type::STRING);
        return *static_cast<std::string*>(data);
    }

    operator unsigned()
    {
        assert(type == result_type::UNSIGNED);
        return *static_cast<unsigned*>(data);
    }

    operator unsigned() const
    {
        assert(type == result_type::UNSIGNED);
        return *static_cast<unsigned*>(data);
    }

    result_type get_type() const
    {
        return type;
    }

};

}

#endif

