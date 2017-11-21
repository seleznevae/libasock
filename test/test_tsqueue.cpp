#include "third_party/Catch/catch.hpp"
#include "asock_tsqueue.h"
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>


void print_short_item(void *item, size_t)
{
    printf("%d ", (int)*(short*)item);
}

void print(asock_tsqueue_t *queue)
{
    asock_tsqueue_for_each(queue, print_short_item);
    printf("\n");
}

TEST_CASE( "Test asock_tsqueue basic operations", "[asock_tsqueue]" )
{
    typedef short item_t;

    const size_t min_capacity = 10;
    asock_tsqueue_t *queue = asock_tsqueue_create(sizeof(item_t), min_capacity);

    REQUIRE( queue != NULL );
    REQUIRE( asock_tsqueue_size(queue)     == 0 );
    REQUIRE( asock_tsqueue_capacity(queue) >= min_capacity );

    size_t que_init_capacity = asock_tsqueue_capacity(queue);

    WHEN("Pushing less items than initial capacity") {
        for (size_t i = 0; i < que_init_capacity; ++i) {
            item_t item = i;
            asock_tsqueue_enqueue(queue, &item, 1);
        }

        THEN("Capacity is not changed") {
            REQUIRE( asock_tsqueue_size(queue)     == que_init_capacity);
            REQUIRE( asock_tsqueue_capacity(queue) == que_init_capacity);
        }

        bool right_dequeue = true;
        for (size_t i = 0; i < que_init_capacity; ++i) {
            item_t item = -1;
            asock_tsqueue_dequeue(queue, &item, 1);
            if (item != (short)i)
                right_dequeue = false;
        }
        REQUIRE( right_dequeue == true);
    }

    WHEN("Pushing more items than initial capacity") {
        for (size_t i = 0; i < 2 * que_init_capacity; ++i) {
            item_t item = i;
            asock_tsqueue_enqueue(queue, &item, 1);
        }

        THEN("Then capacity is increased") {
            REQUIRE( asock_tsqueue_size(queue)     == 2 * que_init_capacity );
            REQUIRE( asock_tsqueue_capacity(queue) > que_init_capacity );
        }
    }

    WHEN("Pushing and popping items") {
        for (size_t i = 0; i < 2 * que_init_capacity; ++i) {
            item_t item = i;
            asock_tsqueue_enqueue(queue, &item, 1);
        }

        THEN("Number of dequeue items == number of enqueue items") {
            size_t dequeue_sz = 0;
            item_t item = -1;
            bool right_dequeue = true;
            while (asock_tsqueue_dequeue(queue, &item, 1) == 1) {
                if (item != (item_t)dequeue_sz)
                    right_dequeue = false;
                dequeue_sz++;
            }
            REQUIRE( right_dequeue == true );
            REQUIRE( dequeue_sz    == 2 * que_init_capacity );
        }
    }

    WHEN("Pushing and popping random number of items") {
        std::vector<item_t> pushed_items;
        std::vector<item_t> popped_items;
        size_t popped_sz = 0;

        item_t items[500];
        for (size_t i = 0; i < 500; ++i) {
            items[i] = i;
        }

        for (size_t i = 0; i < 500; ++i) {
            size_t push_sz   = rand() % 100;
            size_t start_pos = rand() % 300;

            asock_tsqueue_enqueue(queue, &items[start_pos], push_sz);

            std::copy(&items[start_pos],
                      &items[start_pos + push_sz],
                       std::back_inserter(pushed_items));

            size_t pop_sz   = rand() % 100;
            pop_sz = asock_tsqueue_dequeue(queue,
                                            items,
                                            pop_sz);

            popped_sz += pop_sz;
            std::copy(&items[0],
                      &items[pop_sz],
                       std::back_inserter(popped_items));
        }

        while (asock_tsqueue_size(queue) != 0) {
            size_t pop_sz   = rand() % 100;
            pop_sz = asock_tsqueue_dequeue(queue,
                                               items,
                                               pop_sz);
            popped_sz += pop_sz;
            std::copy(&items[0],
                      &items[pop_sz],
                       std::back_inserter(popped_items));
        }

        REQUIRE( pushed_items == popped_items);
    }


    WHEN("Pushing random number of items and popping from head ") {
        std::vector<item_t> pushed_items;
        std::vector<item_t> popped_items;
        size_t popped_sz = 0;

        item_t items[500];
        for (size_t i = 0; i < 500; ++i) {
            items[i] = i;
        }

        for (size_t i = 0; i < 500; ++i) {
            size_t push_sz   = rand() % 100;
            size_t start_pos = rand() % 300;

            asock_tsqueue_enqueue(queue, &items[start_pos], push_sz);

            std::copy(&items[start_pos],
                      &items[start_pos + push_sz],
                       std::back_inserter(pushed_items));

            // Explicitly skipping some iterations
            if (i % 5 == 1) {
                size_t pop_sz = asock_tsqueue_head_size(queue);
                if (pop_sz > 500)
                    pop_sz = 500;

                asock_tsqueue_head_lock(queue);
                item_t *head_data = (item_t *)asock_tsqueue_head(queue);
                std::copy(head_data,
                          head_data + pop_sz,
                          std::back_inserter(popped_items));
                asock_tsqueue_head_unlock(queue);

                asock_tsqueue_dequeue(queue,items, pop_sz);
            }
        }

        while (asock_tsqueue_size(queue) != 0) {
            size_t pop_sz = asock_tsqueue_head_size(queue);
            if (pop_sz > 500)
                pop_sz = 500;

            asock_tsqueue_head_lock(queue);
            item_t *head_data = (item_t *)asock_tsqueue_head(queue);
            std::copy(head_data,
                      head_data + pop_sz,
                      std::back_inserter(popped_items));
            asock_tsqueue_head_unlock(queue);

            asock_tsqueue_dequeue(queue,items, pop_sz);
        }

        REQUIRE( pushed_items == popped_items);
    }


    asock_tsqueue_destroy( queue );
}
