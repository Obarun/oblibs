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
 *
 * Implementation and API for managing a dynamic graph structure with vertex relationships and edges.
 *
 * General Design:
 *  - Graph Representation:
 *    The graph is implemented using vertices (nodes) and directed edges, represented with bitsets and hash tables
 *    for efficient storage and operations. It supports dynamic vertex and edge management, including addition,
 *    removal, and traversal. Functions like 'graph_add_vertex' and 'graph_remove_vertex' ensure flexible handling
 *    of relationships between vertices.
 *
 *  - Memory Management:
 *    The graph dynamically allocates memory for its vertices and edges, resizing as necessary to accommodate
 *    changes. Functions like 'graph_free' ensure that all dynamically allocated resources are properly released.
 *    Bitsets are used for edge management to optimize memory usage while maintaining efficient traversal.
 *
 *  - Error Handling:
 *    Functions return standardized error codes ('EINVAL', 'ENOMEM', etc.) using 'errno' for diagnostics.
 *    This allows developers to handle invalid operations, memory allocation failures, and other edge cases robustly.
 *
 *  - Customization:
 *    The graph supports flexible configurations, including custom comparison functions for priority queues and
 *    edge relationship management. The integration with bitsets provides efficient ways to verify dependencies
 *    and relationships.
 *
 * Usage Scenarios:
 *  This graph structure is suitable for applications requiring dynamic management of nodes and their relationships:
 *  - Representing dependency trees in build systems or task schedulers.
 *  - Managing dynamic workflows or pipelines.
 *  - Implementing algorithms like topological sort, cycle detection, and pathfinding.
 *
 * Core Structure:
 *  The 'graph_t' structure represents the dynamic graph:
 *
 *  typedef struct graph_t {
 *      size_t len;           // Maximum number of vertices in the graph.
 *      vertex_t *vertexes;   // Hash table storing vertices by name.
 *      vertex_t *sindex[];   // Direct access to vertices by index.
 *      queue_t *qindex;      // Binary heap managing free indices.
 *      uint32_t nvertexes;   // Current number of vertices in the graph.
 *      uint32_t *sort;       // Sorting Array storing the topological order of sindex
 *      uint32_t nsort;       // Number of vertices stored in sort.
 *  } graph_t;
 *
 * Macros:
 *  - VERTEX_ZERO: Initializes a default vertex with no edges or dependencies.
 *  - GRAPH_ZERO: Initializes an empty graph structure.
 *
 * Key Functions:
 *  - 'graph_add_vertex': Adds a new vertex to the graph.
 *  - 'graph_remove_vertex': Removes a vertex and optionally clears its dependencies.
 *  - 'graph_add_edge': Creates a directed edge between two vertices.
 *  - 'graph_remove_edge': Removes a directed edge between two vertices.
 *  - 'graph_dfs': Performs depth-first search on the graph.
 *  - 'graph_check_cycle': Detects cycles in the graph.
 *
 * These macros and functions enable efficient manipulation of the graph and its relationships.
 */

#ifndef OB_GRAPH_H
#define OB_GRAPH_H

#include <stddef.h>
#include <stdio.h> // only for graph_show
#include <stdint.h>
#include <stdbool.h>

#include <oblibs/hash.h>
#include <oblibs/queue.h>
#include <oblibs/stack.h>
#include <oblibs/bits.h>

#ifndef GRAPH_MAX_VERTEX_NAME
#define GRAPH_MAX_VERTEX_NAME 256
#endif


/**
 * @struct vertex_t
 * @brief Represents a vertex with its edges and its relations to other vertexes.
 *
 * Each vertex contains a unique index, a name, and bitsets indicating its edges
 * on other vertexes and the vertexes that depend on it.
 *
 * @param name
 * The name of the vertex, stored in a fixed-size character array.
 *
 * @param index
 * A unique index assigned to the vertex within the graph.
 *
 * @param depends
 * A bitset representing the vertexes this vertex depends on.
 *
 * @param requiredby
 * A bitset representing the vertexes that depend on this vertex.
 *
 * @param ndepends
 * The number of vertexes this vertex depends on.
 *
 * @param nrequiredby
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
 * @struct graph
 * @brief Represents a graph containing vertexes and their edges.
 *
 * The graph manages a collection of vertexes, allowing efficient access to each
 * vertex by its index and tracking the available free indices using a heap.
 *
 * @param len
 * Maximum number of vertices in the graph.
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
 *
 * @param sort
 * An array storing the topological sorting order of the graph vertices.
 *
 * @param nsort
 * The number of vertices stored in the sort array.
 */
typedef struct graph_s graph ;
struct graph_s
{
    size_t len ;
    vertex_t *vertexes ;
    vertex_t *sindex[UINT32_BITS_MAX] ;
    queue_t *qindex ;
    uint32_t nvertexes ;
    uint32_t *sort ;
    uint32_t nsort ;
} ;

#define GRAPH_ZERO { 0, NULL, {NULL}, NULL, 0, NULL, 0 }

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
 * A pointer to the graph structure to be initialized.
 *
 * @param glen
 * The maximum number of vertexes that the graph can accommodate.
 *
 * @return
 * 1 if the initialization is successful, 0 otherwise.
 */
extern int graph_init(graph *g, uint32_t glen) ;

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
 * - `EINVAL` if the vertex name is too long or number or graph is full.
 * - `ENOMEM` if there is not enough memory to add the vertex or dependencies.
 *
 * @note
 * - If the queue of free indices is empty, the function assigns the next available index.
 * - Allocated memory for the vertex and its dependencies is automatically freed if an error occurs.
 * - The function maintains the integrity of the graph by updating all related data structures.
 */
extern int graph_add_vertex(graph *g, const char *name) ;

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
extern int graph_remove_vertex(graph *g, const char *name, bool cldepens, bool clrequired) ;

/**
 * @brief Adds a directed edge between two vertices in the graph.
 *
 * This function connects two vertices in the graph by creating a directed edge
 * from the source vertex to the destination vertex. The dependencies and reverse
 * dependencies of the vertices are updated accordingly. If the destination vertex
 * does not exist and the 'addedge' flag is set to true, it will be automatically added.
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
 * @param addedge
 * Boolean flag indicating whether to add the missing vertices automatically.
 *
 * @return 1
 * If the edge is successfully added.
 *
 * @return 0
 * If either vertex is not found (and 'addedge' is false) or if an error occurs ('errno' is set to 'EINVAL').
 *
 * @note
 * - The function does nothing if the edge already exists.
 * - If either vertex is not found, a warning is logged.
 * - If 'addedge' is set to true, missing destination vertices will be added automatically.
 */
extern int graph_add_edge(graph *g, const char *vertex, const char *edge, bool addedge) ;

/**
 * @brief Adds multiple edges to or from a vertex in the graph.
 *
 * This function iterates over the elements in a stack and adds directed edges
 * between the given vertex and each element in the stack. The direction of the edges
 * depends on the 'requiredby' parameter:
 * - If 'requiredby' is false, edges are added from the vertex to the elements in the stack.
 * - If 'requiredby' is true, edges are added from the elements in the stack to the vertex.
 *
 * @param g
 * Pointer to the graph where the edges will be added.
 *
 * @param vertex
 * Name of the vertex involved in the edges.
 *
 * @param stk
 * Pointer to the stack containing the names of the vertices to connect.
 *
 * @param requiredby
 * A boolean flag indicating the direction of the edges:
 * - 'false': Adds edges from 'vertex' to each element in 'stk'.
 * - 'true': Adds edges from each element in 'stk' to 'vertex'.
 *
 * @param addedge
 * Boolean flag indicating whether to add missing vertices when creating edges.
 *
 * @return 1
 * If all edges are successfully added.
 *
 * @return 0
 * If any edge addition fails, or if 'g', 'vertex', or 'stk' is invalid.
 *
 * @note
 * - The function depends on 'graph_add_edge' for individual edge additions.
 * - The stack 'stk' is assumed to be properly initialized and contain valid vertex names.
 * - A failure in adding any edge will halt further additions and return 0.
 */
extern int graph_add_nedge(graph *g, const char *vertex, stack *stk, bool requiredby, bool addedge) ;

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
extern int graph_remove_edge(graph *g, const char *vertex, const char *edge, bool cledge) ;

/**
 * @brief Retrieves edges of a vertex in the graph.
 *
 * This function retrieves all edges (dependencies or reverse dependencies) of
 * a given vertex and stores them in a provided list. Optimized for sparse graphs
 * using bitset traversal.
 *
 * @param g
 * Pointer to the graph.
 *
 * @param vertex
 * Pointer to the vertex whose edges will be retrieved.
 *
 * @param list
 * Array to store pointers to the connected vertices (must be pre-allocated).
 *
 * @param requiredby
 * If true, retrieves reverse dependencies; otherwise, retrieves dependencies.
 */
extern void graph_get_edge(graph *g, const vertex_t *vertex, vertex_t **list, bool requiredby) ;

/**
 * @brief Retrieves the unique ID (index) of a vertex in the graph by its name.
 *
 * This function searches for a vertex in the graph using its name and returns
 * its unique identifier (index). If the vertex is not found or the input is invalid,
 * the function returns 0 and sets `errno` appropriately.
 *
 * @param g
 * Pointer to the graph where the vertex resides.
 *
 * @param name
 * Name of the vertex to search for.
 *
 * @return
 * The unique ID (index) of the vertex if it is found.
 *
 * @return 0
 * If the vertex is not found or if the input graph or name is invalid.
 * In these cases, `errno` is set to:
 * - `EINVAL` if `g` or `name` is `NULL`.
 *
 * @note
 * - The returned ID is specific to the vertex and is used internally to manage
 *   graph operations.
 * - A return value of 0 may indicate an error or that the vertex with the given
 *   name does not exist.
 */
extern uint32_t graph_get_id(graph *g, const char *name) ;

/**
 * @brief Freed up the graph by freeing all allocated memory.
 *
 * This function removes all vertices from the graph, resets its state, and frees
 * all dynamically allocated memory.
 *
 * @param g
 * Pointer to the graph to clean up.
 */
extern void graph_free(graph *g) ;

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
 * Index of the starting vertex for the traversal.
 *
 * @param visited
 * Array to keep track of visited vertices.
 *
 * @param reverse
 * If true, traverses required-by dependencies; otherwise, traverses normal dependencies.
 *
 * @note
 * The function directly updates the graph's sort array with the traversal order.
 * If the vertex index is invalid or already visited, the function returns immediately.
 */
extern void graph_dfs(graph *g, uint32_t vertex, uint32_t *visited, bool reverse) ;

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
extern int check_cycle_recursive(graph *g, vertex_t *src, vertex_t *target, uint32_t *visited) ;

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
extern int graph_check_cycle(graph *g) ;

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
 * @param len
 * Pointer to the position counter in the store array.
 *
 * @param g
 * Pointer to the graph.
 *
 * @param reverse
 * If true, sorts based on required-by dependencies; otherwise, sorts based on dependencies.
 *
 * @return 1
 * If the sort is successful.
 *
 * @return 0
 * If a cycle is detected or an error occurs.
 *
 * @note
 * The function modifies the graph's internal sort array and resets sorting-related variables.
 */
extern int graph_sort(graph *g, bool reverse) ;

/**
 * @brief Displays the current vertexes and their edges.
 *
 * This debug function prints out each vertex in the graph along with its direct
 * edges and the vertexes that depend on it.
 *
 * @param g A pointer to the graph structure to be displayed.
 */
extern void graph_show(graph *g) ;

#endif