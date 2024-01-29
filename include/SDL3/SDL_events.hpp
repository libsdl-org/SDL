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
    struct event_queue_iterator {
        using value_type = SDL_Event;
        using pointer = SDL_EventQueueElement;
        using reference = value_type&;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;

        SDL_EventQueueElement iter;

        event_queue_iterator(pointer p):
        iter(p){}

        reference operator*() noexcept {
            return *(SDL_GetEvent(iter));
        }

        event_queue_iterator& operator++() noexcept {
            iter = SDL_ForwardElement(iter);
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
}