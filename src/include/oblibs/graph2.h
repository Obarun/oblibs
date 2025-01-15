/*
 * graph.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/hash.h>
#include <oblibs/queue.h>

#ifndef GRAPH_MAX_VERTEX
#define GRAPH_MAX_VERTEX 500
#endif

#ifndef GRAPH_MAX_VERTEX_NAME
#define GRAPH_MAX_VERTEX_NAME 256
#endif

#define UINT_NBITS 32


/**
 * @struct bitset_t
 * @brief Represents a bitset used to indicate edges between vertexes.
 *
 * This bitset is used to mark the edges of one vertex on other vertexes,
 * or the vertexes that depend on it.
 *
 * @var bitset_t::bits
 * A bit array representing the edges, where each bit corresponds to a specific vertex.
 *
 * @var bitset_t::size
 * The size of the bitset, representing the number of vertexes tracked.
 */
typedef struct bitset_s bitset_t ;
struct bitset_s {
    uint32_t bits[GRAPH_MAX_VERTEX / (8 * sizeof(uint32_t))] ;
    uint32_t size ;
} ;

#define BITSET_ZERO { {0}, 0 }

/**
 * @struct vertex_t
 * @brief Represents a vertex with its edges and its relations to other vertexes.
 *
 * Each vertex contains a unique index, a name, and bitsets indicating its edges
 * on other vertexes and the vertexes that depend on it.
 *
 * @var vertex_t::name
 * The name of the vertex, stored in a fixed-size character array.
 *
 * @var vertex_t::index
 * A unique index assigned to the vertex within the graph.
 *
 * @var vertex_t::depends
 * A bitset representing the vertexes this vertex depends on.
 *
 * @var vertex_t::requiredby
 * A bitset representing the vertexes that depend on this vertex.
 *
 * @var vertex_t::ndepends
 * The number of vertexes this vertex depends on.
 *
 * @var vertex_t::nrequiredby
 * The number of vertexes that depend on this vertex.
 */
typedef struct vertex_s vertex_t ;
struct vertex_s {
    char name[GRAPH_MAX_VERTEX_NAME + 1] ;
    uint32_t index ;
    bitset_t depends ;
    bitset_t requiredby ;
    uint32_t ndepends ;
    uint32_t nrequiredby ;
    UT_hash_handle hh ;
} ;

#define VERTEX_ZERO { {0}, 0, BITSET_ZERO, BITSET_ZERO, 0, 0, NULL }

/**
 * @struct graph_t
 * @brief Represents a graph containing vertexes and their edges.
 *
 * The graph manages a collection of vertexes, allowing efficient access to each
 * vertex by its index and tracking the available free indices using a heap.
 *
 * @param len
 * Maximum number of vertex for the graph.
 *
 * @param vertexes
 * A hash table used to store vertexes by name.
 *
 * @param sindex
 * An array for direct access to vertexes by their index.
 *
 * @param qindex
 * A binary heap used to store and manage free indices in the graph.
 *
 * @param nvertexes
 * The actual number of vertexes in the graph.
 */
typedef struct graph_s graph_t ;
struct graph_s
{
    size_t len ;
    vertex_t *vertexes ;
    vertex_t *sindex[GRAPH_MAX_VERTEX] ;
    queue_t *qindex ;
    uint32_t nvertexes ;
} ;

#define GRAPH_ZERO { 0, NULL, {NULL}, NULL, 0 }

/**
 * @brief Utility function to create an empty bitset.
 *
 * Initializes a bitset to represent edges with all bits cleared (set to 0).
 *
 * @param len
 * The number of bits (vertexes) the bitset needs to represent.
 *
 * @return
 * A bitset_t structure with all bits cleared.
 */
extern bitset_t bitset_create_empty(uint32_t len) ;

/**
 * @brief Sets a bit in the bitset, corresponding to a vertex index.
 *
 * Marks a vertex as being dependent or required by setting the bit at the given index.
 *
 * @param bitset
 * A pointer to the bitset structure where the bit will be set.
 *
 * @param index
 * The index of the vertex to set in the bitset.
 */
extern void bitset_set(bitset_t *bitset, uint32_t index) ;

/**
 * @brief Clears a bit in the bitset, corresponding to a vertex index.
 *
 * Removes a dependency or "required by" relationship by clearing the bit at the given index.
 *
 * @param bitset
 * A pointer to the bitset structure where the bit will be cleared.
 *
 * @param index
 * The index of the vertex to clear in the bitset.
 */
extern void bitset_clear(bitset_t *bitset, uint32_t index) ;

/**
 * @brief Checks if a bit is set in the bitset.
 *
 * Determines whether a specific vertex (represented by the index) is marked in the bitset.
 *
 * @param bitset
 * A pointer to the bitset structure to check.
 *
 * @param index
 * The index of the vertex to check in the bitset.
 *
 * @return
 * 1 if the bit is set, 0 otherwise.
 */
extern int bitset_isvalid(const bitset_t *bitset, uint32_t index) ;

/**
 * @brief Compares two elements for ordering in a priority queue.
 *
 * This function compares two `uint32_t` values, which are assumed to be
 * pointers to the elements being compared. It is designed to be used as a
 * comparison function in priority queues or heaps.
 *
 * @param a
 * Pointer to the first element to compare.
 *
 * @param b
 * Pointer to the second element to compare.
 *
 * @return -1
 * If the first element is less than the second.
 *
 * @return 1
 * If the first element is greater than the second.
 *
 * @return 0
 * If the two elements are equal.
 *
 * @note
 * This function assumes that the input pointers `a` and `b` point to valid
 * `uint32_t` values. Passing invalid pointers or non-`uint32_t` data may
 * result in undefined behavior.
 */
extern int graph_queue_compare(const void *a, const void *b) ;

/**
 * @brief Initializes a graph structure.
 *
 * Sets up a graph with a specified length, initializes its vertexes, and creates a binary heap for managing free indices.
 *
 * @param g
 * A pointer to the graph_t structure to be initialized.
 *
 * @param glen
 * The maximum number of vertexes that the graph can accommodate.
 *
 * @return
 * 1 if the initialization is successful, 0 otherwise.
 */
extern int graph_init(graph_t *g, uint32_t glen) ;

/**
 * @brief Adds a vertex to the graph.
 *
 * This function creates a new vertex with the given name and adds it to the graph.
 * If the name is already in use, the function does not create a duplicate but instead
 * returns success. It allocates memory for the vertex and updates the graph's data structures
 * accordingly, including handling dependencies and managing free indices.
 *
 * @param g
 * Pointer to the graph where the vertex will be added.
 *
 * @param name
 * Name of the vertex to add. The name must not exceed `GRAPH_MAX_VERTEX_NAME` in length.
 *
 * @return 1
 * If the vertex was successfully added or if the vertex already exists in the graph.
 *
 * @return 0
 * If the vertex could not be added due to an error. Possible errors include:
 * - `EINVAL` if the vertex name is too long.
 * - `ENOMEM` if there is not enough memory to add the vertex or dependencies.
 *
 * @note
 * - If the queue of free indices is empty, the function assigns the next available index.
 * - Allocated memory for the vertex and its dependencies is automatically freed if an error occurs.
 * - The function maintains the integrity of the graph by updating all related data structures.
 */
extern int graph_add_vertex(graph_t *g, const char *name) ;

/**
 * @brief Removes a vertex from the graph with optional dependency clearing.
 *
 * This function removes a vertex identified by its name from the graph. It can optionally
 * clear the dependencies and reverse dependencies of the vertex being removed, based on the
 * values of `cldepends` and `clrequired`. The index of the removed vertex is marked as available
 * for future use.
 *
 * @param g
 * Pointer to the graph from which the vertex will be removed.
 *
 * @param name
 * Name of the vertex to remove.
 *
 * @param cldepends
 * If true, clears the vertex's dependencies from other vertices.
 *
 * @param clrequired
 * If true, clears the vertex from other vertices' reverse dependencies.
 *
 * @return 1
 * If the vertex is successfully removed or does not exist in the graph.
 *
 * @return 0
 * If the vertex could not be removed due to an error, such as failure to insert the freed
 * index back into the queue.
 *
 * @note
 * - If `cldepends` is true, the function clears dependencies of the vertex in other vertices.
 * - If `clrequired` is true, the function clears reverse dependencies of the vertex in other vertices.
 * - The function ensures the index of the removed vertex is marked available by inserting it
 *   back into the free index queue.
 * - Memory allocated for the vertex is freed, and the total vertex count of the graph is decremented.
 * - If both `cldepends` and `clrequired` are false, the vertex is removed without altering other vertices.
 */
extern int graph_remove_vertex(graph_t *g, const char *name, bool cldepens, bool clrequired) ;

/**
 * @brief Adds a directed edge between two vertices in the graph.
 *
 * This function connects two vertices in the graph by creating a directed edge
 * from the source vertex to the destination vertex. The dependencies and reverse
 * dependencies of the vertices are updated accordingly.
 *
 * @param g
 * Pointer to the graph where the edge will be added.
 *
 * @param vertex
 * Name of the source vertex.
 *
 * @param edge
 * Name of the destination vertex.
 *
 * @return 1
 * If the edge is successfully added.
 *
 * @return 0
 * If either vertex is not found or if an error occurs (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function does nothing if the edge already exists.
 * - If either vertex is not found, a warning is logged.
 */
extern int graph_add_edge(graph_t *g, const char *vertex, const char *edge) ;

/**
 * @brief Removes a directed edge between two vertices in the graph.
 *
 * This function removes a directed edge from the source vertex to the destination
 * vertex. It can optionally remove the reverse dependency in the destination vertex.
 *
 * @param g
 * Pointer to the graph where the edge will be removed.
 *
 * @param vertex
 * Name of the source vertex.
 *
 * @param edge
 * Name of the destination vertex.
 *
 * @param cledge
 * If true, removes the reverse dependency from the destination vertex.
 *
 * @return 1
 * If the edge is successfully removed.
 *
 * @return 0
 * If either vertex is not found or if an error occurs (`errno` is set to `EINVAL`).
 */
extern int graph_remove_edge(graph_t *g, const char *vertex, const char *edge, bool cledge) ;

/**
 * @brief Retrieves edges of a vertex in the graph.
 *
 * This function retrieves all edges (dependencies or reverse dependencies) of
 * a given vertex and stores them in a provided list.
 *
 * @param g
 * Pointer to the graph.
 *
 * @param vertex
 * Pointer to the vertex whose edges will be retrieved.
 *
 * @param list
 * Array to store pointers to the connected vertices.
 *
 * @param requiredby
 * If true, retrieves reverse dependencies; otherwise, retrieves dependencies.
 */
extern void graph_get_edge(graph_t *g, const vertex_t *vertex, vertex_t **list, bool requiredby) ;

/**
 * @brief Cleans up the graph by freeing all allocated memory.
 *
 * This function removes all vertices from the graph, resets its state, and frees
 * all dynamically allocated memory.
 *
 * @param g
 * Pointer to the graph to clean up.
 */
extern void graph_cleanup(graph_t *g) ;

/**
 * @brief Performs a depth-first search (DFS) starting from a given vertex.
 *
 * This function traverses the graph in depth-first order starting from the specified
 * vertex. The traversal can be reversed to follow reverse dependencies instead of
 * normal dependencies.
 *
 * @param g
 * Pointer to the graph.
 *
 * @param vertex
 * Pointer to the starting vertex for the traversal.
 *
 * @param visited
 * Array to keep track of visited vertices.
 *
 * @param store
 * Array to store the traversal order of vertices.
 *
 * @param len
 * Pointer to a counter tracking the number of visited vertices.
 *
 * @param requiredby
 * If true, traverses reverse dependencies; otherwise, traverses normal dependencies.
 */
extern void graph_dfs(graph_t *g, vertex_t *vertex, uint32_t *visited, uint32_t *store, uint32_t *len, bool requiredby) ;

/**
 * @brief Recursively checks for cycles starting from a source vertex in the graph.
 *
 * This function traverses the graph recursively from the source vertex to check
 * if there is a path that forms a cycle, i.e., returns to the target vertex.
 * It marks visited vertices to avoid revisiting them during the traversal.
 *
 * @param g
 * Pointer to the graph being analyzed.
 *
 * @param src
 * Pointer to the source vertex from which the traversal begins.
 *
 * @param target
 * Pointer to the target vertex to check for a cycle.
 *
 * @param visited
 * Array of flags representing whether a vertex has been visited during the traversal.
 *
 * @return 0
 * If a cycle is detected, i.e., a path from the source to the target is found.
 *
 * @return 1
 * If no cycle is detected.
 *
 * @note
 * - The function uses depth-first search (DFS) to traverse the graph recursively.
 * - If `src` and `target` are the same vertex, a cycle is immediately detected.
 * - The `visited` array must be pre-allocated with a size equal to the number of vertices
 *   in the graph and initialized to zero before calling the function.
 */
extern int check_cycle_recursive(graph_t *g, vertex_t *src, vertex_t *target, uint32_t *visited) ;

/**
 * @brief Checks the graph for cycles.
 *
 * This function verifies that the graph does not contain any cycles. If a cycle
 * is detected, a warning is logged, and the function returns failure.
 *
 * @param g
 * Pointer to the graph to check.
 *
 * @return 1
 * If the graph is acyclic.
 *
 * @return 0
 * If a cycle is detected.
 *
 * @note
 * - If a cycle is detected, the specific vertices forming the cycle are logged.
 * - This function relies on recursive traversal to check for cycles.
 */
extern int graph_check_cycle(graph_t *g) ;

/**
 * @brief Sorts vertices in the graph topologically.
 *
 * This function performs a topological sort on the graph vertices based on their
 * dependencies or reverse dependencies. It also ensures that the graph is acyclic
 * before proceeding with the sort.
 *
 * @param store
 * Array to store the sorted order of vertex indices.
 *
 * @param store_pos
 * Pointer to the position counter in the store array.
 *
 * @param g
 * Pointer to the graph.
 *
 * @param list
 * Array of vertices to sort.
 *
 * @param len
 * Number of vertices in the list.
 *
 * @param requiredby
 * If true, sorts based on reverse dependencies; otherwise, sorts based on dependencies.
 *
 * @return 1
 * If the sort is successful.
 *
 * @return 0
 * If a cycle is detected or an error occurs.
 */
extern int graph_sort(uint32_t *store, uint32_t *store_pos, graph_t *g, vertex_t **list, uint32_t len, bool requiredby) ;

/**
 * @brief Displays the current vertexes and their edges.
 *
 * This debug function prints out each vertex in the graph along with its direct
 * edges and the vertexes that depend on it.
 *
 * @param g A pointer to the graph_t structure to be displayed.
 * @param msg A message to display before displaying the graph.
extern void graph_show(graph_t *g, const char *msg) ;
*/