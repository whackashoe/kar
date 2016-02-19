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
#include <fstream>
#include <streambuf>
#include <random>
#include <unordered_map>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <boost/config/warning_disable.hpp>
#include <boost/network/include/http/server.hpp>
#include <boost/network/utils/thread_pool.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

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
// constants
constexpr char amnt_default[] { "10" };
constexpr char insc_default[] { "1" };
constexpr char delc_default[] { "1" };
constexpr char repc_default[] { "1" };
constexpr char maxc_default[] { "100" };
constexpr char special_default_word[] { "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ" };

// thread state
boost::mutex mtx;

// database container
struct database
{
    trie tree;
    std::unordered_map<std::string, std::size_t> ids;


    database()
    {
        tree.insert(special_default_word);
        ids[special_default_word] = 0;
    }
};
std::unordered_map<std::string, database> databases;


// settings
bool verbose { false };
std::size_t port          { 4334 };
std::string host          { "0.0.0.0" };
std::size_t thread_amount { 8 };
std::string password      { "secretpass" };



result_container render_error(const std::string & s, const std::string & piece)
{
    std::stringstream ss;
    ss  << s << " => " << piece << std::endl;

    const std::string rstr { ss.str() };

    if(verbose) {
        boost::lock_guard<boost::mutex> lock(mtx);
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
    database & db,
    const std::string & search_term,
    const std::size_t   amount,
    const unsigned      insert_cost,
    const unsigned      delete_cost,
    const unsigned      replace_cost,
    const unsigned      max_cost
) {
    const std::vector<std::pair<unsigned, std::string>> results { db.tree.search(
        search_term,
        amount,
        insert_cost,
        delete_cost,
        replace_cost,
        max_cost
    ) };

    json jres = json::array();
    for(const std::pair<std::size_t, std::string> & i : results) {
        const std::size_t id       { db.ids[i.second] };
        const std::size_t distance { i.first };

        // we skip the default id (special word)
        if(id == 0) {
            continue;
        }
        jres.push_back(json::object({{ "id", id }, { "d", distance }}));
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
        << "-d            : run as daemon" << std::endl
        << "-c FILE       : load config from file" << std::endl;
}

void display_version()
{
    std::cout << "kar" << KAR_VERSION << std::endl;
}

}

struct async_handler;
typedef boost::network::http::async_server<async_handler> server;

struct connection_handler : boost::enable_shared_from_this<connection_handler>
{
    server::connection_ptr conn;
    server::request const &req;
    std::map<std::string, std::string> qparams;
    std::string protocol, host, path, query;
    std::string body;
    std::vector<server::response_header> headers;
    bool password_authorized;

    connection_handler(server::connection_ptr conn, server::request const &request)
    : conn(conn)
    , req(request)
    , body("")
    , headers({
        server::response_header{"Connection",   "close"},
        server::response_header{"Content-Type", "application/json"},
        server::response_header{"X-Powered-By", "kar | kahuna ambiguity resolver"}})
    , password_authorized(false)
    {
        parse_destination(req.destination);

        // check for password authorization
        for(auto i : req.headers) {
            if(i.name == "X-Auth-Password" && i.value == password) {
                password_authorized = true;
            }
        }
    }

    void parse_destination(const std::string& destination) //with boost
    {
        const std::string url { std::string("http://") + host + ":" + std::to_string(port) + destination };


        const std::string prot_end("://");
        std::string::const_iterator prot_i = search(url.begin(), url.end(),
                                               prot_end.begin(), prot_end.end());
        protocol.reserve(std::distance(url.begin(), prot_i));
        std::transform(url.begin(), prot_i,
                  std::back_inserter(protocol),
                  std::ptr_fun<int,int>(std::tolower)); // protocol is icase

        if(prot_i == url.end()) {
            return;
        }

        std::advance(prot_i, prot_end.length());
        std::string::const_iterator pathi = std::find(prot_i, url.end(), '/');
        host.reserve(std::distance(prot_i, pathi));
        std::transform(prot_i, pathi,
                  std::back_inserter(host),
                  std::ptr_fun<int,int>(std::tolower)); // host is icase

        std::string::const_iterator queryi = std::find(pathi, url.end(), '?');
        path.assign(pathi, queryi);
        if(queryi != url.end()) {
            ++queryi;
        }
        query.assign(queryi, url.end());


        {
            std::istringstream iss(query);
            for (std::string chunk; std::getline(iss, chunk, '&'); ) {
                if(! chunk.empty()) {
                    std::istringstream iss_s(std::move(chunk));
                    std::vector<std::string> pieces;

                    for (std::string piece; std::getline(iss_s, piece, '='); ) {
                        pieces.push_back(std::move(piece));
                    }

                    if(pieces.size() == 2 && ! pieces[0].empty()) {
                        qparams[pieces[0]] = pieces[1];
                    }
                }
            }
        }
    }


    void err(const std::string & error)
    {
        json jres;
        jres["status"] = json::object();
        jres["status"]["success"] = false;
        jres["status"]["error"] = error;

        conn->set_status(server::connection::bad_request);
        conn->set_headers(boost::make_iterator_range(headers.begin(), headers.end()));
        conn->write(jres.dump());
    };

    std::string get_db_name_from_path() const
    {
        return path.substr(1, path.length());
    }

    void accept()
    {
        {
            const std::string db_name = get_db_name_from_path();
            if(databases.find(db_name) == databases.end()) {
                err("database_not_found");
                return;
            }
        }


        if(verbose) {
            boost::lock_guard<boost::mutex> lock(mtx);
            std::cout << req.method
                      << " " << req.source
                      << " " << get_db_name_from_path()
                      << std::endl;
        }

        if(req.method == "GET") {
            handle_get_request();
        } else if(req.method == "POST") {
            std::size_t cl { 0 };
            server::request::headers_container_type const &hs = req.headers;
            for(server::request::headers_container_type::const_iterator it = hs.begin(); it!=hs.end(); ++it) {
                if(boost::to_lower_copy(it->name)=="content-length") {
                    cl = boost::lexical_cast<int>(it->value);
                    break;
                }
            }

            read_body_chunk(cl);
        } else if(req.method == "DELETE") {
            handle_delete_request();
        } else {
            err("method_not_supported");
        }
    }

    void read_body_chunk(std::size_t left2read)
    {
        conn->read(
            boost::bind(
                &connection_handler::handle_post_read,
                connection_handler::shared_from_this(),
                _1, _2, _3, left2read
                )
            );
    }

    void handle_post_read(
        server::connection::input_range range,
        boost::system::error_code error,
        std::size_t size,
        std::size_t left2read
    ) {
        if(! error) {
            body.append(boost::begin(range), size);
            const std::size_t left { left2read - size };

            if(left > 0) {
                read_body_chunk(left);
            } else {
                handle_post_request();
            }
        } else {
            boost::lock_guard<boost::mutex> lock(mtx);
            std::cout << "error: " << error.message() << std::endl;
        }
    }

    void handle_post_request()
    {
        if(! password_authorized) {
            err("password_auth");
            return;
        }

        json req_body;
        try {
            req_body = json::parse(body);
        } catch(...) {
            std::cout << "json parse error, recieved: " << body.size() << std::endl;
            err("json_parse");
            return;
        }

        database & db = databases[get_db_name_from_path()];

        std::size_t amount_inserted { 0 };
        {
            boost::lock_guard<boost::mutex> lock(mtx);
            if(! req_body.is_object()) {
                err("not_json_object");
                return;
            }

            for(auto it = req_body.begin(); it != req_body.end(); ++it) {
                const auto k = it.key();
                const auto v = it.value();

                if(! v.is_number()) {
                    err("id_is_not_number");
                    return;
                }

                db.tree.insert(k);
                db.ids[k] = v;
                ++amount_inserted;
            }
        }

        json jres;
        jres["status"] = json::object();
        jres["status"]["success"] = true;

        conn->set_status(server::connection::ok);
        conn->set_headers(boost::make_iterator_range(headers.begin(), headers.end()));
        conn->write(jres.dump());
    }

    void handle_get_request()
    {
        if(qparams.find("q") == qparams.end()) {
            err("q_not_supplied");
            return;
        }

        const std::string search_term { qparams["q"] };

        auto find_or_default = [&](const std::string & key, const std::string & default_) {
            return qparams.find(key) == qparams.end() ? default_ : qparams[key];
        };

        const std::string s_amnt { find_or_default("amnt", std::string(amnt_default)) };
        const std::string s_insc { find_or_default("insc", std::string(insc_default)) };
        const std::string s_delc { find_or_default("delc", std::string(delc_default)) };
        const std::string s_repc { find_or_default("repc", std::string(repc_default)) };
        const std::string s_maxc { find_or_default("maxc", std::string(maxc_default)) };

        auto num_check = [](const std::string & s)
        {
            if(! is_number(s)) {
                return render_error("not a number", s);
            }

            try {
                const unsigned n = std::stoul(s); // todo: stoul is unsigned long
                return result_container(n);
            } catch(std::out_of_range & e) {
                return render_error("out of range => ", s);
            } catch(std::invalid_argument & e) {
                return render_error("invalid argument => ", s);
            } catch(std::exception & e) {
                return render_error("unknown exception encountered => ", s);
            }
        };

        const result_container amnt { num_check(s_amnt) }; if(amnt.get_type() != result_type::UNSIGNED) { err("notnum_amnt"); return; }
        const result_container insc { num_check(s_insc) }; if(insc.get_type() != result_type::UNSIGNED) { err("notnum_insc"); return; }
        const result_container delc { num_check(s_delc) }; if(delc.get_type() != result_type::UNSIGNED) { err("notnum_delc"); return; }
        const result_container repc { num_check(s_repc) }; if(repc.get_type() != result_type::UNSIGNED) { err("notnum_repc"); return; }
        const result_container maxc { num_check(s_maxc) }; if(maxc.get_type() != result_type::UNSIGNED) { err("notnum_maxc"); return; }

        database & db = databases[get_db_name_from_path()];

        json jres;
        jres["status"] = json::object();
        jres["status"]["success"] = true;
        jres["data"] = exec_query(db, search_term, amnt, insc, delc, repc, maxc);

        conn->set_status(server::connection::ok);
        conn->set_headers(boost::make_iterator_range(headers.begin(), headers.end()));
        conn->write(jres.dump());
    }

    void handle_delete_request()
    {
        if(! password_authorized) {
            err("password_auth");
            return;
        }

        database & db = databases[get_db_name_from_path()];

        {
            boost::lock_guard<boost::mutex> lock(mtx);
            db = database();
        }

        json jres;
        jres["status"] = json::object();
        jres["status"]["success"] = true;

        conn->set_status(server::connection::ok);
        conn->set_headers(boost::make_iterator_range(headers.begin(), headers.end()));
        conn->write(jres.dump());
    }
};


struct async_handler
{
    void operator()(server::request const &request, server::connection_ptr conn)
    {
        boost::shared_ptr<connection_handler> hand(new connection_handler(conn, request));
        hand->accept();
    }

    void error(boost::system::error_code const & ec)
    {
        std::cout << "async error: " << ec.message() << std::endl;
    }
};

bool load_config(const std::string & src)
{
    std::cout << "loading config from: " << src << std::endl;
    std::ifstream t(src);
    if(! t.is_open()) {
        std::cerr << "config file could not be opened" << std::endl;
        return false;
    }

    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
                std::istreambuf_iterator<char>());

    try {
        json c = json::parse(str);

        auto read_string_from_config = [&](const std::string & key, std::string & v)
        {
            if(c.find(key) != c.end()) {
                v = c[key];
            }
        };

        auto read_sizet_from_config = [&](const std::string & key, std::size_t & v)
        {
            if(c.find(key) != c.end()) {
                v = c[key];
            }
        };

        read_string_from_config("host", host);
        read_sizet_from_config("port", port);
        read_sizet_from_config("threads", thread_amount);
        if(thread_amount == 0) {
            thread_amount = boost::thread::hardware_concurrency();
            std::cout << "found " << thread_amount << " cores" << std::endl;

            if(thread_amount == 0) {
                std::cerr << "grabbing core count failed, defaulting thread_amount to 4" << std::endl;
                thread_amount = 4;
            }
        }

        read_string_from_config("password", password);
        if(c.find("password") == c.end()) {
            std::cerr << "password must be set" << std::endl;
            return false;
        }

        json db_names = c["databases"];
        for(const std::string & name : db_names) {
            databases[name] = database();
            std::cout << "database \"" << name << "\" created" << std::endl;
        }
    } catch(...) {
        std::cerr << "error reading from config" << std::endl;
        return false;
    }

    return true;
}

std::unique_ptr<server> instance;
std::vector<boost::thread> threads;

void join_server_threads()
{
    for(auto & i : threads) {
        i.join();
    }
}

void sig_handler(const int s)
{
    std::cout << "caught signal " << s << std::endl;
    instance->stop();
    join_server_threads();
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

        while ((c = getopt (argc, argv, "vVhdc:")) != -1) {
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

    if(! config_src.empty()) {
        if(! load_config(config_src)) {
            return EXIT_FAILURE;
        }
    } else {
        std::cerr << "no config file given" << std::endl;
        return EXIT_FAILURE;
    }

    if(daemonize) {
        if(daemon(0, 0) != 0) {
            std::cerr << "error during daemonization: " << errno << std::endl;
            return EXIT_FAILURE;
        }
    }


    async_handler handler;
    server::options options(handler);

    options.address(host).port(std::to_string(port))
        .thread_pool(boost::make_shared<boost::network::utils::thread_pool>(thread_amount));

    instance = std::unique_ptr<server>(new server(options));
    for(std::size_t i=0; i<thread_amount; ++i) {
        threads.push_back(boost::thread(boost::bind(&server::run, instance.get())));
    }

    std::cout << "kar started" << std::endl;
    std::cout << thread_amount << " threads listening on " << host << ":" << port << std::endl;

    instance->run();
    join_server_threads();

    return EXIT_SUCCESS;
}

