/*
 * graph.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_GRAPH_H
#define OB_GRAPH_H

#include <stdint.h> //uint
#include <stddef.h> //size_t
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

/**
 *
 * STRUCT
 *
 * */

typedef struct graph_hash_s graph_hash_t, *graph_hash_t_ref ;
struct graph_hash_s
{
    ssize_t vertex ; // index length in data stralloc
    ssize_t id ; // index of the genalloc edge into graph_vertex_t struct for the associated edges
} ;
#define GRAPH_HASH_ZERO { -1, -1 }

typedef struct graph_vertex_s graph_vertex_t, *graph_vertex_t_ref ;
struct graph_vertex_s
{
    ssize_t vertex ; // index length in data stralloc
    genalloc edge ; // type unsigned int: index length of each deps in data stralloc
    size_t nedge ; // length of the genalloc: number of deps

} ;
#define GRAPH_VERTEX_ZERO { -1, GENALLOC_ZERO, 0 }

typedef struct graph_s graph_t, *graph_t_ref ;
struct graph_s
{
    /** general stralloc for data */
    stralloc data ;

    /** vertex */
    genalloc vertex ; //type graph_vertex_t

    /** matrix */
    genalloc hash ;//type graph_hash_t
    unsigned int mlen ; //length of the matrix
    int front ; // front of the queue
    int rear ; // rear of the queue
    unsigned int sort_count ; //length of the sort array
    unsigned int *sort ; // topological order of vertex array
    unsigned int *matrix ; // matrix itself
} ;
#define GRAPH_ZERO { STRALLOC_ZERO, GENALLOC_ZERO, GENALLOC_ZERO, 0, -1, -1, 0, 0, 0 }

/**
 *
 * FREED
 *
 * */

extern void graph_free_matrix(graph_t *g) ;
extern void graph_free_sort(graph_t *g) ;
extern void graph_free_all(graph_t *g) ;

/**
 *
 * DEBUG API
 *
 * */

extern void graph_show_matrix(graph_t *g) ;
extern void graph_show_cycle(graph_t *g, unsigned int *involved, unsigned int count) ;

/**
 *
 * COMMUN API
 *
 * */

/** initiate an array @array of length @len to zero
 * where @array is on the form array[len] */
extern void graph_array_init_single(unsigned int *array, unsigned int len) ;

/** initialise @array of length @len to zero
 * where @array is on the form array[len*len] */
extern void graph_array_init_double(unsigned int *array, unsigned int len) ;

/** Reverse an array @array of length @len */
extern void graph_array_reverse(unsigned int *array, unsigned int len) ;

/** Insert @value at @where in array @array of length @len */
extern void graph_array_insert(unsigned int value, unsigned int where, unsigned int *array, unsigned int len) ;

/* @len: length of the list, generally graph->mlen
 * @list: list of edge to deal with, list[0] = 0 > not, list[0] = 1 > yes
 * Remove any empty element (e.g list[2] = 0) and rebuild a consecutive
 * list with the number of the matrix element (e.g list[0] = 3 where list[3] was 1)
 * @Return the length of the cleaned @list */
extern int graph_array_rebuild_list(unsigned int *list, unsigned int len) ;

/**
 *
 * HASH API
 *
 * */

ssize_t graph_hash_vertex_get_id(graph_t *g, char const *vertex) ;
ssize_t graph_hash_vertex_get_genid(graph_t *g, char const *vertex) ;

/**
 *
 * VERTEX API
 *
 * */

extern int graph_vertex_add(graph_t *g, char const *vertex) ;
extern int graph_vertex_remove(graph_t *g, char const *vertex) ;
extern int graph_vertex_add_with_edge(graph_t *g, char const *vertex, char const *edge) ;
extern int graph_vertex_add_with_nedge(graph_t *g, char const *vertex, stralloc *edge) ;
extern int graph_vertex_get_edge_salist(stralloc *store, graph_t *g, char const *vertex) ;
extern int graph_vertex_add_with_requiredby(graph_t *g, char const *vertex, char const *requiredby) ;
extern int graph_vertex_add_with_nrequiredby(graph_t *g, char const *vertex, stralloc *requiredby) ;

/**
 *
 * EDGE API
 *
 * */

extern int graph_edge_add_g(graph_t *g, char const *vertex, char const *edge) ;
extern int graph_edge_remove_g(graph_t *g, char const *vertex, char const *edge) ;

/**
 *
 * MATRIX API
 *
 * */

extern int graph_matrix_build(graph_t *g) ;
extern int graph_matrix_analyze_cycle(graph_t *g) ;
extern int graph_matrix_sort(graph_t *g) ;
/* Reverse the graph.matrix array */
extern void graph_matrix_transpose(graph_t *g) ;
/* Reverse the graph.sort array */
extern void graph_matrix_sort_reverse(graph_t *g) ;
/* append @sa as a sastr string with the sorted graph
 * @Return 0 on fail
 * @Return 1 on success.*/
extern int graph_matrix_sort_tosa(stralloc *sa, graph_t *g) ;

/* append @list with the sorted graph */
extern void graph_matrix_sort_tolist(unsigned int *list, graph_t *gc, graph_t *g) ;

/* @list should be iniatilized with graph_array_single function
 * before the call of these functions.
 * graph_array_rebuild function should be called after
 * the use of these functions to cleanup empty entries and
 * get vertex id at @list. */
extern int graph_matrix_get_edge(unsigned int *list, graph_t *g, char const *vertex, uint8_t recursive) ;
extern int graph_matrix_get_requiredby(unsigned int *list, graph_t *g, char const *vertex, uint8_t recursive) ;

/** Search dependencies (or requiredby if @requiredby > 0) of @name
 * and build a list stored on @list
 * @Return 0 on fail
 * @Return 1 on success */
extern int graph_matrix_get_edge_g(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive) ;

/** Call graph_matrix_get_edge_g
 * and append the result to @sa
 * @Return -1 on fail
 * @Return the number of deps on success */
extern int graph_matrix_get_edge_g_sa(stralloc *sa, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive) ;

/** Call graph_matrix_get_edge_g
 * and append the result to @list
 * @Return -1 on fail
 * @Return the number of deps on success */
extern int graph_matrix_get_edge_g_list(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive) ;

/** Build a graph @gc with deps of @name from graph @g.
 * @Return 0 for no deps
 * @Return -1 on fail
 * @Return 1 on success*/
extern int graph_matrix_get_edge_g_sorted(graph_t *g, graph_t *gc, char const *name, uint8_t requiredby, uint8_t recursive) ;

/** append @sa with the name of deps of @name from graph @g
 * @Return 0 for no deps
 * @Return -1 on fail
 * @Return the number of the deps on success */
extern int graph_matrix_get_edge_g_sorted_sa(stralloc *sa, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive) ;

/** same as graph_matrix_get_edge_g_sorted_sa but instead store
 * the result at @list */
extern int graph_matrix_get_edge_g_sorted_list(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive) ;

#endif
