// Copyright 2013 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JSON_SERIALIZER_HPP
#define JSONCONS_JSON_SERIALIZER_HPP

#include <string>
#include <sstream>
#include <vector>
#include <istream>
#include <ostream>
#include <cstdlib>
#include <limits> // std::numeric_limits
#include <fstream>
#include <memory>
#include <jsoncons/json_exception.hpp>
#include <jsoncons/jsoncons_utilities.hpp>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/bignum.hpp>
#include <jsoncons/json_serializing_options.hpp>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/detail/writer.hpp>
#include <jsoncons/detail/print_number.hpp>

namespace jsoncons {

template<class CharT,class Writer=detail::stream_char_writer<CharT>>
class basic_json_serializer final : public basic_json_content_handler<CharT>
{
public:
    using typename basic_json_content_handler<CharT>::string_view_type;
    typedef Writer writer_type;
    typedef typename Writer::output_type output_type;
    typedef typename basic_json_serializing_options<CharT>::string_type string_type;

private:
    enum class structure_type {object, array};

    class line_split_context
    {
        structure_type type_;
        size_t count_;
        line_split_kind split_lines_;
        bool indent_before_;
        bool unindent_after_;
    public:
        line_split_context(structure_type type)
           : type_(type), count_(0), split_lines_(line_split_kind::same_line), indent_before_(false), unindent_after_(false)
        {
        }
        line_split_context(structure_type type, line_split_kind split_lines, bool indent_once)
           : type_(type), count_(0), split_lines_(split_lines), indent_before_(indent_once), unindent_after_(false)
        {
        }

        size_t count() const
        {
            return count_;
        }

        void increment_count()
        {
            ++count_;
        }

        bool unindent_after() const
        {
            return unindent_after_;
        }

        void unindent_after(bool value) 
        {
            unindent_after_ = value;
        }

        bool is_object() const
        {
            return type_ == structure_type::object;
        }

        bool is_array() const
        {
            return type_ == structure_type::array;
        }

        bool is_same_line() const
        {
            return split_lines_ == line_split_kind::same_line;
        }

        bool is_new_line() const
        {
            return split_lines_ == line_split_kind::new_line;
        }

        bool is_multi_line() const
        {
            return split_lines_ == line_split_kind::multi_line;
        }

        bool is_indent_once() const
        {
            return count_ == 0 ? indent_before_ : false;
        }

    };

    int indent_width_;
    bool can_write_nan_replacement_;
    bool can_write_pos_inf_replacement_;
    bool can_write_neg_inf_replacement_;
    string_type nan_replacement_;
    string_type pos_inf_replacement_;
    string_type neg_inf_replacement_;
    bool escape_all_non_ascii_;
    bool escape_solidus_;
    byte_string_chars_format byte_string_format_;
    bignum_chars_format bignum_format_;
    line_split_kind object_object_split_lines_;
    line_split_kind object_array_split_lines_;
    line_split_kind array_array_split_lines_;
    line_split_kind array_object_split_lines_;

    std::vector<line_split_context> stack_;
    int indent_;
    bool indenting_;
    detail::print_double fp_;
    Writer writer_;

    // Noncopyable and nonmoveable
    basic_json_serializer(const basic_json_serializer&) = delete;
    basic_json_serializer& operator=(const basic_json_serializer&) = delete;
public:
    basic_json_serializer(output_type& os)
        : basic_json_serializer(os, indenting::no_indent)
    {
    }

    basic_json_serializer(output_type& os, indenting line_indent)
        : basic_json_serializer(os, basic_json_serializing_options<CharT>(), line_indent)
    {
    }

    basic_json_serializer(output_type& os, const basic_json_write_options<CharT>& options)
       : basic_json_serializer(os, options, indenting::no_indent)
    {
    }

    basic_json_serializer(output_type& os, 
                          const basic_json_write_options<CharT>& options, 
                          indenting line_indent)
       : indent_width_(options.indent()),
         can_write_nan_replacement_(options.can_write_nan_replacement()),
         can_write_pos_inf_replacement_(options.can_write_pos_inf_replacement()),
         can_write_neg_inf_replacement_(options.can_write_neg_inf_replacement()),
         nan_replacement_(options.nan_replacement()),
         pos_inf_replacement_(options.pos_inf_replacement()),
         neg_inf_replacement_(options.neg_inf_replacement()),
         escape_all_non_ascii_(options.escape_all_non_ascii()),
         escape_solidus_(options.escape_solidus()),
         byte_string_format_(options.byte_string_format()),
         bignum_format_(options.bignum_format()),
         object_object_split_lines_(options.object_object_split_lines()),
         object_array_split_lines_(options.object_array_split_lines()),
         array_array_split_lines_(options.array_array_split_lines()),
         array_object_split_lines_(options.array_object_split_lines()),
         indent_(0), 
         indenting_(line_indent == indenting::indent),  
         fp_(floating_point_options(options.floating_point_format(), 
                                    options.precision(),
                                    0)),
         writer_(os)
    {
    }

    ~basic_json_serializer()
    {
    }


#if !defined(JSONCONS_NO_DEPRECATED)

    basic_json_serializer(output_type& os, bool pprint)
       : basic_json_serializer(os, basic_json_serializing_options<CharT>(), (pprint ? indenting::indent : indenting::no_indent))
    {
    }

    basic_json_serializer(output_type& os, const basic_json_serializing_options<CharT>& options, bool pprint)
        : basic_json_serializer(os, basic_json_serializing_options<CharT>(), (pprint ? indenting::indent : indenting::no_indent))
    {
    }
#endif

private:
    void escape_string(const CharT* s, size_t length)
    {
        const CharT* begin = s;
        const CharT* end = s + length;
        for (const CharT* it = begin; it != end; ++it)
        {
            CharT c = *it;
            switch (c)
            {
            case '\\':
                writer_.put('\\'); 
                writer_.put('\\');
                break;
            case '"':
                writer_.put('\\'); 
                writer_.put('\"');
                break;
            case '\b':
                writer_.put('\\'); 
                writer_.put('b');
                break;
            case '\f':
                writer_.put('\\');
                writer_.put('f');
                break;
            case '\n':
                writer_.put('\\');
                writer_.put('n');
                break;
            case '\r':
                writer_.put('\\');
                writer_.put('r');
                break;
            case '\t':
                writer_.put('\\');
                writer_.put('t');
                break;
            default:
                if (escape_solidus_ && c == '/')
                {
                    writer_.put('\\');
                    writer_.put('/');
                }
                else if (is_control_character(c) || escape_all_non_ascii_)
                {
                    // convert utf8 to codepoint
                    unicons::sequence_generator<const CharT*> g(it,end,unicons::conv_flags::strict);
                    if (g.done() || g.status() != unicons::conv_errc())
                    {
                        JSONCONS_THROW(json_exception_impl<std::runtime_error>("Invalid codepoint"));
                    }
                    uint32_t cp = g.get().codepoint();
                    it += (g.get().length() - 1);
                    if (is_non_ascii_codepoint(cp) || is_control_character(c))
                    {
                        if (cp > 0xFFFF)
                        {
                            cp -= 0x10000;
                            uint32_t first = (cp >> 10) + 0xD800;
                            uint32_t second = ((cp & 0x03FF) + 0xDC00);

                            writer_.put('\\');
                            writer_.put('u');
                            writer_.put(to_hex_character(first >> 12 & 0x000F));
                            writer_.put(to_hex_character(first >> 8  & 0x000F));
                            writer_.put(to_hex_character(first >> 4  & 0x000F));
                            writer_.put(to_hex_character(first     & 0x000F));
                            writer_.put('\\');
                            writer_.put('u');
                            writer_.put(to_hex_character(second >> 12 & 0x000F));
                            writer_.put(to_hex_character(second >> 8  & 0x000F));
                            writer_.put(to_hex_character(second >> 4  & 0x000F));
                            writer_.put(to_hex_character(second     & 0x000F));
                        }
                        else
                        {
                            writer_.put('\\');
                            writer_.put('u');
                            writer_.put(to_hex_character(cp >> 12 & 0x000F));
                            writer_.put(to_hex_character(cp >> 8  & 0x000F));
                            writer_.put(to_hex_character(cp >> 4  & 0x000F));
                            writer_.put(to_hex_character(cp     & 0x000F));
                        }
                    }
                    else
                    {
                        writer_.put(c);
                    }
                }
                else
                {
                    writer_.put(c);
                }
                break;
            }
        }
    }
    // Implementing methods
    void do_begin_document() override
    {
    }

    void do_end_document() override
    {
        writer_.flush();
    }

    void do_begin_object(const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            if (!stack_.empty())
            {
                if (stack_.back().count() > 0)
                {
                    writer_. put(',');
                }
            }
        }

        if (indenting_)
        {
            if (!stack_.empty() && stack_.back().is_object())
            {
                stack_.push_back(line_split_context(structure_type::object,object_object_split_lines_, false));
            }
            else if (!stack_.empty())
            {
                if (array_object_split_lines_ != line_split_kind::same_line)
                {
                    stack_.back().unindent_after(true);
                    stack_.push_back(line_split_context(structure_type::object,array_object_split_lines_, false));
                    write_indent1();
                }
                else
                {
                    stack_.push_back(line_split_context(structure_type::object,array_object_split_lines_, false));
                }
            }
            else 
            {
                stack_.push_back(line_split_context(structure_type::object, line_split_kind::multi_line, false));
            }
            indent();
        }
        else
        {
            stack_.push_back(line_split_context(structure_type::object));
        }
        writer_.put('{');
    }

    void do_end_object(const serializing_context&) override
    {
        JSONCONS_ASSERT(!stack_.empty());
        if (indenting_)
        {
            unindent();
            if (stack_.back().unindent_after())
            {
                write_indent();
            }
        }
        stack_.pop_back();
        writer_.put('}');

        end_value();
    }


    void do_begin_array(const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            if (!stack_.empty())
            {
                if (stack_.back().count() > 0)
                {
                    writer_. put(',');
                }
            }
        }
        if (indenting_)
        {
            if (!stack_.empty())
            {
                if (stack_.back().is_object())
                {
                    writer_.put('[');
                    indent();
                    if (object_array_split_lines_ != line_split_kind::same_line)
                    {
                        stack_.push_back(line_split_context(structure_type::array,object_array_split_lines_,true));
                    }
                    else
                    {
                        stack_.push_back(line_split_context(structure_type::array,object_array_split_lines_,false));
                    }
                }
                else // array
                {
                    if (array_array_split_lines_ == line_split_kind::same_line)
                    {
                        if (stack_.back().is_multi_line())
                        {
                            write_indent();
                        }
                        stack_.push_back(line_split_context(structure_type::array,line_split_kind::same_line, false));
                        indent();
                    }
                    else if (array_array_split_lines_ == line_split_kind::multi_line)
                    {
                        write_indent();
                        stack_.push_back(line_split_context(structure_type::array,array_array_split_lines_, false));
                        indent();
                    }
                    else // new_line
                    {
                        write_indent();
                        stack_.push_back(line_split_context(structure_type::array,array_array_split_lines_, false));
                        indent();
                    }
                    writer_.put('[');
                }
            }
            else 
            {
                stack_.push_back(line_split_context(structure_type::array, line_split_kind::multi_line, false));
                writer_.put('[');
                indent();
            }
        }
        else
        {
            stack_.push_back(line_split_context(structure_type::array));
            writer_.put('[');
        }
    }

    void do_end_array(const serializing_context&) override
    {
        JSONCONS_ASSERT(!stack_.empty());
        if (indenting_)
        {
            unindent();
            if (stack_.back().unindent_after())
            {
                write_indent();
            }
        }
        stack_.pop_back();
        writer_.put(']');
        end_value();
    }

    void do_name(const string_view_type& name, const serializing_context&) override
    {
        if (!stack_.empty())
        {
            if (stack_.back().count() > 0)
            {
                writer_. put(',');
            }
            if (indenting_)
            {
                if (stack_.back().is_multi_line())
                {
                    write_indent();
                }
            }
        }

        writer_.put('\"');
        escape_string(name.data(), name.length());
        writer_.put('\"');
        writer_.put(':');
        if (indenting_)
        {
            writer_.put(' ');
        }
    }

    void do_null_value(const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }

        writer_.write(detail::null_literal<CharT>().data(), 
                      detail::null_literal<CharT>().size());

        end_value();
    }

    void do_string_value(const string_view_type& value, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }

        writer_. put('\"');
        escape_string(value.data(), value.length());
        writer_. put('\"');

        end_value();
    }

    void do_byte_string_value(const uint8_t* data, size_t length, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }
        switch (byte_string_format_)
        {
            case byte_string_chars_format::base16:
            {
                std::basic_string<CharT> s;
                encode_base16(data,length,s);
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
            case byte_string_chars_format::base64url:
            {
                std::basic_string<CharT> s;
                encode_base64url(data,length,s);
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
            default:
            {
                std::basic_string<CharT> s;
                encode_base64(data, length, s);
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
        }

        end_value();
    }

    void do_bignum_value(int signum, const uint8_t* data, size_t length, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }

        switch (bignum_format_)
        {
            case bignum_chars_format::integer:
            {
                bignum n = bignum(signum, data, length);
                std::basic_string<CharT> s;
                n.dump(s);
                writer_.write(s.data(),s.size());
                break;
            }
            case bignum_chars_format::base64:
            {
                std::basic_string<CharT> s;
                encode_base64(data, length, s);
                if (signum == -1)
                {
                    s.insert(s.begin(), '~');
                }
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
            case bignum_chars_format::base64url:
            {
                std::basic_string<CharT> s;
                encode_base64url(data, length, s);
                if (signum == -1)
                {
                    s.insert(s.begin(), '~');
                }
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
            default:
            {
                bignum n = bignum(signum, data, length);
                std::basic_string<CharT> s;
                n.dump(s);
                writer_. put('\"');
                writer_.write(s.data(),s.size());
                writer_. put('\"');
                break;
            }
        }

        end_value();
    }

    void do_double_value(double value, const floating_point_options& fmt, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }

        if ((std::isnan)(value))
        {
            if (can_write_nan_replacement_)
            {
                writer_.write(nan_replacement_.data(),
                              nan_replacement_.length());
            }
            else
            {
                writer_.write(detail::null_literal<CharT>().data(),
                              detail::null_literal<CharT>().length());
            }
        }
        else if (value == std::numeric_limits<double>::infinity())
        {
            if (can_write_pos_inf_replacement_)
            {
                writer_.write(pos_inf_replacement_.data(),
                              pos_inf_replacement_.length());
            }
            else
            {
                writer_.write(detail::null_literal<CharT>().data(),
                              detail::null_literal<CharT>().length());
            }
        }
        else if (!(std::isfinite)(value))
        {
            if (can_write_neg_inf_replacement_)
            {
                writer_.write(neg_inf_replacement_.data(),
                              neg_inf_replacement_.length());
            }
            else
            {
                writer_.write(detail::null_literal<CharT>().data(),
                              detail::null_literal<CharT>().length());
            }
        }
        else
        {
            fp_(value, fmt, writer_);
        }

        end_value();
    }

    void do_integer_value(int64_t value, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }
        detail::print_integer(value, writer_);
        end_value();
    }

    void do_uinteger_value(uint64_t value, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }
        detail::print_uinteger(value, writer_);
        end_value();
    }

    void do_bool_value(bool value, const serializing_context&) override
    {
        if (!stack_.empty() && stack_.back().is_array())
        {
            begin_scalar_value();
        }

        if (value)
        {
            writer_.write(detail::true_literal<CharT>().data(),
                          detail::true_literal<CharT>().length());
        }
        else
        {
            writer_.write(detail::false_literal<CharT>().data(),
                          detail::false_literal<CharT>().length());
        }

        end_value();
    }

    void begin_scalar_value()
    {
        if (!stack_.empty())
        {
            if (stack_.back().count() > 0)
            {
                writer_. put(',');
            }
            if (indenting_)
            {
                if (stack_.back().is_multi_line() || stack_.back().is_indent_once())
                {
                    write_indent();
                }
            }
        }
    }

    void end_value()
    {
        if (!stack_.empty())
        {
            stack_.back().increment_count();
        }
    }

    void indent()
    {
        indent_ += static_cast<int>(indent_width_);
    }

    void unindent()
    {
        indent_ -= static_cast<int>(indent_width_);
    }

    void write_indent()
    {
        if (!stack_.empty())
        {
            stack_.back().unindent_after(true);
        }
        writer_. put('\n');
        for (int i = 0; i < indent_; ++i)
        {
            writer_. put(' ');
        }
    }

    void write_indent1()
    {
        writer_. put('\n');
        for (int i = 0; i < indent_; ++i)
        {
            writer_. put(' ');
        }
    }
};

typedef basic_json_serializer<char,detail::stream_char_writer<char>> json_serializer;
typedef basic_json_serializer<wchar_t,detail::stream_char_writer<wchar_t>> wjson_serializer;

typedef basic_json_serializer<char,detail::string_writer<char>> json_string_serializer;
typedef basic_json_serializer<wchar_t,detail::string_writer<wchar_t>> wjson_string_serializer;

}
#endif
