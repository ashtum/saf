// Copyright (c) 2022 Mohammad Nejati
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include <saf.hpp>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(future)

namespace asio = boost::asio;

BOOST_AUTO_TEST_CASE(is_valid)
{
    auto ctx = asio::io_context{};

    auto future = saf::promise<void>{ ctx }.get_future();
    BOOST_CHECK(future.is_valid() == true);

    auto future_2 = std::move(future);
    BOOST_CHECK(future.is_valid() == false);
    BOOST_CHECK(future_2.is_valid() == true);
}

BOOST_AUTO_TEST_CASE(share)
{
    auto ctx = asio::io_context{};

    auto future    = saf::promise<void>{ ctx }.get_future();
    auto sh_future = future.share();
    BOOST_CHECK(sh_future.is_valid() == true);
    BOOST_CHECK(future.is_valid() == false);
    BOOST_CHECK_EXCEPTION(
        boost::ignore_unused(future.share()),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
    auto sh_future_2 = sh_future;
    BOOST_CHECK(sh_future_2.is_valid() == true);
}

BOOST_AUTO_TEST_CASE(get_excecutor)
{
    auto ctx = asio::io_context{};

    auto future = saf::promise<void>{ ctx }.get_future();

    BOOST_CHECK_NO_THROW(boost::ignore_unused(future.get_executor()));
    auto future_2 = std::move(future);
    BOOST_CHECK_EXCEPTION(
        boost::ignore_unused(future.get_executor()),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(is_ready)
{
    auto ctx = asio::io_context{};

    auto promise = saf::promise<void>{ ctx };
    auto future  = promise.get_future();

    BOOST_CHECK(future.is_ready() == false);
    promise.set_value();
    BOOST_CHECK(future.is_ready() == true);

    auto future_2 = std::move(future);
    BOOST_CHECK_EXCEPTION(
        boost::ignore_unused(future.is_ready()),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(get_no_state)
{
    auto future = saf::future<int>{};
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(extract_no_state)
{
    auto future = saf::future<int>{};
    BOOST_CHECK_EXCEPTION(
        future.extract(),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(get_unready)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();

    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::unready_future; });
}

BOOST_AUTO_TEST_CASE(extract_unready)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();

    BOOST_CHECK_EXCEPTION(
        future.extract(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::unready_future; });
}

BOOST_AUTO_TEST_CASE(get_already_extracted)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    BOOST_CHECK_EQUAL(future.extract(), 54);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(extract_already_extracted)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    BOOST_CHECK_EQUAL(future.extract(), 54);
    BOOST_CHECK_EXCEPTION(
        future.extract(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_wait_no_state)
{
    auto future = saf::future<int>{};
    BOOST_CHECK_EXCEPTION(
        future.async_wait(asio::detached),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(async_extract_no_state)
{
    auto future = saf::future<int>{};
    BOOST_CHECK_EXCEPTION(
        future.async_extract(asio::detached),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(async_wait_pre_set_value)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    auto invoked = false;
    future.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EQUAL(future.get(), 54);
}

BOOST_AUTO_TEST_CASE(async_extract_pre_set_value)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    auto invoked = false;
    future.async_extract(
        [&](auto eptr, int value)
        {
            BOOST_CHECK(!eptr);
            BOOST_CHECK_EQUAL(value, 54);
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_wait_pre_extracted)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    BOOST_CHECK_EQUAL(future.extract(), 54);
    auto invoked = false;
    future.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_extract_pre_extracted)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_value(54);
    BOOST_CHECK_EQUAL(future.extract(), 54);
    auto invoked = false;
    future.async_extract(
        [&](auto eptr, int value)
        {
            BOOST_CHECK(eptr);
            BOOST_CHECK_EQUAL(value, int{});
            BOOST_CHECK_EXCEPTION(
                std::rethrow_exception(eptr),
                saf::future_error,
                [](const auto& e)
                { return e.code() == saf::future_errc::value_already_extracted; });
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_wait_pre_set_exception)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_exception(std::make_exception_ptr(std::runtime_error{ "OPS" }));
    auto invoked = false;
    future.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        std::runtime_error,
        [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_CASE(async_extract_pre_set_exception)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    promise.set_exception(std::make_exception_ptr(std::runtime_error{ "OPS" }));
    auto invoked = false;
    future.async_extract(
        [&](auto eptr, int value)
        {
            BOOST_CHECK(eptr);
            BOOST_CHECK_EQUAL(value, int{});
            BOOST_CHECK_EXCEPTION(
                std::rethrow_exception(eptr),
                std::runtime_error,
                [](const auto& e) { return std::string{ e.what() } == "OPS"; });
            invoked = true;
        });
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        std::runtime_error,
        [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_CASE(async_wait_set_value)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    int num      = 0;
    future.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_value(54);
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 4);
    BOOST_CHECK_EQUAL(future.get(), 54);
}

BOOST_AUTO_TEST_CASE(async_extract_set_value)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    int num      = 0;
    future.async_extract(
        [&](auto eptr, int value)
        {
            BOOST_CHECK(!eptr);
            BOOST_CHECK_EQUAL(value, 54);
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_value(54);
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 4);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_wait_set_exception)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    int num      = 0;
    future.async_wait(
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_exception(
                std::make_exception_ptr(std::runtime_error{ "OPS" }));
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 4);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        std::runtime_error,
        [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_CASE(async_extract_set_exception)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    int num      = 0;
    future.async_extract(
        [&](auto eptr, int value)
        {
            BOOST_CHECK(eptr);
            BOOST_CHECK_EQUAL(value, int{});
            BOOST_CHECK_EXCEPTION(
                std::rethrow_exception(eptr),
                std::runtime_error,
                [](const auto& e) { return std::string{ e.what() } == "OPS"; });
            num *= 2;
        });
    ctx.post(
        [&]
        {
            num = 2;
            promise.set_exception(
                std::make_exception_ptr(std::runtime_error{ "OPS" }));
        });
    ctx.run();
    BOOST_CHECK_EQUAL(num, 4);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        std::runtime_error,
        [](const auto& e) { return std::string{ e.what() } == "OPS"; });
}

BOOST_AUTO_TEST_CASE(async_wait_cancellation)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<void>{ ctx };
    auto future  = promise.get_future();
    auto invoked = false;
    auto cs      = asio::cancellation_signal{};
    future.async_wait(asio::bind_cancellation_slot(
        cs.slot(),
        [&](auto ec)
        {
            BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
            invoked = true;
        }));
    cs.emit(asio::cancellation_type::terminal);
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK(future.is_ready() == false);
}

BOOST_AUTO_TEST_CASE(async_extract_cancellation)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    auto invoked = false;
    auto cs      = asio::cancellation_signal{};
    future.async_extract(asio::bind_cancellation_slot(
        cs.slot(),
        [&](auto eptr, auto value)
        {
            BOOST_CHECK(eptr);
            BOOST_CHECK_EQUAL(value, int{});
            BOOST_CHECK_EXCEPTION(
                std::rethrow_exception(eptr),
                saf::system_error,
                [](const auto& e)
                { return e.code() == asio::error::operation_aborted; });
            invoked = true;
        }));
    cs.emit(asio::cancellation_type::terminal);
    ctx.run();
    BOOST_CHECK(invoked == true);
    BOOST_CHECK(future.is_ready() == false);
}

BOOST_AUTO_TEST_CASE(async_wait_cancel_already_completed)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    auto invoked = 0;
    auto cs      = asio::cancellation_signal{};
    future.async_wait(asio::bind_cancellation_slot(
        cs.slot(),
        [&](auto ec)
        {
            BOOST_CHECK(!ec);
            invoked += 1;
        }));
    promise.set_value();
    cs.emit(asio::cancellation_type::terminal);
    ctx.run();
    BOOST_CHECK_EQUAL(invoked, 1);
    BOOST_CHECK(future.is_ready() == true);
}

BOOST_AUTO_TEST_CASE(async_extract_cancel_already_completed)
{
    auto ctx     = asio::io_context{};
    auto promise = saf::promise<int>{ ctx };
    auto future  = promise.get_future();
    auto invoked = 0;
    auto cs      = asio::cancellation_signal{};
    future.async_extract(asio::bind_cancellation_slot(
        cs.slot(),
        [&](auto eptr, auto value)
        {
            BOOST_CHECK(!eptr);
            BOOST_CHECK_EQUAL(value, int{});
            invoked += 1;
        }));
    promise.set_value();
    cs.emit(asio::cancellation_type::terminal);
    ctx.run();
    BOOST_CHECK_EQUAL(invoked, 1);
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::value_already_extracted; });
}

BOOST_AUTO_TEST_CASE(async_wait_shutdown)
{
    auto promise = saf::promise<void>{};
    auto obj     = std::make_shared<int>();
    {
        auto ctx    = asio::io_context{};
        promise     = saf::promise<void>{ ctx };
        auto future = promise.get_future();
        BOOST_CHECK_EQUAL(obj.use_count(), 1);
        future.async_wait([obj](auto) {});
        BOOST_CHECK_EQUAL(obj.use_count(), 2);
    }
    BOOST_CHECK_EQUAL(obj.use_count(), 1);
}

BOOST_AUTO_TEST_CASE(async_extract_shutdown)
{
    auto promise = saf::promise<int>{};
    auto obj     = std::make_shared<int>();
    {
        auto ctx    = asio::io_context{};
        promise     = saf::promise<int>{ ctx };
        auto future = promise.get_future();
        BOOST_CHECK_EQUAL(obj.use_count(), 1);
        future.async_extract([obj](auto,auto) {});
        BOOST_CHECK_EQUAL(obj.use_count(), 2);
    }
    BOOST_CHECK_EQUAL(obj.use_count(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
