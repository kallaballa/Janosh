// Copyright 2016 Daniel Parker
// Distributed under Boost license

#include <catch/catch.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/msgpack/msgpack.hpp>
#include <sstream>
#include <vector>
#include <utility>
#include <ctime>
#include <limits>

using namespace jsoncons;
using namespace jsoncons::msgpack;

void check_decode(const std::vector<uint8_t>& v, const json& expected)
{
    json result = decode_msgpack<json>(v);
    REQUIRE(expected == result);
}

void print_msgpack(const json j)
{
    std::vector<uint8_t> v;
    encode_msgpack(j, v);
    for (auto b : v)
    {
        std::cout << std::hex << (int)b << ','; 
    }
    std::cout << std::endl;
}

TEST_CASE("decode_number_msgpack_test")
{
    // positive fixint 0x00 - 0x7f
    check_decode({0x00},json(0U));
    check_decode({0x01},json(1U));
    check_decode({0x0a},json(10U));
    check_decode({0x17},json(23U));
    check_decode({0x18},json(24U));
    check_decode({0x7f},json(127U)); 

    check_decode({0xcc,0xff},json(255U));
    check_decode({0xcd,0x01,0x00},json(256U));
    check_decode({0xcd,0xff,0xff},json(65535U));
    check_decode({0xce,0,1,0x00,0x00},json(65536U));
    check_decode({0xce,0xff,0xff,0xff,0xff},json(4294967295U));
    check_decode({0xcf,0,0,0,1,0,0,0,0},json(4294967296U));
    check_decode({0xcf,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},json(std::numeric_limits<uint64_t>::max()));

    check_decode({0x01},json(1));
    check_decode({0x0a},json(10));
    check_decode({0x17},json(23)); 
    check_decode({0x18},json(24)); 
    check_decode({0x7f},json(127)); 

    check_decode({0xcc,0xff},json(255));
    check_decode({0xcd,0x01,0x00},json(256));
    check_decode({0xcd,0xff,0xff},json(65535));
    check_decode({0xce,0,1,0x00,0x00},json(65536));
    check_decode({0xce,0xff,0xff,0xff,0xff},json(4294967295));
    check_decode({0xd3,0,0,0,1,0,0,0,0},json(4294967296));
    check_decode({0xd3,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff},json(std::numeric_limits<int64_t>::max()));

    // negative fixint 0xe0 - 0xff
    check_decode({0xe0},json(-32));
    check_decode({0xff},json(-1)); //

    // negative integers
    check_decode({0xd1,0xff,0},json(-256));
    check_decode({0xd1,0xfe,0xff},json(-257));
    check_decode({0xd2,0xff,0xff,0,0},json(-65536));
    check_decode({0xd2,0xff,0xfe,0xff,0xff},json(-65537));
    check_decode({0xd3,0xff,0xff,0xff,0xff,0,0,0,0},json(-4294967296));
    check_decode({0xd3,0xff,0xff,0xff,0xfe,0xff,0xff,0xff,0xff},json(-4294967297));

    // null, true, false
    check_decode({0xc0},json::null()); // 
    check_decode({0xc3},json(true)); //
    check_decode({0xc2},json(false)); //

    // floating point
    check_decode({0xcb,0,0,0,0,0,0,0,0},json(0.0));
    check_decode({0xcb,0xbf,0xf0,0,0,0,0,0,0},json(-1.0));
    check_decode({0xcb,0xc1,0x6f,0xff,0xff,0xe0,0,0,0},json(-16777215.0));

    // string
    check_decode({0xa0},json(""));
    check_decode({0xa1,' '},json(" "));
    check_decode({0xbf,'1','2','3','4','5','6','7','8','9','0',
                       '1','2','3','4','5','6','7','8','9','0',
                       '1','2','3','4','5','6','7','8','9','0',
                       '1'},
                 json("1234567890123456789012345678901"));
    check_decode({0xd9,0x20,'1','2','3','4','5','6','7','8','9','0',
                            '1','2','3','4','5','6','7','8','9','0',
                            '1','2','3','4','5','6','7','8','9','0',
                            '1','2'},
                 json("12345678901234567890123456789012"));


}

TEST_CASE("decode_msgpack_arrays_and_maps")
{
    // fixarray
    check_decode({0x90},json::array());
    check_decode({0x80},json::object());

    check_decode({0x91,'\0'},json::parse("[0]"));
    check_decode({0x92,'\0','\0'},json::array({0,0}));
    check_decode({0x92,0x91,'\0','\0'}, json::parse("[[0],0]"));
    check_decode({0x91,0xa5,'H','e','l','l','o'},json::parse("[\"Hello\"]"));

    check_decode({0x81,0xa2,'o','c',0x91,'\0'}, json::parse("{\"oc\": [0]}"));
    check_decode({0x81,0xa2,'o','c',0x94,'\0','\1','\2','\3'}, json::parse("{\"oc\": [0, 1, 2, 3]}"));
}

