// Copyright (c) 2022 Mohammad Nejati
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include <saf.hpp>

#include <boost/asio.hpp>

#include <iostream>

namespace asio = boost::asio;

asio::awaitable<void> future_getter(saf::future<std::string> future)
{
    std::cout << "Waiting on the future...\n";
    co_await future.async_wait();
    std::cout << "The result: " << future.get() << '\n';
}

asio::awaitable<void> promise_setter(saf::promise<std::string> promise)
{
    auto timer = asio::steady_timer{ co_await asio::this_coro::executor };

    for (auto i = 1; i <= 3; i++)
    {
        timer.expires_after(std::chrono::seconds{ 1 });
        co_await timer.async_wait();
        std::cout << i << '\n';
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