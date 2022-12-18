## What is the saf?

**saf** is a single-header Asio based **scheduler aware future**/promise that does not block a whole thread if you wait for future. instead it cooperates with Asio's executor like other Asio based io_objects (e.g. asio::steady_timer).  
Because saf uses Asio's asynchronous model the wait operation on the future can be composed with other asynchronous operations and supports cancellation.  

### Quick usage

The latest version of the single header can be downloaded from [`include/saf.hpp`](include/saf.hpp).

**NOTE**

If you are using a stand-alone version of Asio you can define `SAF_ASIO_STANDALONE` before including `saf.hpp`.  
If you need thread-safe access to future/promise you can use their concurrent versions (`saf::cc_future`, `saf::cc_shared_future`, `saf::cc_promise`).  
```c++
#include <saf.hpp>

#include <boost/asio.hpp>
#include <fmt/format.h>

namespace asio = boost::asio;

asio::awaitable<void> future_getter(saf::future<std::string> future)
{
    fmt::print("Waiting on future ...\n");
    co_await future.async_wait(asio::deferred);
    fmt::print("Future value: {}\n", future.get());
}

asio::awaitable<void> promise_setter(saf::promise<std::string> promise)
{
    auto timer = asio::steady_timer{ co_await asio::this_coro::executor };

    for (auto i = 1; i <= 3; i++)
    {
        timer.expires_after(std::chrono::seconds{ 1 });
        co_await timer.async_wait(asio::deferred);
        fmt::print("{}\n", i);
    }

    promise.set_value("HOWDY!");
}

int main()
{
    auto ctx = asio::io_context{};

    auto promise = saf::promise<std::string>{ ctx };

    asio::co_spawn(ctx, future_getter(promise.get_future()), asio::detached);
    asio::co_spawn(ctx, promise_setter(std::move(promise)), asio::detached);

    ctx.run();
}
```
### API

#### Promise
```C++
template<typename T, typename Executor, bool CC>
class promise
{
  public:
    /// Default constructor.
    /**
     * This constructor creates a promise with an empty state.
     */
    promise() = default;

    /// Constructor.
    /**
     * @param exec The I/O executor that the future will use, by default, to
     * dispatch handlers for any asynchronous operations performed on the timer.
     */
    explicit promise(Executor exec);

    /// Constructor.
    /**
     * @param ctx An execution context that provides the I/O executor that the
     * future will use, by default, to dispatch handlers for any asynchronous
     * operations performed on the future.
     */
    template<typename ExecutionContext>
    explicit promise(ExecutionContext& ctx);

    /// Constructor.
    /**
     * @param exec The I/O executor that the future will use, by default, to
     * dispatch handlers for any asynchronous operations performed on the timer.
     *
     * @param alloc The allocator will be used for allocating sharing state
     * between promise and future
     */
    template<typename Alloc>
    promise(Executor exec, const Alloc& alloc);

    /// Constructor.
    /**
     * @param ctx An execution context that provides the I/O executor that the
     * future will use, by default, to dispatch handlers for any asynchronous
     * operations performed on the future.
     *
     * @param alloc The allocator will be used for allocating sharing state
     * between promise and future
     */
    template<typename ExecutionContext>
    explicit promise(ExecutionContext& ctx, const Alloc& alloc);

    promise(const promise&)            = delete;
    promise& operator=(const promise&) = delete;

    promise(promise&&)            = default;
    promise& operator=(promise&&) = default;

    /// Gets the executor associated with the object.
    /**
     * @return the executor associated with the object.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if promise does
     * not contain a state.
     */
    [[nodiscard]] Executor get_executor() const;

    /// Checks if the promise contains a state.
    /**
     * This function checks if the promise contains a state, a default
     * constructed, and a moved-from promise does not contain a state.
     *
     * @return true if the promise contains a state, otherwise false.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /// Sets a value in the state.
    /**
     * This function sets a value to be used by the future and signals all the
     * waiting handlers on the future side.
     *
     * @throws `future_error{ future_errc::promise_already_satisfied }` Thrown
     * if a value or an exception has already been set.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    template<typename... Args>
    void set_value(Args&&... args);

    /// Sets an exception in the state.
    /**
     * This function sets an exception to be used by the future and signals all
     * the waiting handlers on the future side.
     *
     * @throws `future_error{ future_errc::promise_already_satisfied }` Thrown
     * if a value or an exception has already been set.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    void set_exception(std::exception_ptr exception_ptr);

    /// Creates a future.
    /**
     * This function creates a future associated with the same state.
     *
     * @return an instance of the future.
     *
     * @throws `future_error{ future_errc::future_already_retrieved }` Thrown if
     * get_future() has already been called.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    [[nodiscard]] future<T, Executor, CC> get_future();

    /// Destroys the promise.
    /**
     * This function destroys the promise, if no value or exception is set an
     * exception of `future_error{ future_errc::broken_promise }` will be set in
     * the state and signal all the waiting handlers on the future side.
     */
    ~promise();
};
```

#### Future
```C++
template<typename T, typename Executor, bool CC>
class future
{
  public:
    /// Default constructor.
    /**
     * This constructor creates a future with an empty state.
     */
    future() = default;

    /// Constructor.
    /**
     * This constructor creates a future from a state. this constructor is only
     * used by promise side to create a future pair.
     */
    explicit future(std::shared_ptr<state<T, Executor, CC>> state) noexcept;

    future(const future&)            = delete;
    future& operator=(const future&) = delete;

    future(future&&)            = default;
    future& operator=(future&&) = default;

    /// Gets the executor associated with the object.
    /**
     * @return the executor associated with the object.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    [[nodiscard]] Executor get_executor() const;

    /// Checks if the future contains a state.
    /**
     * This function checks if the future contains a state, a default
     * constructed, moved-from and a future that is converted to a shared_future
     * does not contain a state.
     *
     * @return true if the future contains a state, otherwise false.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /// Checks if the result is ready.
    /**
     * This function checks if the promise side set a result or an exception in
     * the state.
     *
     * @return true if the result is ready, otherwise false.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    [[nodiscard]] bool is_ready() const;

    /// Returns the result.
    /**
     * This function returns the result or throws the exception that the promise
     * side set, this function should be called after async_wait completes,
     * otherwise `future_error{ future_errc::unready_future }` will be thrown.
     *
     * @return T& or void if T is void
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     *
     * @throws `future_error{ future_errc::unready_future }` Thrown on if future
     * is not ready yet.
     *
     * @throws stored exception if promise side set an exception.
     */
    decltype(auto) get();

    /// Creates a shared_future.
    /**
     * This function transfers the state of the future to an instance of
     * shared_future and leaves the future with an empty state (is_valid() on
     * future returns false after this function).
     *
     * @return an instance of shared_future.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    [[nodiscard]] shared_future<T, Executor, CC> share();

    /// Starts an asynchronous wait on the future.
    /**
     * This function initiates an asynchronous wait against the future and
     * always returns immediately.
     *
     * For each call to async_wait(), the completion handler will be called
     * exactly once. The completion handler will be called when:
     *
     * @li The promise sets a value.
     *
     * @li The promise sets an exception.
     *
     * @li The promise goes out of scope and an exception of `future_error{
     * future_errc::broken_promise }` set automatically.
     *
     * @li The future was canceled, in which case the handler is passed the
     * error code net::error::operation_aborted.
     *
     * @param token The completion_token that will be used to produce a
     * completion handler.
     */
    template<typename CompletionToken>
    auto async_wait(CompletionToken&& token);
};
```

#### Shared Future
```C++
template<typename T, typename Executor, bool CC>
class shared_future
{
  public:
    /// Default constructor.
    /**
     * This constructor creates a shared_future with an empty state.
     */
    shared_future() = default;

    /// Constructor.
    /**
     * This constructor creates a shared_future from a state. this constructor
     * is only used when share() method on future is called.
     */
    explicit shared_future(
        std::shared_ptr<state<T, Executor, CC>> state) noexcept;

    /// Gets the executor associated with the object.
    /**
     * @return the executor associated with the object.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if future does not
     * contain a state.
     */
    [[nodiscard]] Executor get_executor() const;

    /// Checks if the future contains a state.
    /**
     * This function checks if the future contains a state, a default
     * constructed, moved-from and a future that is converted to a shared_future
     * does not contain a state.
     *
     * @return true if the future contains a state, otherwise false.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /// Checks if the result is ready.
    /**
     * This function checks if the promise side set a result or an exception in
     * the state.
     *
     * @return true if the result is ready, otherwise false.
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if shared_future
     * does not contain a state.
     */
    [[nodiscard]] bool is_ready() const;

    /// Returns the result.
    /**
     * This function returns the result or throws the exception that the promise
     * side set, this function should be called after async_wait completes,
     * otherwise `future_error{ future_errc::unready_future }` will be thrown.
     *
     * @return const T& or void if T is void
     *
     * @throws `future_error{ future_errc::no_state }` Thrown if shared_future
     * does not contain a state.
     *
     * @throws `future_error{ future_errc::unready_future }` Thrown on if
     * shared_future is not ready yet.
     *
     * @throws stored exception if promise side set an exception.
     */
    decltype(auto) get() const;

    /// Starts an asynchronous wait on the shared_future.
    /**
     * This function initiates an asynchronous wait against the shared_future
     * and always returns immediately.
     *
     * For each call to async_wait(), the completion handler will be called
     * exactly once. The completion handler will be called when:
     *
     * @li The promise sets a value.
     *
     * @li The promise sets an exception.
     *
     * @li The promise goes out of scope and an exception of `future_error{
     * future_errc::broken_promise }` set automatically.
     *
     * @li The shared_future was canceled, in which case the handler is passed
     * the error code net::error::operation_aborted.
     *
     * @param token The completion_token that will be used to produce a
     * completion handler.
     */
    template<typename CompletionToken>
    auto async_wait(CompletionToken&& token);
};
```