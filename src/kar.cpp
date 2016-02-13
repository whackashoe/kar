#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <memory>
#include <regex>
#include <fstream>
#include <streambuf>


#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <boost/network/protocol/http/server.hpp>
#include <boost/regex.hpp>

#include "json.hpp"
#include "ordered_bounded_queue.hpp"
#include "result_container.hpp"
#include "trie.hpp"

#ifndef KAR_VERSION
#define KAR_VERSION "0.1-alpha"
#endif

using json = nlohmann::json;
using namespace kar;

namespace kar
{

trie tree;
std::map<std::string, std::size_t> ids;
bool verbose { false };
std::string port         { "4334" };
std::string host         { "0.0.0.0" };
std::string amnt_default { "10" };
std::string insc_default { "1" };
std::string delc_default { "1" };
std::string repc_default { "1" };
std::string maxc_default { "100" };


result_container render_error(const std::string & s, const std::string & piece)
{
    std::stringstream ss;
    ss  << s << " => " << piece << std::endl;

    const std::string rstr { ss.str() };

    if(verbose) {
        std::cerr << rstr;
    }

    return result_container(rstr);
}


bool is_number(const std::string & s)
{
    std::string::const_iterator it { s.begin() };

    while (it != s.end() && std::isdigit(*it)) {
        ++it;
    }
    return ! s.empty() && it == s.end();
}

std::string trim(const std::string & s)
{
    std::string ret { s };
    ret.erase(ret.begin(), std::find_if(ret.begin(), ret.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    ret.erase(std::find_if(ret.rbegin(), ret.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), ret.end());
    return ret;
}

json exec_query(
    const std::string & search_term,
    const std::size_t   amount,
    const unsigned      insert_cost,
    const unsigned      delete_cost,
    const unsigned      replace_cost,
    const unsigned      max_cost
) {
    const std::vector<std::pair<unsigned, std::string>> results { tree.search(
        search_term,
        amount,
        insert_cost,
        delete_cost,
        replace_cost,
        max_cost
    ) };

    json jres = json::array();
    for(auto & i : results) {
        jres.push_back(json::object({{ "id", ids[i.second] }, { "d", i.first }}));
    }

    return jres;
}

void display_usage()
{
    std::cout
        << "Usage: kar [OPTION]..." << std::endl
        << "Options and arguments:" << std::endl
        << "-h            : show this help" << std::endl
        << "-v            : show version" << std::endl
        << "-V            : set verbose" << std::endl
        << "-H host       : set host to listen on" << std::endl
        << "-P port       : set port to listen on" << std::endl
        << "-d            : run as daemon" << std::endl;
}

void display_version()
{
    std::cout << "kar" << KAR_VERSION << std::endl;
}

}

struct handler;
typedef boost::network::http::server<handler> http_server;

struct handler {
    std::map<std::string, std::string> parse_params(const std::string& destination) //with boost
    {
        const std::string url { std::string("http://") + host + ":" + port + destination };
        const std::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
        std::cmatch what;

        std::map<std::string, std::string> query_params;
        if(std::regex_match(url.c_str(), what, ex)) {
            const std::string query { std::string(what[5].first, what[5].second) };

            std::istringstream iss(query);
            for (std::string chunk; std::getline(iss, chunk, '&'); ) {
                if(! chunk.empty()) {
                    std::istringstream iss_s(std::move(chunk));
                    std::vector<std::string> pieces;
                    
                    for (std::string piece; std::getline(iss_s, piece, '='); ) {
                        pieces.push_back(std::move(piece));
                    }

                    if(pieces.size() == 2 && ! pieces[0].empty()) {
                        query_params[pieces[0]] = pieces[1];
                    }
                }
            }
        }

        return query_params;
    }

    void operator() (http_server::request const &request,
                     http_server::response &response)
    {
        auto err = [](const std::string & error)
        {
            json jres;
            jres["status"] = json::object();
            jres["status"]["success"] = false;
            jres["status"]["error"] = error;

            return http_server::response::stock_reply(http_server::response::bad_request, jres.dump());
        };

        auto num_check = [](const std::string & s)
        {
            if(! is_number(s)) {
                return render_error("not a number", s);
            }

            unsigned n;
            try {
                n = std::stoul(s);
            } catch(std::out_of_range & e) {
                return render_error("out of range => ", s);
            } catch(std::invalid_argument & e) {
                return render_error("invalid argument => ", s);
            } catch(std::exception & e) {
                return render_error("unknown exception encountered => ", s);
            }

            return result_container(n);
        };

        std::map<std::string, std::string> qparams { parse_params(request.destination) };

        if(request.method == "GET") {
            if(qparams.find("q") == qparams.end()) {
                response = err("q_not_supplied");
                return;
            }

            const std::string search_term    { qparams["q"] };
            const std::string s_amnt { qparams.find("amnt") == qparams.end() ? amnt_default : qparams["amnt"] };
            const std::string s_insc { qparams.find("insc") == qparams.end() ? insc_default : qparams["insc"] };
            const std::string s_delc { qparams.find("delc") == qparams.end() ? delc_default : qparams["delc"] };
            const std::string s_repc { qparams.find("repc") == qparams.end() ? repc_default : qparams["repc"] };
            const std::string s_maxc { qparams.find("maxc") == qparams.end() ? maxc_default : qparams["maxc"] };

            const result_container amnt { num_check(s_amnt) }; if(amnt.get_type() != result_type::UNSIGNED) { response = err("notnum_amnt"); return; }
            const result_container insc { num_check(s_insc) }; if(insc.get_type() != result_type::UNSIGNED) { response = err("notnum_insc"); return; }
            const result_container delc { num_check(s_delc) }; if(delc.get_type() != result_type::UNSIGNED) { response = err("notnum_delc"); return; }
            const result_container repc { num_check(s_repc) }; if(repc.get_type() != result_type::UNSIGNED) { response = err("notnum_repc"); return; }
            const result_container maxc { num_check(s_maxc) }; if(maxc.get_type() != result_type::UNSIGNED) { response = err("notnum_maxc"); return; }

            json jres;
            jres["status"] = json::object();
            jres["status"]["success"] = true;
            jres["data"] = exec_query(search_term, amnt, insc, delc, repc, maxc);

            response = http_server::response::stock_reply(http_server::response::ok, jres.dump());
            return;
        } else if(request.method == "POST" || request.method == "PATCH") {
            json req_body;
            try {
                req_body = json::parse(request.body);
            } catch(...) {
                response = err("json_parse");
                return;
            }

            if(! req_body.is_object()) {
                response = err("not_json_object");
                return;
            }

            if(request.method == "POST") {
                tree.clear();
                ids.clear();
            }

            std::size_t amount_inserted { 0 };
            for(auto it = req_body.begin(); it != req_body.end(); ++it) {
                const auto k = it.key();
                const auto v = it.value();

                if(! v.is_number()) {
                    response = err("id_is_not_number");
                    return;
                }

                tree.insert(k);
                ids[k] = v;
                ++amount_inserted;
            }

            json jres;
            jres["status"] = json::object();
            jres["status"]["success"] = true;

            if(verbose) {
                std::cout << amount_inserted << " items inserted (" << ids.size() << ")" << std::endl;
            }

            response = http_server::response::stock_reply(http_server::response::ok, jres.dump());
            return;
        } else {
            response = err("method_not_supported");
            return;
        }
    }

    void log(http_server::string_type const &info) {
        std::cerr << "ERROR: " << info << '\n';
    }
};

void load_config(const std::string & src)
{
    std::cout << "loading config from: " << src << std::endl;
    std::ifstream t(src);
    if(! t.is_open()) {
        std::cerr << "config file could not be opened" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string str;

    t.seekg(0, std::ios::end);   
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
                std::istreambuf_iterator<char>());

    try {
        json c = json::parse(str);

        auto read_from_config = [&](const std::string & key, std::string & v)
        {
            if(c.find(key) != c.end()) {
                v = c[key];
                std::cout << "config: " << key << " set" << std::endl;
            }
        };

        read_from_config("host", host);
        read_from_config("port", port);
        read_from_config("amnt", amnt_default);
        read_from_config("insc", insc_default);
        read_from_config("delc", delc_default);
        read_from_config("repc", repc_default);
        read_from_config("maxc", maxc_default);

    } catch(...) {
        std::cerr << "error reading from config" << std::endl;
    }
}

void sig_handler(const int s)
{
    std::cout << "caught signal " << s << std::endl;
    exit(1);
}


int main(int argc, char ** argv)
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);


    std::string config_src { "" };
    bool daemonize { false };
    {
        int c;

        while ((c = getopt (argc, argv, "vVhH:P:dc:")) != -1) {
            switch (c) {
            case 'v':
                display_version();
                return EXIT_SUCCESS;
                break;
            case 'V':
                verbose = true;
                break;
            case 'h':
                display_usage();
                return EXIT_SUCCESS;
                break;
            case 'H':
                host = optarg;
                break;
            case 'P':
                port = optarg;
                break;
            case 'd':
                daemonize = true;
                break;
            case 'c':
                config_src = optarg;
                break;
            case '?':
                std::cerr << "kar: invalid option" << std::endl;
                display_usage();
                return EXIT_FAILURE;
            default:
                abort();
            }
        }
    }

    std::cout << "kar started" << std::endl;

    if(! config_src.empty()) {
        load_config(config_src);
    }

    if(daemonize) {
        if(daemon(0, 0) != 0) {
            std::cerr << "error during daemonization: " << errno << std::endl;
            return EXIT_FAILURE;
        }
    }


    try {
        std::cout << "listening on " << host << ":" << port << std::endl;
        handler handler_;
        http_server::options options(handler_);
        http_server server_(options.address(host).port(port));
        server_.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

