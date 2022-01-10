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

#ifndef SCN_DETAIL_VSCAN_H
#define SCN_DETAIL_VSCAN_H

#include "file.h"
#include "visitor.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    // Avoid documentation issues: without this, Doxygen will think
    // SCN_BEGIN_NAMESPACE is a part of the vscan declaration
    namespace dummy {
    }

#if 0
    /**
     * In the spirit of {fmt}/`std::format` and `vformat`, `vscan` behaves
     * similarly to \ref scan, except instead of taking a variadic argument
     * pack, it takes an object of type `basic_args`, which type-erases the
     * arguments to scan. This, in effect, will decrease generated code size and
     * compile times dramatically.
     */
    template <typename Context, typename ParseCtx>
    error vscan(Context& ctx, ParseCtx& pctx)
    {
        return visit(ctx, pctx);
    }
#endif
    template <typename WrappedRange>
    struct vscan_result {
        error err;
        WrappedRange range;
    };

    namespace detail {
        template <typename CharT>
        constexpr int to_format(int i)
        {
            return i;
        }
        template <typename CharT, typename T>
        constexpr basic_string_view<CharT> to_format(T&& f)
        {
            return basic_string_view<CharT>(SCN_FWD(f));
        }

        template <template <typename> class ParseCtx,
                  typename WrappedRange,
                  typename Format,
                  typename CharT = typename WrappedRange::char_type>
        vscan_result<WrappedRange> vscan_boilerplate(WrappedRange&& r,
                                                     const Format& fmt,
                                                     basic_args<CharT> args)
        {
            using context_type = basic_context<WrappedRange>;
            using parse_context_type =
                ParseCtx<basic_default_locale_ref<CharT>>;

            auto ctx = context_type(SCN_MOVE(r));
            auto pctx = parse_context_type(fmt, ctx);
            auto err = visit(ctx, pctx, SCN_MOVE(args));
            return {err, SCN_MOVE(ctx.range())};
        }

        template <template <typename> class ParseCtx,
                  typename WrappedRange,
                  typename Format,
                  typename CharT = typename WrappedRange::char_type>
        vscan_result<WrappedRange> vscan_boilerplate_localized(
            WrappedRange&& r,
            basic_locale_ref<CharT>&& loc,
            const Format& fmt,
            basic_args<CharT> args)
        {
            using locale_type = basic_locale_ref<CharT>;
            using context_type = basic_context<WrappedRange, locale_type>;
            using parse_context_type = ParseCtx<locale_type>;

            auto ctx = context_type(SCN_MOVE(r), SCN_MOVE(loc));
            auto pctx = parse_context_type(fmt, ctx);
            auto err = visit(ctx, pctx, SCN_MOVE(args));
            return {err, SCN_MOVE(ctx.range())};
        }
    }  // namespace detail

    template <typename WrappedRange,
              typename CharT = typename WrappedRange::char_type>
    vscan_result<WrappedRange> vscan(WrappedRange range,
                                     basic_string_view<CharT> fmt,
                                     basic_args<CharT>&& args)
    {
        return detail::vscan_boilerplate<basic_parse_context>(
            SCN_MOVE(range), fmt, SCN_MOVE(args));
    }

    template <typename WrappedRange,
              typename CharT = typename WrappedRange::char_type>
    vscan_result<WrappedRange> vscan(WrappedRange range,
                                     int n_args,
                                     basic_args<CharT>&& args)
    {
        return detail::vscan_boilerplate<basic_empty_parse_context>(
            SCN_MOVE(range), n_args, SCN_MOVE(args));
    }

    template <typename WrappedRange,
              typename CharT = typename WrappedRange::char_type>
    vscan_result<WrappedRange> vscan_localized(WrappedRange range,
                                               basic_locale_ref<CharT>&& loc,
                                               basic_string_view<CharT> fmt,
                                               basic_args<CharT>&& args)
    {
        return detail::vscan_boilerplate_localized<basic_parse_context>(
            SCN_MOVE(range), SCN_MOVE(loc), fmt, SCN_MOVE(args));
    }

    template <typename WrappedRange,
              typename CharT = typename WrappedRange::char_type>
    vscan_result<WrappedRange> vscan_localized(WrappedRange range,
                                               basic_locale_ref<CharT>&& loc,
                                               int n_args,
                                               basic_args<CharT>&& args)
    {
        return detail::vscan_boilerplate_localized<basic_empty_parse_context>(
            SCN_MOVE(range), SCN_MOVE(loc), n_args, SCN_MOVE(args));
    }

#if !defined(SCN_HEADER_ONLY) || !SCN_HEADER_ONLY

#define SCN_VSCAN_DECLARE(Range, WrappedAlias, CharAlias)                     \
    namespace detail {                                                        \
        namespace vscan_macro {                                               \
            using WrappedAlias = typename detail::range_wrapper_for_t<Range>; \
            using CharAlias = typename WrappedAlias::char_type;               \
        }                                                                     \
    }                                                                         \
    vscan_result<detail::vscan_macro::WrappedAlias> vscan(                    \
        detail::vscan_macro::WrappedAlias&&,                                  \
        basic_string_view<detail::vscan_macro::CharAlias>,                    \
        basic_args<detail::vscan_macro::CharAlias>&&);                        \
                                                                              \
    vscan_result<detail::vscan_macro::WrappedAlias> vscan(                    \
        detail::vscan_macro::WrappedAlias&&, int,                             \
        basic_args<detail::vscan_macro::CharAlias>&&)

    SCN_VSCAN_DECLARE(string_view, string_view_wrapped, string_view_char);
    SCN_VSCAN_DECLARE(file&, file_ref_wrapped, file_ref_char);

#endif  // !SCN_HEADER_ONLY

    SCN_END_NAMESPACE
}  // namespace scn

#if defined(SCN_HEADER_ONLY) && SCN_HEADER_ONLY && !defined(SCN_VSCAN_CPP)
#include "vscan.cpp"
#endif

#endif  // SCN_DETAIL_VSCAN_H
