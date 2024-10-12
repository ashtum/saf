// Copyright (c) 2022 Mohammad Nejati
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include <saf.hpp>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(promise)

namespace asio = boost::asio;

BOOST_AUTO_TEST_CASE(is_valid)
{
    auto ctx = asio::io_context{};

    auto promise = saf::promise<void>{ ctx };
    BOOST_CHECK(promise.is_valid());

    auto promise_2 = std::move(promise);
    BOOST_CHECK(!promise.is_valid());
    BOOST_CHECK(promise_2.is_valid());
}

BOOST_AUTO_TEST_CASE(get_future)
{
    auto ctx = asio::io_context{};

    {
        auto promise = saf::promise<void>{ ctx };
        BOOST_CHECK_NO_THROW(boost::ignore_unused(promise.get_future()));
        BOOST_CHECK_EXCEPTION(
            boost::ignore_unused(promise.get_future()),
            saf::future_error,
            [](const auto& e)
            { return e.code() == saf::future_errc::future_already_retrieved; });
    }

    {
        auto promise   = saf::promise<void>{ ctx };
        auto promise_2 = std::move(promise);
        BOOST_CHECK_EXCEPTION(
            boost::ignore_unused(promise.get_future()),
            saf::future_error,
            [](const auto& e)
            { return e.code() == saf::future_errc::no_state; });
    }
}

BOOST_AUTO_TEST_CASE(set_value)
{
    auto ctx = asio::io_context{};

    {
        auto promise = saf::promise<int>{ ctx };
        BOOST_CHECK_NO_THROW(promise.set_value(10));
        BOOST_CHECK_EXCEPTION(
            promise.set_value(20),
            saf::future_error,
            [](const auto& e) {
                return e.code() == saf::future_errc::promise_already_satisfied;
            });
        BOOST_CHECK_EQUAL(promise.get_future().get(), 10);
    }

    {
        auto promise   = saf::promise<int>{ ctx };
        auto promise_2 = std::move(promise);
        BOOST_CHECK_EXCEPTION(
            promise.set_value(10),
            saf::future_error,
            [](const auto& e)
            { return e.code() == saf::future_errc::no_state; });
        BOOST_CHECK(!promise_2.get_future().is_ready());
    }
}

BOOST_AUTO_TEST_CASE(set_value_void)
{
    auto ctx = asio::io_context{};

    {
        auto promise = saf::promise<void>{ ctx };
        BOOST_CHECK_NO_THROW(promise.set_value());
        BOOST_CHECK_EXCEPTION(
            promise.set_value(),
            saf::future_error,
            [](const auto& e) {
                return e.code() == saf::future_errc::promise_already_satisfied;
            });
        BOOST_CHECK_NO_THROW(promise.get_future().get());
    }

    {
        auto promise   = saf::promise<void>{ ctx };
        auto promise_2 = std::move(promise);
        BOOST_CHECK_EXCEPTION(
            promise.set_value(),
            saf::future_error,
            [](const auto& e)
            { return e.code() == saf::future_errc::no_state; });
        BOOST_CHECK(!promise_2.get_future().is_ready());
    }
}

BOOST_AUTO_TEST_CASE(set_exception)
{
    auto ctx = asio::io_context{};

    auto exception_ptr = std::make_exception_ptr(std::runtime_error{ "OPS" });

    {
        auto promise = saf::promise<int>{ ctx };
        BOOST_CHECK_NO_THROW(promise.set_exception(exception_ptr));
        BOOST_CHECK_EXCEPTION(
            promise.set_exception(exception_ptr),
            saf::future_error,
            [](const auto& e) {
                return e.code() == saf::future_errc::promise_already_satisfied;
            });
        BOOST_CHECK_EXCEPTION(
            promise.get_future().get(),
            std::runtime_error,
            [](const auto& e) { return std::string{ e.what() } == "OPS"; });
    }

    {
        auto promise   = saf::promise<int>{ ctx };
        auto promise_2 = std::move(promise);
        BOOST_CHECK_EXCEPTION(
            promise.set_exception(exception_ptr),
            saf::future_error,
            [](const auto& e)
            { return e.code() == saf::future_errc::no_state; });
        BOOST_CHECK(!promise_2.get_future().is_ready());
    }
}

BOOST_AUTO_TEST_CASE(get_excecutor)
{
    auto ctx = asio::io_context{};

    auto promise = saf::promise<int>{ ctx };

    BOOST_CHECK_NO_THROW(boost::ignore_unused(promise.get_executor()));
    auto promise_2 = std::move(promise);
    BOOST_CHECK_EXCEPTION(
        boost::ignore_unused(promise.get_executor()),
        saf::future_error,
        [](const auto& e) { return e.code() == saf::future_errc::no_state; });
}

BOOST_AUTO_TEST_CASE(broken_promise)
{
    auto ctx = asio::io_context{};

    auto future = saf::future<int>{};
    {
        auto promise = saf::promise<int>{ ctx };
        future       = promise.get_future();
    }
    BOOST_CHECK_EXCEPTION(
        future.get(),
        saf::future_error,
        [](const auto& e)
        { return e.code() == saf::future_errc::broken_promise; });
}

BOOST_AUTO_TEST_CASE(custom_allocator)
{
    auto ctx = asio::io_context{};

    {
        auto alloc   = std::allocator<std::byte>{};
        auto promise = saf::promise<int>{ ctx, alloc };
    }

    {
        auto alloc   = std::allocator<std::byte>{};
        auto promise = saf::promise<int>{ ctx.get_executor(), alloc };
    }
}

BOOST_AUTO_TEST_CASE(custom_executor)
{
    auto ctx         = asio::io_context{};
    using executor_t = asio::io_context::executor_type;

    auto promise   = saf::promise<void, executor_t>{ ctx };
    auto future    = saf::future<void, executor_t>{ promise.get_future() };
    auto sh_future = saf::shared_future<void, executor_t>{ future.share() };
}

BOOST_AUTO_TEST_CASE(set_value_with_different_arguments)
{
    auto ctx = asio::io_context{};

    class non_moveable
    {
      public:
        non_moveable(int)
        {
        }
        non_moveable(non_moveable&&) = delete;
    };

    {
        auto promise = saf::promise<non_moveable>{ ctx };
        promise.set_value(1);
    }

    class move_only
    {
      public:
        move_only(int)
        {
        }
        move_only(const move_only&) = delete;
        move_only(move_only&&)      = default;
    };

    {
        auto promise = saf::promise<move_only>{ ctx };
        auto obj     = move_only{ 1 };
        promise.set_value(std::move(obj));
    }

    {
        auto promise = saf::promise<move_only>{ ctx };
        promise.set_value(move_only{ 1 });
    }

    class copyable
    {
      public:
        copyable(int)
        {
        }
    };

    {
        auto promise = saf::promise<copyable>{ ctx };
        auto obj     = copyable{ 1 };
        promise.set_value(obj);
    }

    {
        auto promise   = saf::promise<copyable>{ ctx };
        const auto obj = copyable{ 1 };
        promise.set_value(obj);
    }
}

BOOST_AUTO_TEST_SUITE_END()
