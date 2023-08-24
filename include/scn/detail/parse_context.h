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

#include <scn/util/string_view.h>

namespace scn {
    SCN_BEGIN_NAMESPACE

    /**
     * Format string parsing context, wrapping the format string being parsed,
     * and a counter for argument indexing.
     */
    template <typename CharT>
    class basic_scan_parse_context {
    public:
        using char_type = CharT;
        using iterator = typename std::basic_string_view<CharT>::const_pointer;

        explicit constexpr basic_scan_parse_context(
            std::basic_string_view<CharT> format,
            int next_arg_id = 0)
            : m_format{format}, m_next_arg_id{next_arg_id}
        {
        }

        /// Returns an iterator pointing to the beginning of the format string
        constexpr auto begin() const SCN_NOEXCEPT
        {
            return m_format.data();
        }
        constexpr auto end() const SCN_NOEXCEPT
        {
            return m_format.data() + m_format.size();
        }

        /// Advance the beginning of the format string to `it`
        constexpr void advance_to(iterator it)
        {
            m_format.remove_prefix(static_cast<std::size_t>(it - begin()));
        }

        constexpr size_t next_arg_id()
        {
            if (SCN_UNLIKELY(m_next_arg_id < 0)) {
                on_error(
                    "Cannot switch from manual to automatic argument indexing");
                return 0;
            }

            auto id = static_cast<size_t>(m_next_arg_id++);
            do_check_arg_id(id);
            return id;
        }

        constexpr void check_arg_id(std::size_t id)
        {
            if (SCN_UNLIKELY(m_next_arg_id > 0)) {
                on_error(
                    "Cannot switch from manual to automatic argument indexing");
                return;
            }
            m_next_arg_id = -1;
            do_check_arg_id(id);
        }

        constexpr scan_error on_error(const char* msg) const
        {
            return detail::handle_error(
                scan_error{scan_error::invalid_format_string, msg});
        }

    protected:
        constexpr void do_check_arg_id(size_t id);

        std::basic_string_view<CharT> m_format;
        int m_next_arg_id{0};
    };

    namespace detail {
        /// Parse context with extra data used only for compile-time checks
        template <typename CharT>
        class compile_parse_context : public basic_scan_parse_context<CharT> {
            using base = basic_scan_parse_context<CharT>;

        public:
            explicit constexpr compile_parse_context(
                std::basic_string_view<CharT> format_str,
                int num_args,
                const arg_type* types,
                int next_arg_id = 0)
                : base(format_str, next_arg_id),
                  m_num_args(num_args),
                  m_types(types)
            {
            }

            SCN_NODISCARD constexpr int get_num_args() const
            {
                return m_num_args;
            }
            SCN_NODISCARD constexpr arg_type get_arg_type(std::size_t id) const
            {
                return m_types[id];
            }

            SCN_GCC_PUSH
            SCN_GCC_IGNORE("-Wsign-conversion")

            constexpr std::size_t next_arg_id()
            {
                auto id = base::next_arg_id();
                if (SCN_UNLIKELY(id >= static_cast<size_t>(m_num_args))) {
                    this->on_error("Argument not found");
                }
                return id;
            }

            constexpr void check_arg_id(std::size_t id)
            {
                base::check_arg_id(id);
                if (SCN_UNLIKELY(id >= static_cast<size_t>(m_num_args))) {
                    this->on_error("Argument not found");
                }
            }
            using base::check_arg_id;

        private:
            int m_num_args;
            const arg_type* m_types;

            SCN_GCC_POP  // -Wsign-conversion
        };

        constexpr inline bool is_constant_evaluated(bool default_value = false)
            SCN_NOEXCEPT
        {
#ifdef __cpp_lib_is_constant_evaluated
            SCN_UNUSED(default_value);
            return std::is_constant_evaluated();
#else
            return default_value;
#endif
        }
    }  // namespace detail

    template <typename CharT>
    constexpr void basic_scan_parse_context<CharT>::do_check_arg_id(size_t id)
    {
        if (detail::is_constant_evaluated() &&
            (!SCN_GCC || SCN_GCC >= SCN_COMPILER(12, 0, 0))) {
            // The cast below will cause an error on gcc pre-12
            using parse_context_type = detail::compile_parse_context<CharT>;
            if (static_cast<int>(id) >=
                static_cast<parse_context_type*>(this)->get_num_args()) {
                SCN_UNLIKELY_ATTR
                on_error("Argument not found");
            }
        }
    }

    SCN_END_NAMESPACE
}  // namespace scn
