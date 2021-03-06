#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <session.hpp>
#include <list_cb.hpp>
#include <param.hpp>
#include <iostream>
namespace http {

/// \brief Routing module class
template<class Session>
class router
{

    using self = router<Session>;
    using list_cb_t = list_cb<boost::beast::http::request<typename Session::ReqBody>, Session>;
    using resource_map_t = boost::unordered_map<resource_regex_t,typename list_cb_t::ptr>;
    using method_map_t = std::map<method_t, resource_map_t>;

public:

    router(std::shared_ptr<resource_map_t> & resource_map_cb_p, std::shared_ptr<method_map_t> & method_map_cb_p) noexcept :
        resource_map_cb_p_{resource_map_cb_p},
        method_map_cb_p_{method_map_cb_p}
    {}

    router(router&&) noexcept = default;
    auto operator=(router&&) noexcept -> router& = default;

protected:

#ifndef __cpp_if_constexpr
    template<size_t v>
    struct size_type {
        static constexpr size_t value = v;
    };

    template <typename T, typename F1, typename F2, typename S = size_type<0> >
    struct tuple_cb_for_each
    {
        void operator()(const T& tpl, const F1& f1, const F2& f2){
            const auto& value = std::get<S::value>(tpl);
            f2(value);
            tuple_cb_for_each<T, F1, F2, size_type<S::value + 1> >{}(tpl, f1, f2);
        }
    };

    template <typename T, typename F1, typename F2>
    struct tuple_cb_for_each<T,F1,F2, size_type<std::tuple_size<T>::value - 1> >
    {
        void operator()(const T& tpl, const F1& f1, const F2& f2){
            const auto& value = std::get<size_type<std::tuple_size<T>::value - 1>::value>(tpl);
            f1(value);
            (void)f2;
        }
    };
#else

    template <typename T, typename F1, typename F2, size_t Index = 0>
    void tuple_cb_for_each(const T& tpl, const F1& f1, const F2& f2) {
        constexpr auto tuple_size = std::tuple_size_v<T>;
        const auto& value = std::get<Index>(tpl);
        if constexpr(Index + 1 == tuple_size){
            f1(value);
        }else{
            f2(value);
            tuple_cb_for_each<T, F1, F2, Index + 1>(tpl, f1, f2);
        }
    }
#endif

    template<class... Callback>
    auto prepare_list_cb(const Callback & ... handlers){

        typename list_cb_t::L cb_l;

        auto tuple_cb = std::tie(handlers...);

        constexpr auto tuple_size = std::tuple_size<decltype (tuple_cb) >::value;
        BOOST_STATIC_ASSERT(tuple_size != 0);

        const auto & f1 = [&cb_l](const auto& value){
            cb_l.push_back(
                        typename list_cb_t::F(
                            boost::bind<void>(
                                value,
                                boost::placeholders::_1,
                                boost::placeholders::_2
                                )
                            )
                        );
        };

        const auto & f2 = [&cb_l](const auto& value){
            cb_l.push_back(
                        typename list_cb_t::F(
                            boost::bind<void>(
                                value,
                                boost::placeholders::_1,
                                boost::placeholders::_2,
                                boost::placeholders::_3
                                )
                            )
                        );
        };

#ifndef __cpp_if_constexpr
        tuple_cb_for_each<decltype (tuple_cb), decltype (f1), decltype (f2)>{}
                (tuple_cb, f1, f2);
#else
        tuple_cb_for_each(tuple_cb, f1, f2);
#endif
        return cb_l;

    }

    void add_resource_cb(const resource_regex_t & path_to_resource, const method_t & method, list_cb_t && l){

        if(path_to_resource.empty()) // can not place callback with empty regex
            return;

        if(!method_map_cb_p_)
            method_map_cb_p_ = std::make_shared<method_map_t>();

        method_map_cb_p_->insert({
                                     method,
                                     resource_map_t()
                                 });

        auto & resource_map_cb = method_map_cb_p_->at(method);

        resource_map_cb.insert({
                                   path_to_resource,
                                   typename list_cb_t::ptr()
                               });

        auto & list_cb_p = resource_map_cb.at(path_to_resource);
        if(!list_cb_p)
            list_cb_p = std::make_shared<list_cb_t>();

        *list_cb_p = std::move(l);

    }

    void add_resource_cb_without_method(const resource_regex_t & path_to_resource, list_cb_t && l){

        if(path_to_resource.empty()) // can not place callback with empty regex
            return;

        if(!resource_map_cb_p_)
            resource_map_cb_p_ = std::make_shared<resource_map_t>();

        resource_map_cb_p_->insert({
                                       path_to_resource,
                                       typename list_cb_t::ptr()
                                   });

        auto & list_cb_p = resource_map_cb_p_->at(path_to_resource);
        if(!list_cb_p)
            list_cb_p = std::make_shared<list_cb_t>();

        *list_cb_p = std::move(l);

    }

    void use(const resource_regex_t & path_to_resource, const self & other){
        if(other.resource_map_cb_p_)
            for(const auto & value : *other.resource_map_cb_p_){
                auto l_cb = *value.second;
                add_resource_cb_without_method(path_to_resource + value.first, std::move(l_cb));
            }


        if(other.method_map_cb_p_)
            for(const auto & value_m : *other.method_map_cb_p_){
                auto method = value_m.first;
                const auto & resource_map = value_m.second;

                for(const auto & value_r : resource_map){
                    auto l_cb = *value_r.second;
                    add_resource_cb(path_to_resource + value_r.first, method,std::move(l_cb));
                }
            }
    }

    std::shared_ptr<resource_map_t> & resource_map_cb_p_;
    std::shared_ptr<method_map_t> & method_map_cb_p_;

}; // router class


template<class Session>
class basic_router : public router<Session>{

    using base_t = router<Session>;
    using self_t = basic_router<Session>;
    using list_cb_t = list_cb<boost::beast::http::request<typename Session::ReqBody>, Session>;
    using resource_map_t = boost::unordered_map<resource_regex_t,typename list_cb_t::ptr>;
    using method_map_t = std::map<method_t, resource_map_t>;

public:

    using ref = std::add_lvalue_reference_t<self_t>;
    using cref = std::add_lvalue_reference_t<std::add_const_t<self_t>>;

    basic_router() noexcept
        : base_t{resource_map_cb_p_, method_map_cb_p_}
    {}

    basic_router(std::shared_ptr<resource_map_t> & resource_map_cb_p, std::shared_ptr<method_map_t> & method_map_cb_p) noexcept
        : base_t(resource_map_cb_p, method_map_cb_p)
    {}

    /// Callback signature : template<class Message, class Session /*, class Next (optional)*/>
    ///                     void (Message & message, Session & session /*, Next & next (optional)*/)
    /// \brief Adds a handler for GET method
    template<class... Callback>
    void get(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::get, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for POST method
    template<class... Callback>
    void post(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::post, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for PUT method
    template<class... Callback>
    void put(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::put, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for HEAD method
    template<class... Callback>
    void head(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::head, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for DELETE method
    template<class... Callback>
    void delete_(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::delete_, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for OPTIONS method
    template<class... Callback>
    void options(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::options, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for CONNECT method
    template<class... Callback>
    void connect(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::connect, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for TRACE method
    template<class... Callback>
    void trace(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::trace, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // Webdav
    /// \brief Adds a handler for COPY method
    template<class... Callback>
    void copy(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::copy, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for LOCK method
    template<class... Callback>
    void lock(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::lock, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for MKCOL method
    template<class... Callback>
    void mkcol(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::mkcol, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for MOVE method
    template<class... Callback>
    void move(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::move, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for PROPFIND method
    template<class... Callback>
    void propfind(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::propfind, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for PROPPATCH method
    template<class... Callback>
    void proppatch(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::proppatch, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for SEARCH method
    template<class... Callback>
    void search(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::search, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for UNLOCK method
    template<class... Callback>
    void unlock(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::unlock, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for BIND method
    template<class... Callback>
    void bind(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::bind, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for REBIND method
    template<class... Callback>
    void rebind(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::rebind, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for UNBIND method
    template<class... Callback>
    void unbind(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::unbind, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for ACL method
    template<class... Callback>
    void acl(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::acl, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // subversion
    /// \brief Adds a handler for REPORT method
    template<class... Callback>
    void report(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::report, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for MKACTIVITY method
    template<class... Callback>
    void mkactivity(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::mkactivity, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for CHECKOUT method
    template<class... Callback>
    void checkout(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::checkout, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for MERGE method
    template<class... Callback>
    void merge(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::merge, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // upnp
    /// \brief Adds a handler for MSEARCH method
    template<class... Callback>
    void msearch(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::msearch, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for NOTIFY method
    template<class... Callback>
    void notify(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::notify, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for SUBSCRIBE method
    template<class... Callback>
    void subscribe(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::subscribe, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for UNSUBSCRIBE method
    template<class... Callback>
    void unsubscribe(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::unsubscribe, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // RFC-5789
    /// \brief Adds a handler for PATCH method
    template<class... Callback>
    void patch(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::patch, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for PURGE method
    template<class... Callback>
    void purge(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::purge, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // CalDAV
    /// \brief Adds a handler for MKCALENDAR method
    template<class... Callback>
    void mkcalendar(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::mkcalendar, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    // RFC-2068, section 19.6.1.2
    /// \brief Adds a handler for LINK method
    template<class... Callback>
    void link(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::link, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for UNLINK method
    template<class... Callback>
    void unlink(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb(path_to_resource, method_t::unlink, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }
    /// \brief Adds a handler for the requested resource by default
    /// \note If the handler for the requested resource with method is not found, this on is called
    template<class... Callback>
    void all(const resource_regex_t & path_to_resource, Callback && ... on_resource_handlers) & {
        base_t::add_resource_cb_without_method(path_to_resource, list_cb_t{base_t::prepare_list_cb(on_resource_handlers...)});
    }

    /// \brief Add external source of routes
    void use(const resource_regex_t & path_to_resource, const base_t & other){
        base_t::use(path_to_resource, other);
    }

    /// \brief Add external source of routes
    void use(const base_t & other){
        base_t::use("", other);
    }

    /// \brief Add a parametric process of route
    /// \tparam List of types output data
    template<class... Types>
    auto param(){
        return param_impl<Session, self_t, Types...>{*this};
    }

private:

    std::shared_ptr<resource_map_t> resource_map_cb_p_;
    std::shared_ptr<method_map_t> method_map_cb_p_;

}; //basic_router class



/// \brief Routing module class for chaining process
template<class Session>
class chain_router : public router<Session>{

    friend class chain_node;

    using base_t = router<Session>;
    using self_t = chain_router<Session>;
    using list_cb_t = list_cb<boost::beast::http::request<typename Session::ReqBody>, Session>;
    using resource_map_t = boost::unordered_map<resource_regex_t,typename list_cb_t::ptr>;
    using method_map_t = std::map<method_t, resource_map_t>;

public:

    using ref = std::add_lvalue_reference_t<self_t>;
    using cref = std::add_lvalue_reference_t<std::add_const_t<self_t>>;

    struct chain_node{

        using node_ref = std::add_lvalue_reference_t<chain_node>;

        explicit chain_node(ref router) noexcept
            : router_{router}
        {}

        /// \brief Adds a handler for GET method
        template<class... Callback>
        node_ref get(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::get, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for POST method
        template<class... Callback>
        node_ref post(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::post, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for PUT method
        template<class... Callback>
        node_ref put(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::put, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for HEAD method
        template<class... Callback>
        node_ref head(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::head, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for DELETE method
        template<class... Callback>
        node_ref delete_(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::delete_, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for OPTIONS method
        template<class... Callback>
        node_ref options(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::options, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for CONNECT method
        template<class... Callback>
        node_ref connect(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::connect, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for TRACE method
        template<class... Callback>
        node_ref trace(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::trace, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        //Webdav
        /// \brief Adds a handler for COPY method
        template<class... Callback>
        node_ref copy(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::copy, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for LOCK method
        template<class... Callback>
        node_ref lock(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::lock, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for MKCOL method
        template<class... Callback>
        node_ref mkcol(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::mkcol, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for MOVE method
        template<class... Callback>
        node_ref move(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::move, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for PROPFIND method
        template<class... Callback>
        node_ref propfind(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::propfind, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for PROPPATCH method
        template<class... Callback>
        node_ref proppatch(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::proppatch, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for SEARCH method
        template<class... Callback>
        node_ref search(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::search, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for UNLOCK method
        template<class... Callback>
        node_ref unlock(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::unlock, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for BIND method
        template<class... Callback>
        node_ref bind(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::bind, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for REBIND method
        template<class... Callback>
        node_ref rebind(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::rebind, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for UNBIND method
        template<class... Callback>
        node_ref unbind(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::unbind, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for ACL method
        template<class... Callback>
        node_ref acl(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::acl, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // subversion
        /// \brief Adds a handler for REPORT method
        template<class... Callback>
        node_ref report(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::report, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for MKACTIVITY method
        template<class... Callback>
        node_ref mkactivity(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::mkactivity, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for CHECKOUT method
        template<class... Callback>
        node_ref checkout(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::checkout, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for MERGE method
        template<class... Callback>
        node_ref merge(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::merge, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // upnp
        /// \brief Adds a handler for MSEARCH method
        template<class... Callback>
        node_ref msearch(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::msearch, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for NOTIFY method
        template<class... Callback>
        node_ref notify(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::notify, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for SUBSCRIBE method
        template<class... Callback>
        node_ref subscribe(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::subscribe, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for UNSUBSCRIBE method
        template<class... Callback>
        node_ref unsubscribe(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::unsubscribe, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // RFC-5789
        /// \brief Adds a handler for PATCH method
        template<class... Callback>
        node_ref patch(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::patch, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for PURGE method
        template<class... Callback>
        node_ref purge(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::purge, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // CalDAV
        /// \brief Adds a handler for MKCALENDAR method
        template<class... Callback>
        node_ref mkcalendar(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::mkcalendar, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // RFC-2068, section 19.6.1.2
        /// \brief Adds a handler for LINK method
        template<class... Callback>
        node_ref link(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::link, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

        /// \brief Adds a handler for UNLINK method
        template<class... Callback>
        node_ref unlink(Callback && ... on_resource_handlers) {
            router_.add_resource_cb(router_.tmp_res_regex_, method_t::unlink, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }
        // Ignored methods
        /// \brief Adds a handler for the requested resource by default
        template<class... Callback>
        node_ref all(Callback && ... on_resource_handlers) {
            router_.add_resource_cb_without_method(router_.tmp_res_regex_, list_cb_t{router_.prepare_list_cb(on_resource_handlers...)});
            return *this;
        }

    private:

        ref router_;

    };

    chain_router() noexcept
        : base_t{resource_map_cb_p_, method_map_cb_p_}
    {}

    explicit chain_router(std::shared_ptr<resource_map_t> & resource_map_cb_p, std::shared_ptr<method_map_t> & method_map_cb_p) noexcept
        : base_t{resource_map_cb_p, method_map_cb_p}
    {}

    /// \brief Returns an instance of a single route
    auto route(const resource_regex_t & path_to_resource) & {
        save_to_res_regex(path_to_resource);
        return chain_node{*this};
    }

    /// \brief Add external source of routes
    void use(const resource_regex_t & path_to_resource, const base_t & other){
        base_t::use(path_to_resource, other);
    }

    /// \brief Add external source of routes
    void use(const base_t & other){
        base_t::use("", other);
    }

    /// \brief Add a parametric process of route
    /// \tparam List of types output data
    template<class... Types>
    auto param(){
        return param_impl<Session, self_t, Types...>{*this};
    }

    /// \brief Save to to temporary regex resource
    inline void save_to_res_regex(const resource_regex_t & path_to_resource){
        tmp_res_regex_ = path_to_resource;
    }

private:

    // Temporary variable for storing a regular expression for implementing a chain task of routes
    resource_regex_t tmp_res_regex_;

    std::shared_ptr<resource_map_t> resource_map_cb_p_;
    std::shared_ptr<method_map_t> method_map_cb_p_;

}; // chain_router class

}

#endif // ROUTER_HPP
