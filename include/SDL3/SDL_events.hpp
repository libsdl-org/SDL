#pragma once

#include "SDL3/SDL_events.h"
#include <forward_list>
#include <type_traits>
#include <algorithm>
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

    /// インクリメント時に移動元の要素を破棄する．
    using EQ_pop_iter = event_queue_iterator<true>;

    inline std::vector<SDL_Event> FetchAllEvents() noexcept {
        SDL_LockEventQueue();
        assert(SDL_IsEventQueueActive());
        // キューからイベントを移す．
        auto events = std::vector<SDL_Event>(SDL_NumOfEvent());
        std::move(EQ_pop_iter(SDL_EventQueueBegin()), EQ_pop_iter(SDL_EventQueueEnd()), events.begin());
        SDL_UnlockEventQueue();
        return events;
    }
}

///* Lock the event queue, take a peep at it, and unlock it */
// event = vector.get, numevents = vector.max, action = SDL_GETEVENT, [min, max]=all range, include_sentinel = SDL_FALSE
//static int SDL_PeepEventsInternal(SDL_Event *events, int numevents, SDL_eventaction action,
//                                  Uint32 minType, Uint32 maxType, SDL_bool include_sentinel = SDL_FALSE)
//{
//    int used = 0;
//
//    /* Lock the event queue */
//    SDL_LockMutex(SDL_EventQ.lock);
//    {
//        /* Don't look after we've quit */
//        if (!SDL_EventQ.active) {
//            /* We get a few spurious events at shutdown, so don't warn then */
//            SDL_SetError("The event system has been shut down");
//            SDL_UnlockMutex(SDL_EventQ.lock);
//            return -1;
//        }
//        SDL_EventEntry *entry, *next;
//        Uint32 type;
//
//        for (entry = SDL_EventQ.head; entry; entry = next) {
//            next = entry->next;
//            type = entry->event.type;
//            SDL_copyp(&events[used], &entry->event);
//            SDL_CutEvent(entry);
//            if (type == SDL_EVENT_POLL_SENTINEL) {
//                continue;
//            }
//            ++used;
//        }
//    }
//    SDL_UnlockMutex(SDL_EventQ.lock);
//
//    return used;
//}
/*


*/