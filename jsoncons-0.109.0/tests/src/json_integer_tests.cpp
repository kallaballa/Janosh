// Copyright 2013 Daniel Parker
// Distributed under Boost license

#include <catch/catch.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons/json_serializer.hpp>
#include <sstream>
#include <vector>
#include <utility>
#include <ctime>

using namespace jsoncons;

TEST_CASE("test_integer_limits")
{
    {
        std::ostringstream os;

        os << "{\"max int64_t\":" << (std::numeric_limits<int64_t>::max)() << "}";
        json val = json::parse(os.str());
        REQUIRE(val["max int64_t"].is_integer());
        CHECK(val["max int64_t"].as<int64_t>() == (std::numeric_limits<int64_t>::max)());
    }
    {
        std::ostringstream os;

        os << "{\"min int64_t\":" << (std::numeric_limits<int64_t>::min)() << "}";
        json val = json::parse(os.str());
        REQUIRE(val["min int64_t"].is_integer());
        CHECK(val["min int64_t"].as<int64_t>() == (std::numeric_limits<int64_t>::min)());
    }

    // test overflow
    {
        std::ostringstream os;

        os << "{\"int overflow\":-" << (std::numeric_limits<int64_t>::max)() << "0}";
        json val = json::parse(os.str());
        REQUIRE(val["int overflow"].is_bignum());
    }
    {
        std::ostringstream os;

        os << "{\"max uint64_t\":" << (std::numeric_limits<uint64_t>::max)() << "}";
        json val = json::parse(os.str());
        REQUIRE(val["max uint64_t"].is_uinteger());
        CHECK(val["max uint64_t"].as<uint64_t>() == (std::numeric_limits<uint64_t>::max)());
    }
    {
        std::ostringstream os;

        os << "{\"uint overflow\":" << (std::numeric_limits<uint64_t>::max)() << "0}";
        json val = json::parse(os.str());
        REQUIRE(val["uint overflow"].is_bignum());
    }
}

