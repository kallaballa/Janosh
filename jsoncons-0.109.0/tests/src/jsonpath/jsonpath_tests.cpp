// Copyright 2013 Daniel Parker
// Distributed under Boost license

#include <catch/catch.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <utility>
#include <ctime>
#include <new>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpath/json_query.hpp>

using namespace jsoncons;
using namespace jsoncons::jsonpath;

const json complex_json = json::parse(R"(
[
  {
    "root": {
      "id" : 10,
      "second": [
        {
             "names": [
            2
          ],
          "complex": [
            {
              "names": [
                1
              ],
              "panels": [
                {
                  "result": [
                    1
                  ]
                },
                {
                  "result": [
                    1,
                    2,
                    3,
                    4
                  ]
                },
                {
                  "result": [
                    1
                  ]
                }
              ]
            }
          ]
        }
      ]
    }
  },
  {
    "root": {
      "id" : 20,
      "second": [
        {
          "names": [
            2
          ],
          "complex": [
            {
              "names": [
                1
              ],
              "panels": [
                {
                  "result": [
                    1
                  ]
                },
                {
                  "result": [
                    1,
                    2,
                    3,
                    4
                  ]
                },
                {
                  "result": [
                    1
                  ]
                }
              ]
            }
          ]
        }
      ]
    }
  }
]
)");

const json store = json::parse(R"(
{
    "store": {
        "book": [
            {
                "category": "reference",
                "author": "Nigel Rees",
                "title": "Sayings of the Century",
                "price": 8.95
            },
            {
                "category": "fiction",
                "author": "Evelyn Waugh",
                "title": "Sword of Honour",
                "price": 12.99
            },
            {
                "category": "fiction",
                "author": "Herman Melville",
                "title": "Moby Dick",
                "isbn": "0-553-21311-3",
                "price": 8.99
            },
            {
                "category": "fiction",
                "author": "J. R. R. Tolkien",
                "title": "The Lord of the Rings",
                "isbn": "0-395-19395-8",
                "price": 22.99
            }
        ],
        "bicycle": {
            "color": "red",
            "price": 19.95
        }
    }
}
)");

struct jsonpath_fixture
{
    static const char* store_text()
    {
        static const char* text = "{ \"store\": {\"book\": [ { \"category\": \"reference\",\"author\": \"Nigel Rees\",\"title\": \"Sayings of the Century\",\"price\": 8.95},{ \"category\": \"fiction\",\"author\": \"Evelyn Waugh\",\"title\": \"Sword of Honour\",\"price\": 12.99},{ \"category\": \"fiction\",\"author\": \"Herman Melville\",\"title\": \"Moby Dick\",\"isbn\": \"0-553-21311-3\",\"price\": 8.99},{ \"category\": \"fiction\",\"author\": \"J. R. R. Tolkien\",\"title\": \"The Lord of the Rings\",\"isbn\": \"0-395-19395-8\",\"price\": 22.99}],\"bicycle\": {\"color\": \"red\",\"price\": 19.95}}}";
        return text;
    }
    static const char* store_text_empty_isbn()
    {
        static const char* text = "{ \"store\": {\"book\": [ { \"category\": \"reference\",\"author\": \"Nigel Rees\",\"title\": \"Sayings of the Century\",\"price\": 8.95},{ \"category\": \"fiction\",\"author\": \"Evelyn Waugh\",\"title\": \"Sword of Honour\",\"price\": 12.99},{ \"category\": \"fiction\",\"author\": \"Herman Melville\",\"title\": \"Moby Dick\",\"isbn\": \"0-553-21311-3\",\"price\": 8.99},{ \"category\": \"fiction\",\"author\": \"J. R. R. Tolkien\",\"title\": \"The Lord of the Rings\",\"isbn\": \"\",\"price\": 22.99}],\"bicycle\": {\"color\": \"red\",\"price\": 19.95}}}";
        return text;
    }
    static const char* book_text()
    {
        static const char* text = "{ \"category\": \"reference\",\"author\": \"Nigel Rees\",\"title\": \"Sayings of the Century\",\"price\": 8.95}";
        return text;
    }

    json book()
    {
        json root = json::parse(jsonpath_fixture::store_text());
        json book = root["store"]["book"];
        return book;
    }

    json bicycle()
    {
        json root = json::parse(jsonpath_fixture::store_text());
        json bicycle = root["store"]["bicycle"];
        return bicycle;
    }
};

TEST_CASE("test_path")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$.store.book");

    json expected = json::array();
    expected.push_back(fixture.book());

    CHECK(expected == result);

    //std::cout << pretty_print(result) << std::endl;
}

TEST_CASE("test_jsonpath_store_book2")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$['store']['book']");

    json expected = json::array();
    expected.push_back(fixture.book());

    CHECK(expected == result);
    //    std::c/out << pretty_print(result) << std::endl;
}

TEST_CASE("test_jsonpath_bracket_with_double_quotes")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$[\"store\"][\"book\"]");

    json expected = json::array();
    expected.push_back(fixture.book());

    CHECK(expected == result);
    //    std::c/out << pretty_print(result) << std::endl;
}

TEST_CASE("test_jsonpath_store_book_bicycle")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$['store']['book','bicycle']");

    json expected = json::array();
    expected.push_back(fixture.book());
    expected.push_back(fixture.bicycle());
    CHECK(expected == result);

    //std::cout << pretty_print(result) << std::endl;
}

TEST_CASE("test_jsonpath_store_book_bicycle_unquoted")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$[store][book,bicycle]");

    json expected = json::array();
    expected.push_back(fixture.book());
    expected.push_back(fixture.bicycle());
    CHECK(expected == result);

    //std::cout << pretty_print(result) << std::endl;
}

TEST_CASE("test_jsonpath_store_book_union")
{
    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$['store']..['author','title']");

    json expected = json::parse(R"(
[
    "Nigel Rees",
    "Sayings of the Century",
    "Evelyn Waugh",
    "Sword of Honour",
    "Herman Melville",
    "Moby Dick",
    "J. R. R. Tolkien",
    "The Lord of the Rings"
]
    )");
}

TEST_CASE("test_jsonpath_store_book_star")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$['store']['book'][*]");
    json expected = fixture.book();

    //std::cout << pretty_print(result) << std::endl;
    CHECK(expected == result);
}

TEST_CASE("test_store_dotdot_price")
{
    jsonpath_fixture fixture;
    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$.store..price");

    json expected = json::array();
    expected.push_back(fixture.bicycle()["price"]);
    json book_list = fixture.book();
    for (size_t i = 0; i < book_list.size(); ++i)
    {
        expected.push_back(book_list[i]["price"]);
    }

    //std::cout << pretty_print(result) << std::endl;

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_recursive_descent")
{
    json root = json::parse(jsonpath_fixture::store_text());

    json result1 = json_query(root,"$..book[2]");
    //std::cout << pretty_print(result1) << std::endl;
    CHECK(result1.size() == 1);
    CHECK(result1[0] == root["store"]["book"][2]);

    json result1a = json_query(root,"$..book.2");
    //std::cout << pretty_print(result1a) << std::endl;
    CHECK(result1a.size() == 1);
    CHECK(result1a[0] == root["store"]["book"][2]);

    json result2 = json_query(root,"$..book[-1:]");
    //std::cout << pretty_print(result2) << std::endl;
    CHECK(result2.size() == 1);
    CHECK(result2[0] == root["store"]["book"][3]);

    json result3 = json_query(root,"$..book[0,1]");
    //std::cout << pretty_print(result3) << std::endl;
    CHECK(result3.size() == 2);
    CHECK(result3[0] == root["store"]["book"][0]);
    CHECK(result3[1] == root["store"]["book"][1]);

    json result4 = json_query(root,"$..book[:2]");
    //std::cout << pretty_print(result4) << std::endl;
    CHECK(result4.size() == 2);
    CHECK(result4[0] == root["store"]["book"][0]);
    CHECK(result4[1] == root["store"]["book"][1]);

    json result5 = json_query(root,"$..book[1:2]");
    //std::cout << pretty_print(result5) << std::endl;
    CHECK(result5.size() == 1);
    CHECK(result5[0] == root["store"]["book"][1]);

    json result6 = json_query(root,"$..book[-2:]");
    //std::cout << pretty_print(result6) << std::endl;
    CHECK(result6.size() == 2);
    CHECK(result6[0] == root["store"]["book"][2]);
    CHECK(result6[1] == root["store"]["book"][3]);

    json result7 = json_query(root,"$..book[2:]");
    //std::cout << pretty_print(result7) << std::endl;
    CHECK(result7.size() == 2);
    CHECK(result7[0] == root["store"]["book"][2]);
    CHECK(result7[1] == root["store"]["book"][3]);
}

TEST_CASE("test_jsonpath_filter1")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(@.price<10)]");
    //std::cout << pretty_print(result) << std::endl;
    json books = fixture.book();
    json expected = json::array();
    for (size_t i = 0; i < books.size(); ++i)
    {
        double price = books[i]["price"].as<double>();
        if (price < 10)
        {
            expected.push_back(books[i]);
        }
    }
    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_filter2")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(10 > @.price)]");

    //std::cout << pretty_print(result) << std::endl;
    json books = fixture.book();
    json expected = json::array();
    for (size_t i = 0; i < books.size(); ++i)
    {
        double price = books[i]["price"].as<double>();
        if (10 > price)
        {
            expected.push_back(books[i]);
        }
    }
    CHECK(expected == result);
}
 
TEST_CASE("test_jsonpath_filter_category_eq_reference")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(@.category == 'reference')]");

    //std::cout << pretty_print(result) << std::endl;
    json books = fixture.book();
    json expected = json::array();
    for (size_t i = 0; i < books.size(); ++i)
    {
        double price = books[i]["price"].as<double>();
        if (books[i]["category"].as<std::string>() == "reference")
        {
            expected.push_back(books[i]);
        }
    }
    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_filter3")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?((@.price > 8) && (@.price < 12))]");

    json books = fixture.book();

    json expected = json::array();
    for (size_t i = 0; i < books.size(); ++i)
    {
        double price = books[i]["price"].as<double>();
        if (price > 8 && price < 12)
        {
            expected.push_back(books[i]);
        }
    }
    //std::cout << pretty_print(result) << std::endl;
    //std::cout << pretty_print(expected) << std::endl;

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_book_isbn")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json books = fixture.book();
    for (size_t i = 0; i < books.size(); ++i)
    {
        bool has_isbn = books[i].count("isbn") > 0;
        if (has_isbn)
        {
            json result = json_query(books[i],"@.isbn");
            json expected = json::array();
            expected.push_back(books[i]["isbn"]);
            CHECK(expected == result);
            //std::cout << pretty_print(result) << std::endl;
        }
    }
}

TEST_CASE("test_jsonpath_book_empty_isbn")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text_empty_isbn());

    json books = fixture.book();
    for (size_t i = 0; i < books.size(); ++i)
    {
        bool has_isbn = books[i].count("isbn") > 0;
        if (has_isbn)
        {
            json result = json_query(books[i],"@.isbn");
            json expected = json::array();
            expected.push_back(books[i]["isbn"]);
            CHECK(expected == result);
            //std::cout << pretty_print(result) << std::endl;
        }
    }
}

TEST_CASE("test_jsonpath_filter4")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(@.isbn)]");

    json books = fixture.book();

    json expected = json::array();
    for (size_t i = 0; i < books.size(); ++i)
    {
        if (books[i].count("isbn") > 0)
        {
            expected.push_back(books[i]);
        }
    }
    //std::cout << pretty_print(result) << std::endl;
    //std::cout << pretty_print(expected) << std::endl;

    CHECK(expected == result);
}
TEST_CASE("test_jsonpath_array_length")
{

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book.length");

    //std::cout << pretty_print(result) << std::endl;

    CHECK(1 == result.size());
    CHECK(root["store"]["book"].size() == result[0].as<size_t>());
}
 
TEST_CASE("test_jsonpath_book_category")
{
    json root = json::parse(jsonpath_fixture::book_text());

    json result = json_query(root,"@.category");

    CHECK(1 == result.size());
    CHECK("reference" == result[0].as<std::string>());
}

TEST_CASE("test_jsonpath_book_filter_false")
{

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(false)]");
    //std::cout << pretty_print(result) << std::endl;
    
    json expected = json::array();

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_book_filter_false_and_false")
{

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(false && false)]");
    //std::cout << pretty_print(result) << std::endl;
    
    json expected = json::array();

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_book_filter_false_or_false")
{
    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(false || false)]");
    //std::cout << pretty_print(result) << std::endl;
    
    json expected = json::array();

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_book_filter_false_or_true")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..book[?(false || true)]");
    //std::cout << pretty_print(result) << std::endl;
    
    CHECK(fixture.book() == result);
}

TEST_CASE("test_jsonpath_store_book_authors")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$.store.book[?(@.price < 10)].author");

    json expected = json::array();
    json book_list = fixture.book();
    for (size_t i = 0; i < book_list.size(); ++i)
    {
        json book = book_list[i];
        if (book["price"].as<double>() < 10)
        {
            expected.push_back(book["author"]);
        }
    }

    //json expected = fixture.book();

    //std::cout << pretty_print(result) << std::endl;

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_store_book_tests")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result1 = json_query(root,"$.store.book[ ?(@.category == @.category) ]");
    CHECK(fixture.book() == result1);

    json result2 = json_query(root,"$.store.book[ ?(@.category == @['category']) ]");
    CHECK(fixture.book() == result2);

    json result3 = json_query(root,"$.store.book[ ?(@ == @) ]");
    CHECK(fixture.book() == result3);

    json result4 = json_query(root,"$.store.book[ ?(@.category != @.category) ]");
    json expected4 = json::array();
    CHECK(expected4 == result4);
}

TEST_CASE("test_jsonpath_store_book_tests2")
{
    json root = json::parse(jsonpath_fixture::store_text());

    json result1 = json_query(root,"$.store.book[ ?((@.author == 'Nigel Rees') || (@.author == 'Evelyn Waugh')) ].author");
    json expected1 = json::array();
    expected1.push_back("Nigel Rees");
    expected1.push_back("Evelyn Waugh");
    CHECK(expected1 == result1);

    json result1b = json_query(root,"$.store.book[ ?((@.author == 'Nigel Rees') || (@.author == 'Evelyn Waugh')) ].title");
    json expected1b = json::array();
    expected1b.push_back("Sayings of the Century");
    expected1b.push_back("Sword of Honour");
    //std::cout << result1b << std::endl;
    CHECK(expected1b == result1b);

    json result2 = json_query(root,"$.store.book[ ?(((@.author == 'Nigel Rees') || (@.author == 'Evelyn Waugh')) && (@.price < 15)) ].author");
    json expected2 = json::array();
    expected2.push_back("Nigel Rees");
    expected2.push_back("Evelyn Waugh");
    CHECK(expected2 == result2);

    json result3 = json_query(root,"$.store.book[ ?(((@.author == 'Nigel Rees') || (@.author == 'Evelyn Waugh')) && (@.category == 'reference')) ].author");
    json expected3 = json::array();
    expected3.push_back("Nigel Rees");
    CHECK(expected3 == result3);

    json result4 = json_query(root,"$.store.book[ ?(((@.author == 'Nigel Rees') || (@.author == 'Evelyn Waugh')) && (@.category != 'fiction')) ].author");
    json expected4 = json::array();
    expected4.push_back("Nigel Rees");
    CHECK(expected4 == result4);

    json result5 = json_query(root,"$.store.book[?('a' == 'a')].author");
    json expected5 = json::array();
    expected5.push_back("Nigel Rees");
    expected5.push_back("Evelyn Waugh");
    expected5.push_back("Herman Melville");
    expected5.push_back("J. R. R. Tolkien");
    CHECK(expected5 == result5);

    json result6 = json_query(root,"$.store.book[?('a' == 'b')].author");
    json expected6 = json::array();
    CHECK(expected6 == result6);
}

#if !defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
// GCC 4.8 has broken regex support: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53631
TEST_CASE("test_jsonpath_store_book_regex")
{
    json root = json::parse(jsonpath_fixture::store_text());

    json result3 = json_query(root,"$.store.book[ ?(@.category =~ /fic.*?/)].author");
    json expected3 = json::array();
    expected3.push_back("Evelyn Waugh");
    expected3.push_back("Herman Melville");
    expected3.push_back("J. R. R. Tolkien");
    CHECK(expected3 == result3);

    json result4 = json_query(root,"$.store.book[ ?(@.author =~ /Evelyn.*?/)].author");
    json expected4 = json::array();
    expected4.push_back("Evelyn Waugh");
    CHECK(expected4 == result4);

    json result5 = json_query(root,"$.store.book[ ?(!(@.author =~ /Evelyn.*?/))].author");
    json expected5 = json::array();
    expected5.push_back("Nigel Rees");
    expected5.push_back("Herman Melville");
    expected5.push_back("J. R. R. Tolkien");
    CHECK(expected5 == result5);
}
#endif

TEST_CASE("test_jsonpath_everything")
{
    jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$.store.*");
    //std::cout << result << std::endl;
 
    json expected = json::array();
    expected.push_back(fixture.bicycle());
    expected.push_back(fixture.book());

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_everything_in_store")
{

    json root = json::parse(jsonpath_fixture::store_text());

    json result = json_query(root,"$..*");
    //std::cout << result << std::endl;
 
    json expected = json::array();
    expected.push_back(root["store"]);

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_last_of_two_arrays")
{
    json val = json::parse(R"(
{ "store": {
    "book": [ 
          { "author": "Nigel Rees"
          },
          { "author": "Evelyn Waugh"
          },
          { "author": "Herman Melville"
          }
        ]
    },
    "Roman": {
    "book": [ 
          { "author": "Tolstoy L"
          },
          { "author": "Tretyakovskiy R"
          },
          { "author": "Kulik M"
          }
        ]
    }  
}
    )");

    json expected = json::parse(R"(
[
    { "author": "Kulik M"},
    { "author": "Herman Melville"}
]
    )");

    json result = json_query(val, "$..book[(@.length - 1)]");

    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_next_to_last_of_two_arrays")
{
    json val = json::parse(R"(
{ "store": {
    "book": [ 
          { "author": "Nigel Rees"
          },
          { "author": "Evelyn Waugh"
          },
          { "author": "Herman Melville"
          }
        ]
    },
    "Roman": {
    "book": [ 
          { "author": "Tolstoy L"
          },
          { "author": "Tretyakovskiy R"
          },
          { "author": "Kulik M"
          }
        ]
    }  
}
    )");

    json expected = json::parse(R"(
[
    { "author": "Tretyakovskiy R"},
    { "author": "Evelyn Waugh"}
]
    )");

    json result = json_query(val, "$..book[(@.length - 2)]");

    CHECK(expected == result);

    json expected2 = json::parse(R"(
[
    "Tolstoy L",
    "Nigel Rees"
]
    )");
    std::string path2 = "$..[0].author";
    json result2 = json_query(val, path2);
    CHECK(expected2 == result2);

}

TEST_CASE("test_jsonpath_aggregation")
{
    json val = json::parse(R"(
{
  "firstName": "John",
  "lastName" : "doe",
  "age"      : 26,
  "address"  : {
    "streetAddress": "naist street",
    "city"         : "Nara",
    "postalCode"   : "630-0192"
  },
  "phoneNumbers": [
    {
      "type"  : "iPhone",
      "number": "0123-4567-8888"
    },
    {
      "type"  : "home",
      "number": "0123-4567-8910"
    }
  ]
}
    )");

    json expected = json::parse(R"(
["John","doe"]
)");

    json result2 = json_query(val, "$['firstName','lastName']");
    CHECK(expected == result2);

    json result3 = json_query(val, "$[\"firstName\",\"lastName\"]");
    CHECK(expected == result3);

    json expected4 = json::parse(R"(
["John","Nara"]
)");
    std::string path4 = "$..['firstName','city']";

    json result4 = json_query(val, path4);
    CHECK(expected4 == result4);
}

TEST_CASE("test_jsonpath_aggregation2")
{
    json val = json::parse(R"(
{ "store": {
    "book": [ 
          { "author": "Nigel Rees"
          },
          { "author": "Evelyn Waugh"
          },
          { "author": "Herman Melville"
          }
        ]
    }  
}
    )");

    json result = json_query(val, "$..book[(@.length - 1),(@.length - 2)]");

    json expected = json::parse(R"(
[{"author": "Herman Melville"},{"author": "Evelyn Waugh"}]
)");
    CHECK(expected == result);
}

TEST_CASE("test_jsonpath_aggregation3")
{
    json val = json::parse(R"(
{
  "firstName": "John",
  "lastName" : "doe",
  "age"      : 26,
  "address"  : {
    "streetAddress": "naist street",
    "city"         : "Nara",
    "postalCode"   : "630-0192"
  },
  "phoneNumbers": [
    {
      "type"  : "iPhone",
      "number": "0123-4567-8888"
    },
    {
      "type"  : "home",
      "number": "0123-4567-8910"
    }
  ]
}
    )");

    json expected2 = json::parse(R"(
["iPhone","0123-4567-8888","home","0123-4567-8910"]
)");

    json result2 = json_query(val, "$..['type','number']");
    CHECK(expected2 == result2);
}

TEST_CASE("test_jsonpath_aggregation4")
{
    json val = json::parse(R"(
{
  "firstName": "John",
  "lastName" : "doe",
  "age"      : 26,
  "address"  : {
    "streetAddress": "naist street",
    "city"         : "Nara",
    "postalCode"   : "630-0192"
  },
  "phoneNumbers": [
    {
      "type"  : "iPhone",
      "number": "0123-4567-8888"
    },
    {
      "type"  : "home",
      "number": "0123-4567-8910"
    }
  ]
}
    )");

    json expected2 = json::parse(R"(
["iPhone","0123-4567-8888","home","0123-4567-8910"]
)");
    json test1 = json_query(val, "$.phoneNumbers");
    //std::cout << test1 << std::endl;
    json test2 = json_query(val, "$[phoneNumbers]");
    //std::cout << test2 << std::endl;
    json test3 = json_query(val, "$..['type']");
    //std::cout << test3 << std::endl;

    json result2 = json_query(val, "$.phoneNumbers..['type','number']");
    CHECK(expected2 == result2);
}

TEST_CASE("test_jsonpath_string_indexation")
{
    json val;
    val["about"] = "I\xe2\x82\xacJ";

    json expected1 = json::array(1,"I");
    json result1 = json_query(val, "$..about[0]");
    CHECK(expected1 == result1);

    json expected2 = json::array(1,"\xe2\x82\xac");
    json result2 = json_query(val, "$..about[1]");
    CHECK(expected2 == result2);

    json expected3 = json::array(1,"J");
    json result3 = json_query(val, "$..about[2]");
    CHECK(expected3 == result3);

    json expected4 = json::array(1,3);
    json result4 = json_query(val, "$..about.length");
    CHECK(expected4 == result4);
}

TEST_CASE("test_union_array_elements")
{
    json val = json::parse(R"(
{ "store": {
    "book": [ 
          { "author": "Nigel Rees"
          },
          { "author": "Evelyn Waugh"
          },
          { "author": "Herman Melville"
          }
        ]
    },
  "Roman": {
    "book": [ 
          { "author": "Tolstoy L"
          },
          { "author": "Tretyakovskiy R"
          },
          { "author": "Kulik M"
          }
        ]
    }  
}
    )");

    json expected1 = json::parse(R"(
[
    { "author": "Kulik M"},
    { "author": "Herman Melville"}
]
    )");
    json result1 = json_query(val, "$..book[-1]");
    CHECK(expected1 == result1);

    json expected2 = json::parse(R"(
[
    {
        "author": "Kulik M"
    },
    {
        "author": "Tolstoy L"
    },
    {
        "author": "Herman Melville"
    },
    {
        "author": "Nigel Rees"
    }
]
    )");
    json result2 = json_query(val, "$..book[-1,-3]");
    CHECK(expected2 == result2);

    json expected3 = expected2;
    json result3 = json_query(val, "$..book[-1,(@.length - 3)]");
    CHECK(expected3 == result3);

    json expected4 = expected2;
    json result4 = json_query(val, "$..book[(@.length - 1),-3]");
    CHECK(expected4 == result4);
}

TEST_CASE("test_array_slice_operator")
{
    //jsonpath_fixture fixture;

    json root = json::parse(jsonpath_fixture::store_text());

    json result1 = json_query(root,"$..book[1:2].author");
    json expected1 = json::parse(R"(
[
   "Evelyn Waugh"
]
    )");
    CHECK(expected1 == result1);

    json result2 = json_query(root,"$..book[1:3:2].author");
    json expected2 = expected1;
    CHECK(expected2 == result2);

    json result3 = json_query(root,"$..book[1:4:2].author");
    json expected3 = json::parse(R"(
[
   "Evelyn Waugh",
   "J. R. R. Tolkien"
]    
    )");
    CHECK(expected3 == result3);

    json result4 = json_query(root,"$..book[1:4:2,0].author");
    json expected4 = json::parse(R"(
[
    "Evelyn Waugh",
    "J. R. R. Tolkien",
    "Nigel Rees"
]    
    )");
    CHECK(expected4 == result4);

    json result5 = json_query(root,"$..book[1::2,0].author");
    json expected5 = json::parse(R"(
[
    "Evelyn Waugh",
    "J. R. R. Tolkien",
    "Nigel Rees"
]    
    )");
    CHECK(expected5 == result5);

}

TEST_CASE("test_replace")
{
    json j;
    try
    {
        j = json::parse(R"(
{"store":
{"book": [
{"category": "reference",
"author": "Margaret Weis",
"title": "Dragonlance Series",
"price": 31.96}, {"category": "reference",
"author": "Brent Weeks",
"title": "Night Angel Trilogy",
"price": 14.70
}]}}
)");
    }
    catch (const parse_error& e)
    {
        std::cout << e.what() << std::endl;
    }

    //std::cout << "!!!test_replace" << std::endl;
    //std::cout << ("1\n") << pretty_print(j) << std::endl;

    CHECK(31.96 == Approx(j["store"]["book"][0]["price"].as<double>()).epsilon(0.001));

    json_replace(j,"$..book[?(@.price==31.96)].price", 30.9);

    CHECK(30.9 == Approx(j["store"]["book"][0]["price"].as<double>()).epsilon(0.001));

    //std::cout << ("2\n") << pretty_print(j) << std::endl;
}

TEST_CASE("test_max_pre")
{
    std::string path = "$.store.book[*].price";
    json result = json_query(store,path);

    //std::cout << result << std::endl;
}

TEST_CASE("test_max")
{
    std::string path = "$.store.book[?(@.price < max($.store.book[*].price))].title";

    json expected = json::parse(R"(
["Sayings of the Century","Sword of Honour","Moby Dick"]
    )");

    json result = json_query(store,path);
    CHECK(expected == result);

    //std::cout << result << std::endl;
}

TEST_CASE("test_min")
{
    std::string path = "$.store.book[?(@.price > min($.store.book[*].price))].title";

    json expected = json::parse(R"(
["Sword of Honour","Moby Dick","The Lord of the Rings"]
    )");

    json result = json_query(store,path);
    CHECK(expected == result);
}

TEST_CASE("test_ws1")
{
    json result = json_query(store,"$..book[ ?(( @.price > 8 ) && (@.price < 12)) ].author");

    json expected = json::parse(R"(
[
   "Nigel Rees",
   "Herman Melville"
]
)");

    CHECK(expected == result);
}

TEST_CASE("test_select_two")
{
    json j = json::parse(R"(
[
  {
    "a": 5,
    "b": 500,
    "c": 5000
  },
  {
    "a": 6,
    "b": 600,
    "c": 6000
  },
  {
    "a": 7,
    "b": 700,
    "c": 7000
  }
]
)");

    json result = json_query(j,"$..*[?((@.a == 5 && @.b == 500) || (@.a == 6 && @.b == 600))]");

    json expected = json::parse(R"(
[
  {
    "a": 5,
    "b": 500,
    "c": 5000
  },
  {
    "a": 6,
    "b": 600,
    "c": 6000
  }
]
)");

    CHECK(expected == result);
}

TEST_CASE("test_select_length_4")
{
    json j = json::parse(R"(
[
      {
        "result": [
          1,
          2,
          3,
          4
        ]
      }
]

)");

    json result = json_query(j,"$..[?(@.result.length == 4)]");

    json expected = json::parse(R"(
[{"result":[1,2,3,4]}]
    )");

    CHECK(expected == result);
}

TEST_CASE("test_select_length_4_2")
{

    json result = json_query(complex_json,"$..[?(@.result.length == 4)]");

    json expected = json::parse(R"(
[{"result":[1,2,3,4]},{"result":[1,2,3,4]}]
    )");

    CHECK(expected == result);
}

TEST_CASE("test_select_length_4_2_plus")
{

    json result = json_query(complex_json,"$..[?(@.id == 10)]..[?(@.result.length == 4)]");
    //std::cout << result << std::endl;

    json expected = json::parse(R"(
[{"result":[1,2,3,4]}]
    )");

    CHECK(expected == result);
}

TEST_CASE("test_select_length_4_2_plus_plus")
{

    json result = json_query(complex_json,"$..[?(@.result.length == 4)][?(@.result[0] == 3 || @.result[1] == 3 || @.result[2] == 3 || @.result[3] == 3)]");
    //std::cout << result << std::endl;

    json expected = json::parse(R"(
[{"result":[1,2,3,4]},{"result":[1,2,3,4]}]
    )");

    CHECK(expected == result);
}

TEST_CASE("test_nested")
{
    json j = json::parse(R"(
{
    "id" : 10,
    "b": {"id" : 10} 
}        
)");

    json result = json_query(j,"$..[?(@.id == 10)]");

    json expected = json::parse(R"(
[
   {
      "id" : 10,
      "b" : {
         "id" : 10
      }
   },
   {
      "id" : 10
   }
]
)");

    CHECK(expected == result);
}

TEST_CASE("test_array_nested")
{
    json j = json::parse(R"(
{
    "a" : [
        { 
            "id" : 10,
            "b": {"id" : 10} 
        }
    ]
}        
)");

    json result = json_query(j,"$..[?(@.id == 10)]");

    json expected = json::parse(R"(
[
   {
      "id" : 10,
      "b" : {
         "id" : 10
      }
   },
   {
      "id" : 10
   }
]
)");

    CHECK(expected == result);
}

TEST_CASE("test_array_array_nested")
{
    json j = json::parse(R"(
{
    "a" : [[
        { 
            "id" : 10,
            "b": {"id" : 10} 
        }
    ]]
}        
)");

    json result = json_query(j,"$..[?(@.id == 10)]");

    json expected = json::parse(R"(
[
   {
      "id" : 10,
      "b" : {
         "id" : 10
      }
   },
   {
      "id" : 10
   }
]
)");

    CHECK(expected == result);
}




