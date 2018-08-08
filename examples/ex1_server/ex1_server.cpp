#include <iostream>

#include <server.hpp>

using namespace std;

template<class Request>
auto make_response(const Request & req, const string & user_body){

    boost::beast::http::string_body::value_type body(user_body);

    auto const body_size = body.size();

    boost::beast::http::response<boost::beast::http::string_body> res{
         std::piecewise_construct,
         std::make_tuple(std::move(body)),
         std::make_tuple(boost::beast::http::status::ok, req.version())};

    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.content_length(body_size);
    res.keep_alive(req.keep_alive());

    return res;

}

int main()
{
//g++ -c -std=gnu++17 -I.. -o ex1_server.o ./ex1_server.cpp
//g++ -o ex1_server ex1_server.o -lboost_system -lboost_thread -lpthread -lboost_regex -licui18n

//    root@x0x0:~# curl localhost --request 'GET' --request-target '/1'
//    root@x0x0:~# curl localhost --request 'GET' --request-target '/2'
    http::server my_http_server;

    my_http_server.get("/1", [](auto & req, auto & session){
       cout << req << endl;
       session.do_write(make_response(req, "GET 1\n"));
    });

    my_http_server.get("/2", [](auto & req, auto & session){
       cout << req << endl;
       session.do_write(make_response(req, "GET 2\n"));
    });

    my_http_server.all(".*", [](auto & req, auto & session){
        cout << req << endl;
        session.do_write(make_response(req, "error\n"));
    });

    const auto & address = "127.0.0.1";
    uint32_t port = 80;

    my_http_server.listen(address, port, [](auto & session){
        http::base::out("New client!!!");
        session.do_read();
    });

    cout << "Server starting on " << address << ':' << boost::lexical_cast<string>(port) << endl;

    uint32_t pool_size = boost::thread::hardware_concurrency();
    http::base::processor::get().start(pool_size == 0 ? 4 : pool_size << 1);
    http::base::processor::get().wait();

    return 0;
}