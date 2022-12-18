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
