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
#include <scn/detail/error.h>
#include <scn/detail/input_map.h>

#include <tuple>
#include "scn/detail/erased_range.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    /**
     * Scan result type, containing the unparsed input, and a possible error.
     * The first element in the tuple returned by scan().
     */
    template <typename ResultMappedRange>
    class scan_result {
    public:
        using range_type = ResultMappedRange;

        scan_result() = default;

        template <typename Range,
                  typename = std::enable_if_t<
                      std::is_constructible_v<range_type, Range>>>
        explicit scan_result(Range&& r, scan_error e = {})
            : m_range(SCN_FWD(r)), m_error(SCN_MOVE(e))
        {
        }

        template <typename Range,
                  typename = std::enable_if_t<
                      std::is_constructible_v<range_type, Range>>>
        scan_result(const scan_result<Range>& o)
            : m_range(o.range()), m_error(o.error())
        {
        }
        template <typename Range,
                  typename = std::enable_if_t<
                      std::is_constructible_v<range_type, Range>>>
        scan_result(scan_result<Range>&& o)
            : m_range(SCN_MOVE(o.range())), m_error(SCN_MOVE(o.error()))
        {
        }

        template <typename Range,
                  typename =
                      std::enable_if_t<std::is_assignable_v<range_type, Range>>>
        scan_result& operator=(Range&& r)
        {
            m_range = SCN_FWD(r);
            return *this;
        }
        template <typename Range,
                  typename =
                      std::enable_if_t<std::is_assignable_v<range_type, Range>>>
        scan_result& operator=(scan_result<Range>&& r)
        {
            m_range = SCN_FWD(r.range());
            m_error = SCN_FWD(r.error());
            return *this;
        }

        /// True, if the operation succeeded
        constexpr explicit operator bool() const
        {
            return m_error.operator bool();
        }
        /// True, if the operation succeeded
        SCN_NODISCARD constexpr bool good() const
        {
            return operator bool();
        }

        /// Error, if one occured
        SCN_NODISCARD constexpr scan_error error() const
        {
            return m_error;
        }

        /// The unparsed input
        range_type& range() & SCN_NOEXCEPT
        {
            return m_range;
        }
        const range_type& range() const& SCN_NOEXCEPT
        {
            return m_range;
        }
        range_type&& range() && SCN_NOEXCEPT
        {
            return SCN_MOVE(m_range);
        }
        range_type&& range() const&& SCN_NOEXCEPT
        {
            return SCN_MOVE(m_range);
        }

    private:
        range_type m_range;
        scan_error m_error{};
    };

    template <typename Range>
    scan_result(Range) -> scan_result<Range>;
    template <typename Range>
    scan_result(Range, scan_error) -> scan_result<Range>;

    namespace detail {
        // Make a user-friendly range value from the return value of vscan

        template <typename SourceRange, typename ResultRange>
        auto map_scan_result_range(const SourceRange& source,
                                   const ResultRange& result)
        {
            if constexpr (is_erased_range_or_subrange<ResultRange>::value &&
                          !is_erased_range_or_subrange<SourceRange>::value) {
                using iterator = ranges::iterator_t<const SourceRange&>;
                using sentinel = ranges::sentinel_t<const SourceRange&>;
                constexpr ranges::subrange_kind kind =
                    ranges::sized_range<const SourceRange&>
                        ? ranges::subrange_kind::sized
                        : ranges::subrange_kind::unsized;

                return ranges::subrange<iterator, sentinel, kind>{
                    ranges::next(ranges::begin(source),
                                 result.begin().distance_from_begin()),
                    ranges::end(source)};
            }
            else {
                return result;
            }
        }
#if 0
        // string_view -> self
        // Not affected by Source type
        template <typename CharT, typename Source>
        std::basic_string_view<CharT> map_scan_result_range(
            const Source&,
            const scan_result<std::basic_string_view<CharT>>& result)
            SCN_NOEXCEPT_P(
                std::is_nothrow_constructible_v<std::basic_string_view<CharT>,
                                                const CharT*,
                                                std::size_t>)
        {
            return result.range();
        }

        // istreambuf_subrange -> self
        // Not affected by Source type
        template <typename CharT, typename Source>
        basic_istreambuf_subrange<CharT> map_scan_result_range(
            const Source&,
            const scan_result<basic_istreambuf_subrange<CharT>>& result)
            SCN_NOEXCEPT_P(
                std::is_nothrow_constructible_v<
                    basic_istreambuf_subrange<CharT>,
                    ranges::iterator_t<basic_istreambuf_subrange<CharT>>&,
                    ranges::sentinel_t<basic_istreambuf_subrange<CharT>>&>)
        {
            return result.range();
        }

        // erased_subrange, when Source is an erased type (erased_(sub)range)
        //   -> erased_subrange
        // erased_subrange, when Source is any other range
        //   -> subrange<Source::iterator, Source::sentinel>
        template <typename CharT, typename Source>
        auto map_scan_result_range(
            const Source& source,
            const scan_result<basic_erased_subrange<CharT>>& result)
        {
            if constexpr (is_erased_range_or_subrange<Source>::value) {
                return result.range();
            }
            else {
                using iterator = ranges::iterator_t<const Source&>;
                using sentinel = ranges::sentinel_t<const Source&>;
                constexpr ranges::subrange_kind kind =
                    ranges::sized_range<const Source&>
                        ? ranges::subrange_kind::sized
                        : ranges::subrange_kind::unsized;

                return ranges::subrange<iterator, sentinel, kind>{
                    ranges::next(ranges::begin(source),
                                 result.range().begin().distance_from_begin()),
                    ranges::end(source)};
            }
        }
#endif
    }  // namespace detail

    namespace detail {
        template <typename... T, std::size_t... I>
        std::tuple<T&...> make_ref_tuple_impl(std::tuple<T...>& t,
                                              std::index_sequence<I...>)
        {
            return std::tie(std::get<I>(t)...);
        }

        template <typename... T>
        std::tuple<T&...> make_ref_tuple(std::tuple<T...>& t)
        {
            return make_ref_tuple_impl(
                t, std::make_index_sequence<sizeof...(T)>{});
        }
    }  // namespace detail

    /**
     * The result type returned by scan().
     * Contains a scan_result<R>, and a std::tuple<Args...>.
     * Can be used as-is, or be destructured with structured bindings or
     * std::tie.
     */
    template <typename ResultMappedRange, typename... Args>
    class scan_result_tuple {
    public:
        using result_type = scan_result<ResultMappedRange>;
        using range_type = typename result_type::range_type;
        using tuple_type = std::tuple<Args...>;

        scan_result_tuple() = default;

        template <typename Range,
                  typename = std::enable_if_t<
                      std::is_constructible_v<range_type, Range>>>
        scan_result_tuple(scan_result<Range>&& result, tuple_type values)
            : m_result(SCN_MOVE(result)), m_values(SCN_MOVE(values))
        {
        }

        template <
            typename Range,
            typename... A,
            typename = std::enable_if_t<
                std::is_constructible_v<range_type, const Range&> &&
                std::is_constructible_v<tuple_type, const std::tuple<A...>&>>>
        scan_result_tuple(const scan_result_tuple<Range, A...>& other)
            : m_result(other.result()), m_values(other.values())
        {
        }
        template <typename Range,
                  typename... A,
                  typename = std::enable_if_t<
                      std::is_constructible_v<range_type, Range&&> &&
                      std::is_constructible_v<tuple_type, std::tuple<A...>&&>>>
        scan_result_tuple(scan_result_tuple<Range, A...>&& other)
            : m_result(SCN_MOVE(other.result())),
              m_values(SCN_MOVE(other.values()))
        {
        }

        template <
            typename Range,
            typename... A,
            typename = std::enable_if_t<
                std::is_assignable_v<range_type, const Range&> &&
                std::is_assignable_v<tuple_type, const std::tuple<A...>&>>>
        scan_result_tuple& operator=(
            const scan_result_tuple<Range, A...>& other)
        {
            m_result = other.result();
            m_values = other.values();
            return *this;
        }
        template <typename Range,
                  typename... A,
                  typename = std::enable_if_t<
                      std::is_assignable_v<range_type, Range&&> &&
                      std::is_assignable_v<tuple_type, std::tuple<A...>&&>>>
        scan_result_tuple& operator=(scan_result_tuple<Range, A...>&& other)
        {
            m_result = SCN_MOVE(other.result());
            m_values = SCN_MOVE(other.values());
            return *this;
        }

        /// The scan_result value associated with this result.
        /// Contains the unparsed input, and a possible error.
        /// The first/0th element in this tuple, if accessed with std::get.
        constexpr result_type& result() & SCN_NOEXCEPT
        {
            return m_result;
        }
        constexpr const result_type& result() const& SCN_NOEXCEPT
        {
            return m_result;
        }
        constexpr result_type&& result() && SCN_NOEXCEPT
        {
            return SCN_MOVE(m_result);
        }
        constexpr const result_type&& result() const&& SCN_NOEXCEPT
        {
            return SCN_MOVE(m_result);
        }

        /// The tuple of values, which contain the parsed values.
        constexpr tuple_type& values() & SCN_NOEXCEPT
        {
            return m_values;
        }
        constexpr const tuple_type& values() const& SCN_NOEXCEPT
        {
            return m_values;
        }
        constexpr tuple_type&& values() && SCN_NOEXCEPT
        {
            return SCN_MOVE(m_values);
        }
        constexpr const tuple_type&& values() const&& SCN_NOEXCEPT
        {
            return SCN_MOVE(m_values);
        }

        /// True, if the operation succeeded
        constexpr explicit operator bool() const SCN_NOEXCEPT
        {
            return result().operator bool();
        }
        /// True, if the operation succeeded
        SCN_NODISCARD constexpr bool good() const SCN_NOEXCEPT
        {
            return operator bool();
        }

        /// Error, if any occurred
        SCN_NODISCARD constexpr scan_error error() const SCN_NOEXCEPT
        {
            return result().error();
        }

        /// The unparsed input
        range_type& range() & SCN_NOEXCEPT
        {
            return result().range();
        }
        const range_type& range() const& SCN_NOEXCEPT
        {
            return result().range();
        }
        range_type&& range() && SCN_NOEXCEPT
        {
            return SCN_MOVE(result().range());
        }
        const range_type&& range() const&& SCN_NOEXCEPT
        {
            return SCN_MOVE(result().range());
        }

        // For std::tie support
        operator std::tuple<result_type&, Args&...>()
        {
            return std::tuple_cat(std::tuple<result_type&>{m_result},
                                  detail::make_ref_tuple(m_values));
        }

    private:
        result_type m_result{};
        tuple_type m_values{};
    };

    SCN_END_NAMESPACE
}  // namespace scn

template <std::size_t I, typename R, typename... T>
struct std::tuple_element<I, scn::scan_result_tuple<R, T...>> {
    using type = std::tuple_element_t<I, std::tuple<scn::scan_result<R>, T...>>;
};
template <typename R, typename... T>
struct std::tuple_size<scn::scan_result_tuple<R, T...>>
    : std::integral_constant<std::size_t, 1 + sizeof...(T)> {};

namespace scn {
    SCN_BEGIN_NAMESPACE

    template <std::size_t I, typename R, typename... T>
    decltype(auto) get(scan_result_tuple<R, T...>& r)
    {
        if constexpr (I == 0) {
            return r.result();
        }
        else {
            return std::get<I - 1>(r.values());
        }
    }
    template <std::size_t I, typename R, typename... T>
    decltype(auto) get(const scan_result_tuple<R, T...>& r)
    {
        if constexpr (I == 0) {
            return r.result();
        }
        else {
            return std::get<I - 1>(r.values());
        }
    }
    template <std::size_t I, typename R, typename... T>
    decltype(auto) get(scan_result_tuple<R, T...>&& r)
    {
        if constexpr (I == 0) {
            return r.result();
        }
        else {
            return std::get<I - 1>(r.values());
        }
    }

    SCN_END_NAMESPACE
}  // namespace scn
