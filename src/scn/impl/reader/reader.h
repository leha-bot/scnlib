// Copyright 2017 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#pragma once

#include <scn/detail/args.h>
#include <scn/detail/format_string.h>
#include <scn/detail/xchar.h>

#include <scn/impl/reader/bool_reader.h>
#include <scn/impl/reader/code_unit_and_point_reader.h>
#include <scn/impl/reader/float_reader.h>
#include <scn/impl/reader/integer_reader.h>
#include <scn/impl/reader/pointer_reader.h>
#include <scn/impl/reader/regex_reader.h>
#include <scn/impl/reader/string_reader.h>
#include <scn/impl/util/contiguous_context.h>

namespace scn {
SCN_BEGIN_NAMESPACE

namespace impl {
template <typename Range>
eof_expected<simple_borrowed_iterator_t<Range>> skip_ws_before_if_required(
    bool is_required,
    Range&& range,
    detail::locale_ref loc)
{
    if (auto e = eof_check(range); SCN_UNLIKELY(!e)) {
        return unexpected(e);
    }

    if (!is_required) {
        return ranges::begin(range);
    }

    return skip_classic_whitespace(SCN_FWD(range));
}

template <typename T, typename CharT>
constexpr auto make_reader()
{
    if constexpr (std::is_same_v<T, bool>) {
        return reader_impl_for_bool<CharT>{};
    }
    else if constexpr (std::is_same_v<T, char>) {
        return reader_impl_for_char<CharT>{};
    }
    else if constexpr (std::is_same_v<T, wchar_t>) {
        return reader_impl_for_wchar<CharT>{};
    }
    else if constexpr (std::is_same_v<T, char32_t>) {
        return reader_impl_for_code_point<CharT>{};
    }
    else if constexpr (std::is_same_v<T, std::string_view> ||
                       std::is_same_v<T, std::wstring_view>) {
        return reader_impl_for_string<CharT>{};
    }
    else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, std::wstring>) {
        return reader_impl_for_string<CharT>{};
    }
    else if constexpr (std::is_same_v<T, regex_matches> ||
                       std::is_same_v<T, wregex_matches>) {
        return reader_impl_for_regex_matches<CharT>{};
    }
    else if constexpr (std::is_same_v<T, void*>) {
        return reader_impl_for_voidptr<CharT>{};
    }
    else if constexpr (std::is_floating_point_v<T>) {
        return reader_impl_for_float<CharT>{};
    }
    else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, char> &&
                       !std::is_same_v<T, wchar_t> &&
                       !std::is_same_v<T, char32_t> &&
                       !std::is_same_v<T, bool>) {
        return reader_impl_for_int<CharT>{};
    }
    else {
        return reader_impl_for_monostate<CharT>{};
    }
}

template <typename Context>
struct default_arg_reader {
    using context_type = Context;
    using char_type = typename context_type::char_type;
    using args_type = typename context_type::args_type;

    using range_type = typename context_type::range_type;
    using iterator = ranges::iterator_t<range_type>;

    template <typename Reader, typename Range, typename T>
    scan_expected<ranges::iterator_t<Range>> impl(Reader& rd,
                                                  const Range& rng,
                                                  T& value)
    {
        SCN_TRY(it,
                skip_ws_before_if_required(rd.skip_ws_before_read(), rng, loc)
                    .transform_error(make_eof_scan_error));
        return rd.read_default(ranges::subrange{it, ranges::end(rng)}, value,
                               loc);
    }

    template <typename T>
    scan_expected<iterator> operator()(T& value)
    {
        if constexpr (!detail::is_type_disabled<T> &&
                      std::is_same_v<
                          context_type,
                          basic_contiguous_scan_context<char_type>>) {
            auto rd = make_reader<T, char_type>();
            return impl(rd, range, value);
        }
        else if constexpr (!detail::is_type_disabled<T>) {
            auto rd = make_reader<T, char_type>();
            if (!is_segment_contiguous(range)) {
                return impl(rd, range, value);
            }
            auto crange = get_as_contiguous(range);
            SCN_TRY(it, impl(rd, crange, value));
            return ranges_polyfill::batch_next(
                ranges::begin(range), ranges::distance(crange.begin(), it));
        }
        else {
            SCN_EXPECT(false);
            SCN_UNREACHABLE;
        }
    }

    basic_scan_context<char_type> make_custom_ctx()
    {
        if constexpr (std::is_same_v<
                          context_type,
                          basic_contiguous_scan_context<char_type>>) {
            auto it =
                typename detail::basic_scan_buffer<char_type>::forward_iterator{
                    std::basic_string_view<char_type>(range.data(),
                                                      range.size()),
                    0};
            return {it, args, loc};
        }
        else {
            return {range.begin(), args, loc};
        }
    }

    scan_expected<iterator> operator()(
        typename context_type::arg_type::handle h)
    {
        if constexpr (!detail::is_type_disabled<void>) {
            basic_scan_parse_context<char_type> parse_ctx{{}};
            auto ctx = make_custom_ctx();
            if (auto e = h.scan(parse_ctx, ctx); !e) {
                return unexpected(e);
            }

            if constexpr (std::is_same_v<
                              context_type,
                              basic_contiguous_scan_context<char_type>>) {
                return range.begin() + ctx.begin().position();
            }
            else {
                return ctx.begin();
            }
        }
        else {
            SCN_EXPECT(false);
            SCN_UNREACHABLE;
        }
    }

    range_type range;
    args_type args;
    detail::locale_ref loc;
};

template <typename Iterator>
using skip_fill_result = std::pair<Iterator, std::ptrdiff_t>;

template <typename Range>
auto skip_fill(Range&& range,
               std::ptrdiff_t max_width,
               const detail::fill_type& fill,
               bool want_skipped_width)
    -> scan_expected<skip_fill_result<simple_borrowed_iterator_t<Range>>>
{
    SCN_EXPECT(ranges::begin(range) != ranges::end(range));

    using char_type = detail::char_t<Range>;
    using result_type = skip_fill_result<simple_borrowed_iterator_t<Range>>;

    auto it = ranges::begin(range);
    if (fill.size() <= sizeof(char_type)) {
        const auto fill_ch = fill.template get_code_unit<char_type>();
        const auto pred = [=](char_type ch) { return ch == fill_ch; };

        if (max_width == 0) {
            it = read_while_code_unit(range, pred);
        }
        else {
            it =
                read_while_code_unit(take_width(range, max_width), pred).base();
        }

        if (want_skipped_width) {
            auto prefix_width =
                static_cast<std::ptrdiff_t>(
                    calculate_text_width(static_cast<char32_t>(fill_ch))) *
                ranges::distance(ranges::begin(range), it);
            return result_type{it, prefix_width};
        }
        return result_type{it, 0};
    }

    const auto fill_chars = fill.template get_code_units<char_type>();
    if (max_width == 0) {
        it = read_while_code_units(range, fill_chars);
    }
    else {
        it = read_while_code_units(take_width(range, max_width), fill_chars)
                 .base();
    }

    if (want_skipped_width) {
        auto prefix_width =
            static_cast<std::ptrdiff_t>(calculate_text_width(fill_chars)) *
            ranges::distance(ranges::begin(range), it) /
            ranges::ssize(fill_chars);
        return result_type{it, prefix_width};
    }
    return result_type{it, 0};
}

template <typename Context>
struct arg_reader {
    using context_type = Context;
    using char_type = typename context_type::char_type;

    using range_type = typename context_type::range_type;
    using iterator = ranges::iterator_t<range_type>;

    template <typename Reader, typename Range, typename T>
    scan_expected<ranges::iterator_t<Range>> impl(Reader& rd,
                                                  const Range& rng,
                                                  T& value)
    {
        auto it = ranges::begin(rng);
        SCN_EXPECT(it != ranges::end(rng));

        const bool need_skipped_width =
            specs.width != 0 || specs.precision != 0;

        // Read prefix
        std::ptrdiff_t prefix_width = 0;
        if (specs.align == detail::align_type::none &&
            rd.skip_ws_before_read()) {
            // Default alignment:
            // Skip preceding whitespace, if required by the reader
            SCN_TRY_ASSIGN(it, skip_classic_whitespace(rng).transform_error(
                                   make_eof_scan_error));

            if (need_skipped_width) {
                prefix_width = calculate_text_width(
                    make_contiguous_buffer(
                        ranges::subrange{ranges::begin(rng), it})
                        .view());
            }
        }
        else if (specs.align == detail::align_type::right ||
                 specs.align == detail::align_type::center) {
            SCN_TRY(prefix_skip_result,
                    skip_fill(rng, specs.precision, specs.fill,
                              need_skipped_width));
            std::tie(it, prefix_width) = prefix_skip_result;
        }
        const auto prefix_end_it = it;

        // Read value
        if (specs.precision != 0) {
            if (specs.precision <= prefix_width) {
                return unexpected_scan_error(
                    scan_error::invalid_scanned_value,
                    "Too many spaces before value, precision exceeded before "
                    "reading value");
            }

            SCN_TRY(w_it, rd.read_specs(
                              take_width(ranges::subrange{it, ranges::end(rng)},
                                         static_cast<std::ptrdiff_t>(
                                             specs.precision - prefix_width)),
                              specs, value, loc));
            it = w_it.base();
        }
        else {
            SCN_TRY_ASSIGN(
                it, rd.read_specs(ranges::subrange{it, ranges::end(rng)}, specs,
                                  value, loc));
        }
        const auto value_end_it = it;

        std::ptrdiff_t value_width = 0;
        if (need_skipped_width) {
            value_width = calculate_text_width(
                make_contiguous_buffer(ranges::subrange{prefix_end_it, it})
                    .view());
        }

        // Read postfix
        std::ptrdiff_t postfix_width = 0;
        if (it != ranges::end(rng)) {
            if (specs.align == detail::align_type::left ||
                specs.align == detail::align_type::center) {
                SCN_TRY(postfix_skip_result,
                        skip_fill(ranges::subrange{it, ranges::end(rng)},
                                  specs.precision - value_width - prefix_width,
                                  specs.fill, need_skipped_width));
                std::tie(it, postfix_width) = postfix_skip_result;
            }
            else if (specs.align == detail::align_type::none &&
                     !rd.skip_ws_before_read() &&
                     ((specs.width != 0 &&
                       prefix_width + value_width < specs.width) ||
                      (specs.precision != 0 &&
                       prefix_width + value_width < specs.precision))) {
                SCN_TRY_ASSIGN(it,
                               skip_classic_whitespace(
                                   ranges::subrange{it, ranges::end(rng)}, true)
                                   .transform_error(make_eof_scan_error));
                if (need_skipped_width) {
                    postfix_width = calculate_text_width(
                        make_contiguous_buffer(
                            ranges::subrange{value_end_it, it})
                            .view());
                }
            }
        }

        if (specs.width != 0) {
            if (prefix_width + value_width + postfix_width < specs.width) {
                return unexpected_scan_error(
                    scan_error::invalid_scanned_value,
                    "Scanned value too narrow, width did not exceed what "
                    "was "
                    "specified in the format string");
            }
        }
        if (specs.precision != 0) {
            if (prefix_width + value_width + postfix_width > specs.precision) {
                return unexpected_scan_error(
                    scan_error::invalid_scanned_value,
                    "Scanned value too wide, width exceeded the specified "
                    "precision");
            }
        }

        return it;
    }

    template <typename T>
    scan_expected<iterator> operator()(T& value)
    {
        if constexpr (!detail::is_type_disabled<T> &&
                      std::is_same_v<
                          context_type,
                          basic_contiguous_scan_context<char_type>>) {
            auto rd = make_reader<T, char_type>();
            if (auto e = rd.check_specs(specs); SCN_UNLIKELY(!e)) {
                return unexpected(e);
            }

            return impl(rd, range, value);
        }
        else if constexpr (!detail::is_type_disabled<T>) {
            auto rd = make_reader<T, char_type>();
            if (auto e = rd.check_specs(specs); SCN_UNLIKELY(!e)) {
                return unexpected(e);
            }

            if (!is_segment_contiguous(range) || specs.precision != 0 ||
                specs.width != 0) {
                return impl(rd, range, value);
            }

            auto crange = get_as_contiguous(range);
            SCN_TRY(it, impl(rd, crange, value));
            return ranges_polyfill::batch_next(
                ranges::begin(range), ranges::distance(crange.begin(), it));
        }
        else {
            SCN_EXPECT(false);
            SCN_UNREACHABLE;
        }
    }

    scan_expected<iterator> operator()(typename context_type::arg_type::handle)
    {
        SCN_EXPECT(false);
        SCN_UNREACHABLE;
    }

    range_type range;
    const detail::format_specs& specs;
    detail::locale_ref loc;
};

template <typename Context>
struct custom_reader {
    using context_type = Context;
    using char_type = typename context_type::char_type;
    using parse_context_type = typename context_type::parse_context_type;
    using iterator = typename context_type::iterator;

    template <typename T>
    scan_expected<iterator> operator()(T&) const
    {
        SCN_EXPECT(false);
        SCN_UNREACHABLE;
    }

    scan_expected<iterator> operator()(
        typename context_type::arg_type::handle h) const
    {
        if (auto e = h.scan(parse_ctx, ctx); !e) {
            return unexpected(e);
        }
        return {ctx.begin()};
    }

    parse_context_type& parse_ctx;
    context_type& ctx;
};

}  // namespace impl

SCN_END_NAMESPACE
}  // namespace scn
