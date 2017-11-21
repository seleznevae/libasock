#include "third_party/Catch/catch.hpp"
#include "asock_vector.h"
#include "asock.h"




TEST_CASE( "Test asock_vector basic operations", "[asock_vector]" )
{
    typedef short item_t;
    const size_t init_capacity = 10;
    asock_vector *vector = asock_vector_create(sizeof(item_t), init_capacity);

    REQUIRE( vector != NULL );
    REQUIRE( asock_vector_size(vector)     == 0 );
    REQUIRE( asock_vector_capacity(vector) == init_capacity );

    WHEN("Pushing less items than initial capacity") {
        for (size_t i = 0; i < init_capacity; ++i) {
            item_t item = i;
            asock_vector_push(vector, &item);
        }

        THEN("Then capacity is not changed") {
            REQUIRE( asock_vector_size(vector)     == init_capacity );
            REQUIRE( asock_vector_capacity(vector) == init_capacity );
        }
    }

    WHEN("Pushing more items than initial capacity") {
        for (size_t i = 0; i < 2 * init_capacity; ++i) {
            item_t item = 2 * i;
            asock_vector_push(vector, &item);
        }

        THEN("Then capacity is increased") {
            REQUIRE( asock_vector_size(vector)     == 2 * init_capacity );
            REQUIRE( asock_vector_capacity(vector)  > init_capacity );
        }

        WHEN("Checking indexes of items") {
            item_t item =  6;
            REQUIRE( asock_vector_index_of(vector, &item) == 3 );
            item =  14;
            REQUIRE( asock_vector_index_of(vector, &item) == 7 );
            item =  25;
            REQUIRE( asock_vector_index_of(vector, &item) == ASOCK_INVALID_VEC_INDEX );
        }

        WHEN("Checking access to items") {
            REQUIRE( *(item_t *)asock_vector_at(vector,  0) == 0 );
            REQUIRE( *(item_t *)asock_vector_at(vector, 10) == 20 );

            REQUIRE( (item_t *)asock_vector_at(vector, 20) == NULL );

        }

        WHEN("Erasing items") {
            REQUIRE( asock_vector_erase(vector, 20) != ASOCK_SUCCESS );

            REQUIRE( asock_vector_erase(vector,  0) == ASOCK_SUCCESS );
            REQUIRE( asock_vector_erase(vector, 10) == ASOCK_SUCCESS );

            item_t first_item = *(item_t*)asock_vector_at(vector, 0);
            REQUIRE( first_item == 2 );
            item_t item =  6;
            REQUIRE( asock_vector_index_of(vector, &item) == 2 );
            item =  26;
            REQUIRE( asock_vector_index_of(vector, &item) == 11 );
        }
    }

    WHEN("Clearing vector") {
        asock_vector_clear(vector);

        THEN("Then size == 0") {
            REQUIRE( asock_vector_size(vector) == 0 );
        }
    }

    asock_vector_destroy( vector );
}
