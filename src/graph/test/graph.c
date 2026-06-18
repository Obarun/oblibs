#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <oblibs/graph.h>

void test_graph_init() {
    printf("Testing graph_init...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 10));
    assert(g.len == 10);
    assert(g.nvertexes == 0);
    assert(hash_count(&g.vertexes) == 0);
    assert(g.qindex != NULL);
    graph_free(&g);
}

void test_graph_add_vertex() {
    printf("Testing graph_add_vertex...\n");
    graph g ;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(g.nvertexes == 3);
    assert(g.sindex[0] != NULL && strcmp(g.sindex[0]->name, "VertexA") == 0);
    assert(g.sindex[1] != NULL && strcmp(g.sindex[1]->name, "VertexB") == 0);
    assert(g.sindex[2] != NULL && strcmp(g.sindex[2]->name, "VertexC") == 0);
    assert(graph_add_vertex(&g, "VertexD"));
    assert(graph_add_vertex(&g, "VertexE"));
    assert(!graph_add_vertex(&g, "VertexF") && errno == EINVAL);
    graph_free(&g);
}

void test_graph_add_edge() {
    printf("Testing graph_add_edge...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_edge(&g, "VertexA","VertexB",false));
    assert(graph_add_edge(&g, "VertexA","VertexC",false));
    vertex_t *vertexA = g.sindex[0];
    vertex_t *vertexB = g.sindex[1];
    vertex_t *vertexC = g.sindex[2];
    assert(bitset32_isvalid(&vertexA->depends, vertexB->index));
    assert(bitset32_isvalid(&vertexA->depends, vertexC->index));
    assert(vertexA->ndepends == 2);
    assert(bitset32_isvalid(&vertexB->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 1);
    assert(bitset32_isvalid(&vertexC->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 1);
    assert(!graph_add_edge(&g, NULL, "VertexC", false));
    assert(!graph_add_edge(&g, "VertexC", NULL, false));
    assert(!graph_add_edge(&g, NULL, NULL,false));
    graph_free(&g);
}

void test_graph_remove_edge() {
    printf("Testing graph_remove_edge...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_remove_edge(&g, "VertexA", "VertexB", true));
    vertex_t *vertexA = g.sindex[0];
    vertex_t *vertexB = g.sindex[1];
    assert(!bitset32_isvalid(&vertexA->depends, vertexB->index));
    assert(vertexA->ndepends == 0);
    assert(!bitset32_isvalid(&vertexB->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 0);
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_remove_edge(&g, "VertexA", "VertexB",false));
    vertexA = g.sindex[0];
    vertexB = g.sindex[1];
    assert(!bitset32_isvalid(&vertexA->depends, vertexB->index));
    assert(vertexA->ndepends == 0);
    assert(bitset32_isvalid(&vertexB->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 1);
    graph_free(&g);
}

void test_graph_remove_vertex() {
    printf("Testing graph_remove_vertex...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_remove_vertex(&g, "VertexB", true, true));
    vertex_t *vertexA = g.sindex[0];
    assert(vertexA != NULL);
    assert(vertexA->ndepends == 0);
    assert(g.nvertexes == 2);
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexD"));
    assert(graph_add_edge(&g, "VertexC", "VertexB",false));
    assert(graph_remove_vertex(&g, "VertexC", true, true));
    assert(graph_add_vertex(&g, "VertexY"));
    graph_free(&g);
}

void test_graph_get_edge() {
    printf("Testing graph_get_edge...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_add_edge(&g, "VertexA", "VertexC",false));
    vertex_t *vertexA = g.sindex[0];
    vertex_t *vertexB = g.sindex[1];
    vertex_t *vertexC = g.sindex[2];
    vertex_t *list[2];
    graph_get_edge(&g, vertexA, list, false);
    assert(list[0] != NULL && strcmp(list[0]->name, "VertexB") == 0);
    assert(list[1] != NULL && strcmp(list[1]->name, "VertexC") == 0);
    graph_get_edge(&g, vertexB, list, true);
    assert(list[0] != NULL && strcmp(list[0]->name, "VertexA") == 0);
    graph_get_edge(&g, vertexC, list, true);
    assert(list[0] != NULL && strcmp(list[0]->name, "VertexA") == 0);
    graph_free(&g);
}

void test_check_cycle_recursive() {
    printf("Testing check_cycle_recursive...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false)); // A depends on B
    assert(graph_add_edge(&g, "VertexB", "VertexC",false)); // B depends on C
    vertex_t *vertexA = g.sindex[0];
    vertex_t *vertexC = g.sindex[2];
    uint32_t visited[5];
    /* Reachability primitive: returns 0 when target is reachable from src
       (a directed path exists, or src == target), 1 otherwise. Each independent
       query needs a fresh visited. */
    memset(visited, 0, sizeof(visited));
    assert(!check_cycle_recursive(&g, vertexA, vertexC, visited)); // A->B->C reaches C
    memset(visited, 0, sizeof(visited));
    assert(check_cycle_recursive(&g, vertexC, vertexA, visited));  // C is a leaf, cannot reach A
    assert(graph_add_edge(&g, "VertexC", "VertexA",false));        // close the cycle A->B->C->A
    memset(visited, 0, sizeof(visited));
    assert(!check_cycle_recursive(&g, vertexC, vertexA, visited)); // now C reaches A
    graph_free(&g);
}

void test_graph_check_cycle_deep() {
    printf("Testing graph_check_cycle (deep)...\n");

    /* 3-node cycle hidden behind a leaf dependency: T->S, S->D(leaf), S->E, E->T.
       The leaf D is explored before E, which used to poison the shared visited
       state and make the E->T branch be skipped. */
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 8));
    assert(graph_add_vertex(&g, "T"));
    assert(graph_add_vertex(&g, "D"));
    assert(graph_add_vertex(&g, "S"));
    assert(graph_add_vertex(&g, "E"));
    assert(graph_add_edge(&g, "T", "S", false));
    assert(graph_add_edge(&g, "S", "D", false));
    assert(graph_add_edge(&g, "S", "E", false));
    assert(graph_add_edge(&g, "E", "T", false));
    assert(!graph_check_cycle(&g)); // cycle T->S->E->T must be detected
    graph_free(&g);

    /* Cycle reachable only after a non-cyclic branch was explored first:
       P->M, Q->M, M->Q (cycle Q<->M). Used to recurse infinitely (stack
       overflow) because visited was indexed by the loop counter, not the
       neighbour's vertex index. */
    graph h = GRAPH_ZERO;
    assert(graph_init(&h, 8));
    assert(graph_add_vertex(&h, "P"));
    assert(graph_add_vertex(&h, "Q"));
    assert(graph_add_vertex(&h, "M"));
    assert(graph_add_edge(&h, "P", "M", false));
    assert(graph_add_edge(&h, "Q", "M", false));
    assert(graph_add_edge(&h, "M", "Q", false));
    assert(!graph_check_cycle(&h)); // cycle Q<->M must be detected without recursing forever
    graph_free(&h);

    /* Diamond DAG must stay clean (no false positive). */
    graph d = GRAPH_ZERO;
    assert(graph_init(&d, 8));
    assert(graph_add_vertex(&d, "A"));
    assert(graph_add_vertex(&d, "B"));
    assert(graph_add_vertex(&d, "C"));
    assert(graph_add_vertex(&d, "Dd"));
    assert(graph_add_edge(&d, "A", "B", false));
    assert(graph_add_edge(&d, "A", "C", false));
    assert(graph_add_edge(&d, "B", "Dd", false));
    assert(graph_add_edge(&d, "C", "Dd", false));
    assert(graph_check_cycle(&d)); // no cycle
    graph_free(&d);
}

void test_graph_check_cycle() {
    printf("Testing graph_check_cycle...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_check_cycle(&g));
    assert(graph_add_edge(&g, "VertexB", "VertexA",false)); // Create cycle
    assert(!graph_check_cycle(&g));
    graph_free(&g);
}

void test_graph_dfs() {
    printf("Testing graph_dfs...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_vertex(&g, "VertexD"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_add_edge(&g, "VertexB", "VertexC",false));
    assert(graph_add_edge(&g, "VertexB", "VertexD",false));
    uint32_t visited[5] = {0};
    graph_dfs(&g, 0, visited, false);
    assert(g.sort[0] == 2); // VertexC
    assert(g.sort[1] == 3); // VertexD
    assert(g.sort[2] == 1); // VertexB
    assert(g.sort[3] == 0); // VertexA
    graph_free(&g);
}

void test_graph_sort() {
    printf("Testing graph_sort...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 5));
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));
    assert(graph_add_vertex(&g, "VertexD"));
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_add_edge(&g, "VertexB", "VertexC",false));
    assert(graph_sort(&g, false));
    assert(g.sort[0] == 2); // VertexC
    assert(g.sort[1] == 3); // VertexD
    assert(g.sort[2] == 1); // VertexB
    assert(g.sort[3] == 0); // VertexA
    assert(graph_add_edge(&g, "VertexC", "VertexA",false)); // Create cycle
    assert(!graph_sort(&g, false));
    graph_free(&g);
}

void test_graph_get_id() {
    printf("Testing graph_get_id...\n");
    graph g = GRAPH_ZERO;

    // Initialize the graph
    assert(graph_init(&g, 5));

    // Add vertices
    assert(graph_add_vertex(&g, "VertexA"));
    assert(graph_add_vertex(&g, "VertexB"));
    assert(graph_add_vertex(&g, "VertexC"));

    // Test retrieving IDs
    uint32_t idA = graph_get_id(&g, "VertexA");
    uint32_t idB = graph_get_id(&g, "VertexB");
    uint32_t idC = graph_get_id(&g, "VertexC");

    assert(idA == 0) ;
    assert(idB == 1) ;
    assert(idC == 2) ;

    assert(graph_remove_vertex(&g, "VertexB", true, true));
    assert(graph_add_vertex(&g, "VertexD"));
    assert(graph_add_vertex(&g, "VertexB"));

    idB = graph_get_id(&g, "VertexB");
    uint32_t idD = graph_get_id(&g, "VertexD");
    assert(idB == 3) ;
    assert(idD == 1) ;

    // Check IDs are unique and consistent
    assert(idA != idB && idA != idC && idB != idC);

    // Test non-existent vertex
    uint32_t invalid_id = graph_get_id(&g, "VertexE");
    assert(!invalid_id); // Expect UINT32_MAX for non-existent vertex

    // Cleanup
    graph_free(&g);
}

int main() {
    test_graph_init();
    test_graph_add_vertex();
    test_graph_add_edge();
    test_graph_remove_edge();
    test_graph_remove_vertex();
    test_graph_get_edge();
    test_check_cycle_recursive();
    test_graph_check_cycle();
    test_graph_check_cycle_deep();
    test_graph_dfs();
    test_graph_sort();
    test_graph_get_id();
    printf("All tests passed successfully.\n") ;
    return 0;
}
