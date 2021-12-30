#include <gtest/gtest.h>
#include <iostream>

#include "melon/static_graph.hpp"

#include "lemon/static_graph.h"
#include "lemon/smart_graph.h"

int main(int argc, char ** argv) {
    StaticDigraphBuilder builder;

    builder.add(1,2);
    builder.add(2,3);

    StaticDigraph graph = builder.build();    

    return 0;
}
