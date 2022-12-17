// Copyright (c) 2022 Mohammad nejati, Klemens D. Morgenstern, Ricahrd Hodges
//
// Distributed under the Boost Software License, Version 1.0

#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/post.hpp>

#include <mutex>
#include <variant>

namespace saf
{
namespace net = boost::asio;

enum class future_errc
{
    no_state = 1,
    unready_future,
    promise_already_satisfied,
    future_already_retrieved,
    broken_promise,
};
} // namespace saf

namespace std
{
template<>
struct is_error_code_enum<saf::future_errc> : true_type
{
};
} // namespace std

namespace saf
{
inline const std::error_category& future_category()
{
    static const struct : std::error_category
    {
        const char* name() const noexcept override
        {
            return "future";
        }

        std::string message(int ev) const override
        {
            switch (static_cast<future_errc>(ev))
            {
                case future_errc::no_state:
                    return "No associated state";
                case future_errc::unready_future:
                    return "Unready Future";
                case future_errc::promise_already_satisfied:
                    return "Promise already satisfied";
                case future_errc::future_already_retrieved:
                    return "Future already retrieved";
                case future_errc::broken_promise:
                    return "Broken promise";
                default:
                    return "Unknown error";
            }
        }
    } category;

    return category;
};

inline std::error_code make_error_code(future_errc e)
{
    return { static_cast<int>(e), future_category() };
}

class future_error : public std::system_error
{
  public:
    using std::system_error::system_error;
};

namespace detail
{
struct bilist_node
{
    bilist_node* next_{ this };
    bilist_node* prev_{ this };

    bilist_node() noexcept = default;

    bilist_node(bilist_node const&) = delete;

    bilist_node(bilist_node&& lhs) noexcept
        : next_(lhs.next_)
        , prev_(lhs.prev_)
    {
        lhs.next_ = &lhs;
        lhs.prev_ = &lhs;

        if (next_ == &lhs)
            next_ = this;
        else
            next_->prev_ = this;

        if (prev_ == &lhs)
            prev_ = this;
        else
            prev_->next_ = this;
    }

    void unlink() noexcept
    {
        next_->prev_ = prev_;
        prev_->next_ = next_;
    }

    void link_before(bilist_node* next) noexcept
    {
        next_        = next;
        prev_        = next->prev_;
        prev_->next_ = this;
        next->prev_  = this;
    }
};

class service_member : public bilist_node
{
  public:
    virtual void shutdown() noexcept = 0;
    virtual ~service_member()        = default;
};

template<bool IsMT>
class locking_strategy
{
    struct null_mutex
    {
        void lock()
        {
        }

        void unlock() noexcept
        {
        }

        bool try_lock()
        {
            return true;
        }
    };

    std::conditional_t<IsMT, std::mutex, null_mutex> mutex_;

  public:
    auto internal_lock() noexcept
    {
        return std::lock_guard{ mutex_ };
    }
};

template<bool IsMT>
class service final
    : public locking_strategy<IsMT>
    , public net::detail::execution_context_service_base<service<IsMT>>
{
    bilist_node entries_;

  public:
    explicit service(net::execution_context& ctx) noexcept
        : net::detail::execution_context_service_base<service>(ctx)
    {
    }

    void register_queue(bilist_node* sm) noexcept
    {
        auto lg = this->internal_lock();
        sm->link_before(&entries_);
    }

    void unregister_queue(bilist_node* sm) noexcept
    {
        auto lg = this->internal_lock();
        sm->unlink();
    }

    void shutdown() noexcept override
    {
        auto e  = std::move(entries_);
        auto nx = e.next_;
        while (nx != &e)
        {
            auto nnx = nx->next_;
            static_cast<service_member*>(nx)->shutdown();
            e.next_ = nx = nnx;
        }
    }
};

struct wait_op : bilist_node
{
    virtual void shutdown() noexcept                 = 0;
    virtual void complete(boost::system::error_code) = 0;
    virtual ~wait_op()                               = default;
};

template<class Executor, class Handler>
class wait_op_model final : public wait_op
{
    net::executor_work_guard<Executor> work_guard_;
    Handler handler_;

  public:
    wait_op_model(Executor e, Handler handler)
        : work_guard_(std::move(e))
        , handler_(std::move(handler))
    {
    }

    [[nodiscard]] auto get_cancellation_slot() const noexcept
    {
        return net::get_associated_cancellation_slot(handler_);
    }

    static wait_op_model* construct(Executor e, Handler handler)
    {
        auto halloc  = net::get_associated_allocator(handler);
        auto alloc   = typename std::allocator_traits<decltype(halloc)>::template rebind_alloc<wait_op_model>(halloc);
        using traits = std::allocator_traits<decltype(alloc)>;
        auto pmem    = traits::allocate(alloc, 1);

        try
        {
            return new (pmem) wait_op_model(std::move(e), std::move(handler));
        }
        catch (...)
        {
            traits::deallocate(alloc, pmem, 1);
            throw;
        }
    }

    static void destroy(wait_op_model* self, net::associated_allocator_t<Handler> halloc)
    {
        auto alloc = typename std::allocator_traits<decltype(halloc)>::template rebind_alloc<wait_op_model>(halloc);
        self->~wait_op_model();
        auto traits = std::allocator_traits<decltype(alloc)>();
        traits.deallocate(alloc, self, 1);
    }

    virtual void complete(boost::system::error_code ec)
    {
        get_cancellation_slot().clear();
        auto g = std::move(work_guard_);
        auto h = std::move(handler_);
        this->unlink();
        destroy(this, net::get_associated_allocator(h));
        net::post(g.get_executor(), net::append(std::move(h), ec));
    }

    virtual void shutdown() noexcept
    {
        get_cancellation_slot().clear();
        this->unlink();
        destroy(this, net::get_associated_allocator(this->handler_));
    }
};

template<typename T, typename Executor, bool IsMT>
class state final
    : public locking_strategy<IsMT>
    , public service_member
{
    Executor exec_;
    service<IsMT>* service_;
    bilist_node waiters_;
    std::variant<std::monostate, std::exception_ptr, std::conditional_t<std::is_void_v<T>, std::monostate, T>> value_;

  public:
    explicit state(Executor e)
        : exec_{ std::move(e) }
        , service_{ &net::use_service<service<IsMT>>({ net::query(exec_, net::execution::context) }) }
    {
        service_->register_queue(this);
    }

    state(const state&)            = delete;
    state& operator=(const state&) = delete;

    state(state&&)            = delete;
    state& operator=(state&&) = delete;

    [[nodiscard]] Executor get_executor() const
    {
        return exec_;
    }

    [[nodiscard]] bool is_ready() const
    {
        return value_.index() != 0;
    }

    template<typename... Args>
    void set_value(Args&&... args)
    {
        if (is_ready())
            throw future_error{ future_errc::promise_already_satisfied };

        value_.template emplace<2>(std::forward<Args>(args)...);

        complete_all({});
    }

    decltype(auto) get()
    {
        if (!is_ready())
            throw future_error{ future_errc::unready_future };

        if (auto* p = std::get_if<2>(&value_))
        {
            if constexpr (std::is_same_v<T, void>)
            {
                return;
            }
            else
            {
                return *p;
            }
        }

        std::rethrow_exception(std::get<1>(value_));
    }

    decltype(auto) get() const
    {
        if (!is_ready())
            throw future_error{ future_errc::unready_future };

        if (auto* p = std::get_if<2>(&value_))
        {
            if constexpr (std::is_same_v<T, void>)
            {
                return;
            }
            else
            {
                return std::as_const(*p);
            }
        }

        std::rethrow_exception(std::get<1>(value_));
    }

    void set_exception(std::exception_ptr exception_ptr)
    {
        if (is_ready())
            throw future_error{ future_errc::promise_already_satisfied };

        value_.template emplace<1>(exception_ptr);

        complete_all({});
    }

    template<typename CompletionToken>
    auto async_wait(CompletionToken&& token)
    {
        return net::async_initiate<decltype(token), void(boost::system::error_code)>(
            [this](auto handler)
            {
                auto e = get_associated_executor(handler, exec_);

                if (is_ready())
                    return net::post(std::move(e), net::append(std::move(handler), boost::system::error_code{}));

                using handler_type = std::decay_t<decltype(handler)>;
                using model_type   = wait_op_model<decltype(e), handler_type>;
                model_type* model  = model_type ::construct(std::move(e), std::forward<decltype(handler)>(handler));
                auto slot          = model->get_cancellation_slot();
                if (slot.is_connected())
                {
                    slot.assign(
                        [model, this](net::cancellation_type type)
                        {
                            if (type != net::cancellation_type::none)
                            {
                                auto lg = this->internal_lock();
                                model->complete(net::error::operation_aborted);
                            }
                        });
                }
                model->link_before(&waiters_);
            },
            token);
    }

    void complete_all(boost::system::error_code ec)
    {
        auto& nx = waiters_.next_;
        while (nx != &waiters_)
            static_cast<wait_op*>(nx)->complete(ec);
    }

    void shutdown() noexcept override
    {
        auto& nx = waiters_.next_;
        while (nx != &waiters_)
            static_cast<wait_op*>(nx)->shutdown();
    }

    ~state()
    {
        service_->unregister_queue(this);
    }
};

template<typename T, typename Executor, bool IsMT>
class shared_future
{
    std::shared_ptr<state<T, Executor, IsMT>> state_;

  public:
    shared_future() = default;

    explicit shared_future(std::shared_ptr<state<T, Executor, IsMT>> state) noexcept
        : state_{ std::move(state) }
    {
    }

    [[nodiscard]] Executor get_executor() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        return state_->get_executor();
    }

    [[nodiscard]] bool is_valid() const noexcept
    {
        return !!state_;
    }

    [[nodiscard]] bool is_ready() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return state_->is_ready();
    }

    decltype(auto) get() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return std::as_const(*state_).get();
    }

    template<typename CompletionToken>
    auto async_wait(CompletionToken&& token)
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return state_->async_wait(std::forward<CompletionToken>(token));
    }
};

template<typename T, typename Executor, bool IsMT>
class future
{
    std::shared_ptr<state<T, Executor, IsMT>> state_;

  public:
    future() = default;

    explicit future(std::shared_ptr<state<T, Executor, IsMT>> state) noexcept
        : state_{ std::move(state) }
    {
    }

    future(const future&)            = delete;
    future& operator=(const future&) = delete;

    future(future&&)            = default;
    future& operator=(future&&) = default;

    [[nodiscard]] Executor get_executor() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        return state_->get_executor();
    }

    [[nodiscard]] bool is_valid() const noexcept
    {
        return !!state_;
    }

    [[nodiscard]] bool is_ready() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return state_->is_ready();
    }

    decltype(auto) get()
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return state_->get();
    }

    [[nodiscard]] shared_future<T, Executor, IsMT> share() noexcept
    {
        return shared_future<T, Executor, IsMT>{ std::move(state_) };
    }

    template<typename CompletionToken>
    auto async_wait(CompletionToken&& token)
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        return state_->async_wait(std::forward<CompletionToken>(token));
    }
};

template<typename T, typename Executor, bool IsMT>
class promise
{
    std::shared_ptr<state<T, Executor, IsMT>> state_;
    bool retrieved_{ false };

  public:
    promise() = default;

    explicit promise(Executor exec)
        : state_{ std::make_shared<state<T, Executor, IsMT>>(std::move(exec)) }
    {
    }

    template<typename ExecutionContext>
    explicit promise(ExecutionContext& ctx, std::enable_if_t<std::is_convertible_v<ExecutionContext&, net::execution_context&>>* = nullptr)
        : promise{ ctx.get_executor() }
    {
    }

    template<typename Alloc>
    promise(Executor exec, const Alloc& alloc)
        : state_{ std::allocate_shared<state<T, Executor, IsMT>, Alloc>(alloc, std::move(exec)) }
    {
    }

    template<typename ExecutionContext, typename Alloc>
    explicit promise(ExecutionContext& ctx, const Alloc& alloc, std::enable_if_t<std::is_convertible_v<ExecutionContext&, net::execution_context&>>* = nullptr)
        : promise{ ctx.get_executor(), alloc }
    {
    }

    promise(const promise&)            = delete;
    promise& operator=(const promise&) = delete;

    promise(promise&&)            = default;
    promise& operator=(promise&&) = default;

    [[nodiscard]] Executor get_executor() const
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        return state_->get_executor();
    }

    [[nodiscard]] bool is_valid() const noexcept
    {
        return !!state_;
    }

    template<typename... Args>
    void set_value(Args&&... args)
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        state_->set_value(std::forward<Args>(args)...);
    }

    void set_exception(std::exception_ptr exception_ptr)
    {
        if (!state_)
            throw future_error{ future_errc::no_state };

        auto lg = state_->internal_lock();

        state_->set_exception(exception_ptr);
    }

    [[nodiscard]] future<T, Executor, IsMT> get_future()
    {
        if (std::exchange(retrieved_, true))
            throw future_error{ future_errc::future_already_retrieved };

        if (!state_)
            throw future_error{ future_errc::no_state };

        return future<T, Executor, IsMT>{ state_ };
    }

    ~promise()
    {
        if (state_)
        {
            auto lg = state_->internal_lock();
            if (!state_->is_ready())
                state_->set_exception(std::make_exception_ptr(future_error{ future_errc::broken_promise }));
        }
    }
};
} // namespace detail
struct mt
{
    template<typename T, typename Executor = net::any_io_executor>
    using future = detail::future<T, Executor, true>;

    template<typename T, typename Executor = net::any_io_executor>
    using shared_future = detail::shared_future<T, Executor, true>;

    template<typename T, typename Executor = net::any_io_executor>
    using promise = detail::promise<T, Executor, true>;
};

struct st
{
    template<typename T, typename Executor = net::any_io_executor>
    using future = detail::future<T, Executor, false>;

    template<typename T, typename Executor = net::any_io_executor>
    using shared_future = detail::shared_future<T, Executor, false>;

    template<typename T, typename Executor = net::any_io_executor>
    using promise = detail::promise<T, Executor, false>;
};
} // namespace saf
