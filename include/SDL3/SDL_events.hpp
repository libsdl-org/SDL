#pragma once

#include "SDL3/SDL_events.h"
#include <forward_list>
#include <type_traits>
#include <concepts>
#include <iterator>
#include <cassert>
#include <cstddef>
#include <vector>

namespace SDLcpp {
    /**
     * @brief SDLのイベントキューを扱うイテレータ
     * @tparam does_remove インクリメント時に現在の要素を破棄するか
     */
    template <bool does_remove>
    struct event_queue_iterator {
        using value_type = SDL_Event;
        using pointer = SDL_EventQueueElement;
        using reference = std::add_lvalue_reference_t<value_type>;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        SDL_EventQueueElement iter;

        event_queue_iterator(pointer p):
        iter(p){}

        reference operator*() const noexcept {
            return *(SDL_GetEvent(iter));
        }

        event_queue_iterator& operator++() noexcept {
            iter = SDL_ForwardElement(iter, does_remove);
            return *this;
        }

        event_queue_iterator operator++(int) noexcept {
            event_queue_iterator ret(iter);
            ++*this;
            return ret;
        }

        bool operator==(const event_queue_iterator& rval) noexcept {
            return iter == rval.iter;
        }
    };

    static_assert(std::input_iterator<event_queue_iterator<true>>);
    static_assert(std::input_iterator<event_queue_iterator<false>>);

}