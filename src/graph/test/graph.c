#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <oblibs/graph2.h>

void test_graph_init() {
    printf("Testing graph_init...\n");
    graph g = GRAPH_ZERO;
    assert(graph_init(&g, 10));
    assert(g.len == 10);
    assert(g.nvertexes == 0);
    assert(g.vertexes == NULL);
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
    assert(bitset_isvalid(&vertexA->depends, vertexB->index));
    assert(bitset_isvalid(&vertexA->depends, vertexC->index));
    assert(vertexA->ndepends == 2);
    assert(bitset_isvalid(&vertexB->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 1);
    assert(bitset_isvalid(&vertexC->requiredby, vertexA->index));
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
    assert(!bitset_isvalid(&vertexA->depends, vertexB->index));
    assert(vertexA->ndepends == 0);
    assert(!bitset_isvalid(&vertexB->requiredby, vertexA->index));
    assert(vertexB->nrequiredby == 0);
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_remove_edge(&g, "VertexA", "VertexB",false));
    vertexA = g.sindex[0];
    vertexB = g.sindex[1];
    assert(!bitset_isvalid(&vertexA->depends, vertexB->index));
    assert(vertexA->ndepends == 0);
    assert(bitset_isvalid(&vertexB->requiredby, vertexA->index));
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
    assert(graph_add_edge(&g, "VertexA", "VertexB",false));
    assert(graph_add_edge(&g, "VertexB", "VertexC",false));
    uint32_t visited[5] = {0};
    vertex_t *vertexA = g.sindex[0];
    vertex_t *vertexC = g.sindex[2];
    assert(check_cycle_recursive(&g, vertexA, vertexC, visited));
    assert(graph_add_edge(&g, "VertexC", "VertexA",false)); // Create cycle
    memset(visited, 0, sizeof(visited));
    assert(!check_cycle_recursive(&g, vertexA, vertexA, visited));
    graph_free(&g);
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
    test_graph_dfs();
    test_graph_sort();
    test_graph_get_id();
    printf("All tests passed successfully.\n") ;
    return 0;
}
