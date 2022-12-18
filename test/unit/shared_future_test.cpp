// Copyright (c) 2022 Mohammad Nejati
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include <saf.hpp>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(shared_future)

namespace asio = boost::asio;

BOOST_AUTO_TEST_CASE(is_valid)
{
    auto ctx = asio::io_context{};

    auto sh_future = saf::promise<void>{ ctx }.get_future().share();
    BOOST_CHECK(sh_future.is_valid() == true);

    auto sh_future_2 = sh_future;
    BOOST_CHECK(sh_future.is_valid() == true);
    BOOST_CHECK(sh_future_2.is_valid() == true);

    auto sh_future_3 = saf::shared_future<void>{};
    BOOST_CHECK(sh_future_3.is_valid() == false);
}

BOOST_AUTO_TEST_CASE(get_excecutor)
{
    auto ctx = asio::io_context{};

    auto sh_future = saf::promise<void>{ ctx }.get_future().share();

    BOOST_CHECK_NO_THROW(boost::ignore_unused(sh_future.get_executor()));
    auto sh_future_2 = std::move(sh_future);
    BOOST_CHECK_EXCEPTION(boost::ignore_unused(sh_future.get_executor()), saf::future_error, [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(is_ready)
{
    auto ctx = asio::io_context{};

    auto promise   = saf::promise<void>{ ctx };
    auto sh_future = promise.get_future().share();

    BOOST_CHECK(sh_future.is_ready() == false);
    promise.set_value();
    BOOST_CHECK(sh_future.is_ready() == true);

    auto sh_future_2 = std::move(sh_future);
    BOOST_CHECK_EXCEPTION(boost::ignore_unused(sh_future.is_ready()), saf::future_error, [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(get_no_state)
{
    auto sh_future = saf::shared_future<int>{};
    BOOST_CHECK_EXCEPTION(sh_future.get(), saf::future_error, [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(get_unready)
{
    auto ctx       = asio::io_context{};
    auto promise   = saf::promise<int>{ ctx };
    auto sh_future = promise.get_future().share();

    BOOST_CHECK_EXCEPTION(sh_future.get(), saf::future_error, [](const auto& e) { return e.code() == saf::future_errc::unready_future; });
}

BOOST_AUTO_TEST_CASE(async_wait_pre_set_value)
{
    auto ctx         = asio::io_context{};
    auto promise     = saf::promise<int>{ ctx };
    auto sh_future_1 = promise.get_future().share();
    auto sh_future_2 = sh_future_1;
    promise.set_value(54);
    int invoked = 0;
    sh_future_1.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            invoked = 2;
        });
    sh_future_2.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            invoked *= 2;
        });
    ctx.run();
    BOOST_CHECK_EQUAL(invoked, 4);
    BOOST_CHECK_EQUAL(sh_future_1.get(), 54);
    BOOST_CHECK_EQUAL(sh_future_2.get(), 54);
}

BOOST_AUTO_TEST_CASE(async_wait_pre_set_exception)
{
    auto ctx         = asio::io_context{};
    auto promise     = saf::promise<int>{ ctx };
    auto sh_future_1 = promise.get_future().share();
    auto sh_future_2 = sh_future_1;
    promise.set_exception(std::make_exception_ptr(std::runtime_error{ "OPS" }));
    int invoked = 0;
    sh_future_1.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            invoked = 2;
        });
    sh_future_2.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            invoked *= 2;
        });
    ctx.run();
    BOOST_CHECK_EQUAL(invoked, 4);
    BOOST_CHECK_EXCEPTION(sh_future_1.get(), std::runtime_error, [](const auto& e) { return std::string{ e.what() } == "OPS"; });
    BOOST_CHECK_EXCEPTION(sh_future_2.get(), std::runtime_error, [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_CASE(async_wait_set_value)
{
    auto ctx         = asio::io_context{};
    auto promise     = saf::promise<int>{ ctx };
    auto sh_future_1 = promise.get_future().share();
    auto sh_future_2 = sh_future_1;
    int num          = 0;
    sh_future_1.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            num *= 2;
        });
    sh_future_2.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_value(54);
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 8);
    BOOST_CHECK_EQUAL(sh_future_1.get(), 54);
    BOOST_CHECK_EQUAL(sh_future_2.get(), 54);
}

BOOST_AUTO_TEST_CASE(async_wait_set_exception)
{
    auto ctx         = asio::io_context{};
    auto promise     = saf::promise<int>{ ctx };
    auto sh_future_1 = promise.get_future().share();
    auto sh_future_2 = sh_future_1;
    int num          = 0;
    sh_future_1.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            num *= 2;
        });
    sh_future_2.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_exception(std::make_exception_ptr(std::runtime_error{ "OPS" }));
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 8);
    BOOST_CHECK_EXCEPTION(sh_future_1.get(), std::runtime_error, [](const auto& e) { return std::string{ e.what() } == "OPS"; });
    BOOST_CHECK_EXCEPTION(sh_future_2.get(), std::runtime_error, [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_SUITE_END()
