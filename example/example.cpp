// Copyright (c) 2022 Mohammad nejati
//
// Distributed under the Boost Software License, Version 1.0

#include <saf.hpp>

#include <boost/asio.hpp>

#include <fmt/format.h>

namespace asio = boost::asio;

asio::awaitable<void> future_awaiter(saf::st::future<std::string> future)
{
    fmt::print("Waiting on future ...\n");
    co_await future.async_wait(asio::deferred);
    fmt::print("Future value: {}\n", future.get());
}

asio::awaitable<void> async_main()
{
    auto executor = co_await asio::this_coro::executor;
    auto promise  = saf::st::promise<std::string>{ executor };

    asio::co_spawn(executor, future_awaiter(promise.get_future()), asio::detached);

    auto timer = asio::steady_timer{ executor };
    for (int i = 1; i <= 3; i++)
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

    asio::co_spawn(ctx, async_main(), asio::detached);

    ctx.run();
}