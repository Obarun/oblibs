/*
 * graph.c
 *
 * Copyright (c) 2021-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/graph.h>

#include <stdio.h> // only for graph_display()
#include <stdlib.h> //free, malloc
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

/**
 *
 * FREED HELPER
 *
 * */

void graph_free_matrix(graph_t *g)
{
    free(g->matrix) ;
}

void graph_free_sort(graph_t *g)
{
    free(g->sort) ;
}

static void graph_free_edge(graph_vertex_t *g)
{
    genalloc_free(unsigned int, &g->edge) ;
}

static void graph_free_vertex(graph_t *g)
{
    size_t pos = 0 ;
    for (; pos < genalloc_len(graph_vertex_t, &g->vertex) ; pos++)
        graph_free_edge(&genalloc_s(graph_vertex_t, &g->vertex)[pos]) ;

    genalloc_free(graph_vertex_t, &g->vertex) ;
}

static void graph_free_data(graph_t *g)
{
    stralloc_free(&g->data) ;
}

static void graph_free_hash(graph_t *g)
{
    genalloc_free(graph_hash_t, &g->hash) ;
}

/**
 *
 * INIT HELPER
 *
 * */

static void graph_init_length(graph_t *g, unsigned int len)
{
    g->mlen = len ;
}

static unsigned int *graph_alloc(unsigned int len)
{
    return (unsigned int *)malloc(len*sizeof(unsigned int)) ;
}

static int graph_alloc_sort(graph_t *g)
{
    if (!g->mlen)
        return 0 ;

    g->sort = graph_alloc(g->mlen) ;

    if(!g->sort)
        return 0 ;

    return 1 ;
}

static int graph_alloc_matrix(graph_t *g)
{
    if (!g->mlen)
        return 0 ;

    g->matrix = graph_alloc(g->mlen * g->mlen) ;

    if(!g->matrix)
        return 0 ;

    return 1 ;
}

static int graph_init_sort(graph_t *g)
{
    if (!g->mlen)
        return 0 ;

    graph_array_init_single(g->sort, g->mlen) ;

    return 1 ;
}

static int graph_init_matrix(graph_t *g)
{
    if (!g->mlen)
        return 0 ;

    graph_array_init_double(g->matrix, g->mlen) ;

    return 1 ;
}

static int graph_init(graph_t *g)
{
    graph_init_length(g, (unsigned int)genalloc_len(graph_hash_t,&g->hash)) ;

    if (!g->mlen)
        log_warn_return(LOG_EXIT_ZERO,"empty graph") ;

    if (!graph_alloc_sort(g))
        log_warnusys_return(LOG_EXIT_ZERO,"alloc sort array") ;

    if (!graph_alloc_matrix(g))
        log_warnusys_return(LOG_EXIT_ZERO,"alloc matrix") ;

    if (!graph_init_sort(g))
        return 0 ;

    if (!graph_init_matrix(g))
        return 0 ;

    return 1 ;
}

/**
 *
 * COMMON HELPER
 *
 *  */

static ssize_t graph_add_string(graph_t *g, char const *str)
{
    ssize_t len = sastr_cmp(&g->data, str) ;

    if (len < 0) {

        len = (ssize_t)g->data.len ;

        if (!sastr_add_string(&g->data, str))
            return -1 ;
    }

    return len ;
}

/**
 *
 * QUEUE HELPER
 *
 *  */

static int graph_queue_isempty(graph_t *g)
{
    if (g->front == -1 || g->front > g->rear)
        return 1 ;

    return 0 ;
}

static int graph_queue_addvertex(graph_t *g, unsigned int *queue, int vertex)
{
    if ((unsigned int)g->rear == g->mlen - 1) {

        return 0 ;

    } else {

        if (g->front == -1)
            g->front = 0 ;

        g->rear = g->rear + 1 ;
        queue[g->rear] = vertex ;
    }
    return 1 ;
}

static int graph_queue_delvertex(graph_t *g, unsigned int *queue)
{
    int vertex ;
    if (graph_queue_isempty(g)) {

        return -1 ;

    } else {

        vertex = queue[g->front] ;
        g->front = g->front + 1 ;
        return vertex ;
    }
    return 1 ;
}

/**
 *
 * HASH HELPER
 *
 * */

static int graph_hash_insert_vertex(graph_t *g, size_t vertex, size_t genid)
{
    ssize_t i ;

    graph_hash_t h = GRAPH_HASH_ZERO ;

    i = graph_hash_vertex_get_id(g, g->data.s + vertex) ;

    if (i == -1) {

        h.vertex = (ssize_t)vertex ;
        h.id = (ssize_t)genid ;

        if (!genalloc_append(graph_hash_t, &g->hash, &h))
            return 0 ;
    }

    return 1 ;
}

static int graph_hash_remove_vertex(graph_t *g, size_t vertex)
{
    ssize_t i ;

    size_t genlen = 0 ;

    i = graph_hash_vertex_get_id(g, g->data.s + vertex) ;

    if (i == -1)
        return 1 ;

    genlen = genalloc_len(graph_hash_t, &g->hash) ;

    genalloc_s(graph_hash_t, &g->hash)[i] = genalloc_s(graph_hash_t, &g->hash)[genlen - 1] ;

    genalloc_setlen(graph_hash_t, &g->hash, genlen - 1) ;

    genalloc_shrink(graph_hash_t, &g->hash) ;

    return 1 ;
}

/**
 *
 * VERTEX HELPER
 *
 * */

static int graph_vertex_insert_tohash(graph_t *g, graph_vertex_t *v, char const *vertex, size_t genid)
{
    v->vertex = graph_add_string(g, vertex) ;
    if (v->vertex < 0)
        return 0 ;
    /*
    v->vertex = g->data.len ;

    if (!sastr_add_string(&g->data, vertex))
        return 0 ;
    */
    if (!graph_hash_insert_vertex(g, v->vertex, genid))
        return 0 ;

    return 1 ;
}

static int graph_vertex_remove_fromhash(graph_t *g, graph_vertex_t *v)
{
    if (!graph_hash_remove_vertex(g, v->vertex))
        return 0 ;

    return 1 ;
}

/**
 *
 * EDGE HELPER
 *
 * */

static int graph_edge_add(graph_t *g, graph_vertex_t *v, char const *edge)
{
    ssize_t len = graph_add_string(g, edge) ;

    if (len < 0)
        return 0 ;

    unsigned int l = (unsigned int)len ;

    if (!genalloc_append(unsigned int, &v->edge, &l))
        return 0 ;

    //if (!sastr_add_string(&g->data, edge))
      //  return 0 ;

    v->nedge = genalloc_len(unsigned int, &v->edge) ;

    return 1 ;
}

static int graph_edge_remove(graph_t *g, graph_vertex_t *v, char const *edge)
{
    unsigned int pos = 0 ;

    for (; pos < v->nedge; pos++) {

        char *n = g->data.s + genalloc_s(unsigned int, &v->edge)[pos] ;

        if (!strcmp(n, edge)) {

            size_t genlen = genalloc_len(unsigned int, &v->edge) ;

            genalloc_s(unsigned int, &v->edge)[pos] = genalloc_s(unsigned int, &v->edge)[genlen - 1] ;

            genalloc_setlen(unsigned int, &v->edge, genlen - 1) ;

            genalloc_shrink(unsigned int, &v->edge) ;

            v->nedge-- ;

            break ;
        }
    }

    return 1 ;
}

/**
 *
 * MATRIX HELPER
 *
 * */

static int graph_matrix_search_rootvertex(graph_t *g, int vertex)
{
    unsigned int count = 0, res = 0 ;

    for(; count < g->mlen; count++)
        if(g->matrix[vertex * g->mlen + count] == 1)
            res++ ;

    return res ;
}

static void graph_matrix_copy(graph_t *g, unsigned int *copy)
{
    unsigned int v = 0, e = 0 ;
    for (v = 0 ; v < g->mlen ; v++)
        for (e = 0 ; e < g->mlen ; e++)
            copy[v * g->mlen + e] = g->matrix[v * g->mlen + e] ;

}

static int graph_matrix_edge_cycle(graph_t *g, unsigned int *involved, unsigned int *incount, unsigned int main, unsigned int edge, unsigned int *visit)
{
    unsigned int list[g->mlen + 2], pos = 0 ;

    int count ;

    char *ename = g->data.s + genalloc_s(graph_hash_t,&g->hash)[edge].vertex ;

    if (!visit[edge]) {
        visit[edge] = 1 ;
    } else return 1 ;

    graph_array_init_single(list, g->mlen) ;

    count = graph_matrix_get_edge(list, g, ename, 0) ;

    if (count == -1)
        return 0 ;

    count = graph_array_rebuild_list(list, g->mlen) ;

    for (; pos < count ; pos++) {

        if (g->matrix[edge * g->mlen + list[pos]] == 1) {

            involved[(*incount)++] = list[pos] ;

            if (list[pos] == main)
                return 0 ;

            if (!graph_matrix_edge_cycle(g, involved, incount, main, list[pos], visit))
                return 0 ;
        }
    }
    return 1 ;
}

/**
 *
 * FREED API
 *
 * */

void graph_free_all(graph_t *g)
{
    graph_free_matrix(g) ;
    graph_free_sort(g) ;
    graph_free_data(g) ;
    graph_free_vertex(g) ;
    graph_free_hash(g) ;

}

/**
 *
 * DEBUG API
 *
 * */

void graph_show_matrix(graph_t *g)
{
    unsigned int pos = 0, rev = 0 ;
    size_t len = 0 ;

    for (pos = 0 ; pos < g->mlen ; pos++) {
        char *vertex = g->data.s + genalloc_s(graph_hash_t,&g->hash)[pos].vertex ;
        size_t dlen = strlen(vertex) ;
        if (dlen > len)
            len = dlen ;
    }

    for (pos = 0 ; pos < g->mlen ; pos++) {
        char *vertex = g->data.s + genalloc_s(graph_hash_t,&g->hash)[pos].vertex ;
        printf("%s",vertex) ;
        printf("%*s",(int)((len - strlen(vertex)) + 1)," ") ;
        for (rev = 0 ; rev < g->mlen ; rev++) {
            printf(" %i ",g->matrix[pos * g->mlen + rev]) ;
        }
        printf("\n") ;
    }
    printf("\n") ;
}

void graph_show_cycle(graph_t *g, unsigned int *involved, unsigned int count)
{
    if (VERBOSITY < 2)
        return ;

    unsigned int verbo = VERBOSITY, pos = 0 ;
    set_trailing_newline(0) ;
    VERBOSITY = 2 ;
    log_warn("cycle encountered with: ") ;
    set_clock_enable(0) ;
    set_default_msg(0) ;

    for (; pos < count ; pos++)
        log_warn(g->data.s + genalloc_s(graph_hash_t,&g->hash)[involved[pos]].vertex, (pos + 1) >= count ? "\n" : " -> ") ;

    set_trailing_newline(1) ;
    set_default_msg(1) ;
    VERBOSITY = verbo ;
}

/**
 *
 * COMMUN API
 *
 * */

void graph_array_init_single(unsigned int *array, unsigned int len)
{
    unsigned int v = 0 ;
    for (v = 0 ; v < len ; v++)
        array[v] = 0 ;
}

void graph_array_init_double(unsigned int *array, unsigned int len)
{
    unsigned int v = 0, e = 0 ;
    for (v = 0 ; v < len ; v++)
        for (e = 0 ; e < len ; e++)
            array[v * len + e] = 0 ;
}

void graph_array_reverse(unsigned int *array, unsigned int len)
{
    unsigned int end = len - 1, pos = 0, t = 0 ;

    for (; pos < len/2 ; pos++, end--) {
        t = array[pos] ;
        array[pos] = array[end] ;
        array[end] = t ;
    }
}

void graph_array_insert(unsigned int value, unsigned int where, unsigned int *array, unsigned int array_len)
{
    ssize_t pos = array_len - 1 ;

    for ( ; pos >= where ; pos--) {
        if (pos == -1) {
            array[1] = array[0] ;
            break ;
        }
        array[pos + 1] = array[pos] ;
    }

    array[where] = value ;
}

int graph_array_rebuild_list(unsigned int *list, unsigned int len)
{
    unsigned int i = 0, count = 0, array[len] ;

    graph_array_init_single(array,len) ;

    for (; i < len ; i++)
        if (list[i] == 1)
            array[count++] = i ;

    graph_array_init_single(list,len) ;

    for (i = 0 ; i < count; i++)
        list[i] = array[i] ;

    return count ;
}

/**
 *
 * HASH API
 *
 * */

ssize_t graph_hash_vertex_get_id(graph_t *g, char const *vertex)
{
    size_t i = 0, len = genalloc_len(graph_hash_t, &g->hash) ;

    for (; i < len ; i++) {

        char *n = g->data.s + genalloc_s(graph_hash_t, &g->hash)[i].vertex ;

        if (!strcmp(vertex,n))
            return (ssize_t)i ;
    }

    return -1 ;
}

ssize_t graph_hash_vertex_get_genid(graph_t *g, char const *vertex)
{
    if (!vertex)
        return -1 ;

    ssize_t id = graph_hash_vertex_get_id(g, vertex) ;

    if (id == -1)
        return -1 ;

    return genalloc_s(graph_hash_t, &g->hash)[id].id ;
}

/**
 *
 * VERTEX API
 *
 * */

int graph_vertex_add(graph_t *g, char const *vertex)
{
    ssize_t found = -1 ;
    size_t genid = 0 ;
    /** avoid double entry */
    found = graph_hash_vertex_get_id(g, vertex) ;

    if (found == -1) {

        graph_vertex_t v = GRAPH_VERTEX_ZERO ;

        genid = genalloc_len(graph_vertex_t, &g->vertex) ;

        if (!graph_vertex_insert_tohash(g, &v, vertex, genid))
            return 0 ;

        if (!genalloc_append(graph_vertex_t, &g->vertex, &v))
            return 0 ;
    }

    return 1 ;
}

int graph_vertex_remove(graph_t *g, char const *vertex)
{
    ssize_t found = -1 ;
    size_t genid = 0, genlen = 0 ;

    found = graph_hash_vertex_get_id(g, vertex) ;

    if (found == -1)
        return 1 ;

    genid = graph_hash_vertex_get_genid(g, vertex) ;

    if (!graph_vertex_remove_fromhash(g, &genalloc_s(graph_vertex_t, &g->vertex)[genid]))
        return 0 ;

    genlen = genalloc_len(graph_vertex_t, &g->vertex) ;

    genalloc_s(graph_vertex_t, &g->vertex)[genid] = genalloc_s(graph_vertex_t, &g->vertex)[genlen - 1] ;

    genalloc_setlen(graph_vertex_t, &g->vertex, genlen - 1) ;

    genalloc_shrink(graph_vertex_t, &g->vertex) ;

    return 1 ;
}

int graph_vertex_add_with_edge(graph_t *g, char const *vertex, char const *edge)
{
    if (!graph_vertex_add(g, vertex))
        return 0 ;

    if(!graph_edge_add_g(g, vertex, edge))
        return 0 ;

    return 1 ;
}

/** optimizer function for mutliple edge entry in one row*/
int graph_vertex_add_with_nedge(graph_t *g, char const *vertex, stralloc *edge)
{
    if (!graph_vertex_add(g, vertex))
        return 0 ;

    /** vertex have edges? */
    if (edge->len) {

        ssize_t genid = graph_hash_vertex_get_genid(g, vertex) ;

        size_t pos = 0 ;

        FOREACH_SASTR(edge, pos) {

            if(!graph_edge_add(g, &(genalloc_s(graph_vertex_t,&g->vertex)[genid]), edge->s + pos))
                return 0 ;
        }
    }

    return 1 ;
}

int graph_vertex_get_edge_salist(stralloc *store, graph_t *g, char const *vertex)
{
    if (!vertex)
        return 0 ;

    ssize_t genid = graph_hash_vertex_get_genid(g, vertex) ;

    if (genid == -1)
        return 0 ;

    size_t nedge = genalloc_s(graph_vertex_t,&g->vertex)[genid].nedge ;
    size_t pos = 0 ;
    unsigned int id = 0 ;

    for (; pos < nedge ; pos++) {

        id = genalloc_s(unsigned int, &(genalloc_s(graph_vertex_t,&g->vertex)[genid].edge))[pos] ;

        if (!sastr_add_string(store, g->data.s + id))
            return 0 ;
    }

    return 1 ;
}

int graph_vertex_add_with_requiredby(graph_t *g, char const *vertex, char const *requiredby)
{
    /** insert vertex if it doesn't exist yet */
    if (!graph_vertex_add(g, vertex))
        return 0 ;

    if (!graph_vertex_add_with_edge(g, requiredby, vertex))
        return 0 ;

    return 1 ;
}

int graph_vertex_add_with_nrequiredby(graph_t *g, char const *vertex, stralloc *requiredby)
{
    size_t pos = 0 ;

    /** insert vertex if it doesn't exist yet */
    if (!graph_vertex_add(g, vertex))
        return 0 ;

    FOREACH_SASTR(requiredby, pos)
        if (!graph_vertex_add_with_edge(g, requiredby->s + pos, vertex))
            return 0 ;

    return 1 ;
}

/**
 *
 * EDGE API
 *
 * */

int graph_edge_add_g(graph_t *g, char const *vertex, char const *edge)
{
    ssize_t id = graph_hash_vertex_get_genid(g, vertex) ;

    if (id == -1)
        return 0 ;

    if(!graph_edge_add(g, &(genalloc_s(graph_vertex_t,&g->vertex)[id]), edge))
        return 0 ;

    return 1 ;
}

int graph_edge_remove_g(graph_t *g, char const *vertex, char const *edge)
{
    ssize_t id = graph_hash_vertex_get_genid(g, vertex) ;

    if (id == -1)
        return 0 ;

    if (!graph_edge_remove(g, &(genalloc_s(graph_vertex_t,&g->vertex)[id]), edge))
        return 0 ;

    return 1 ;
}

/**
 *
 * MATRIX API
 *
 * */

int graph_matrix_build(graph_t *g)
{
    if (!graph_init(g))
        log_warnu_return(LOG_EXIT_ONE,"init graph") ;

    unsigned int i = 0 ;

    for (i = 0 ; i < g->mlen ; i++) {

        ssize_t genid = genalloc_s(graph_hash_t,&g->hash)[i].id ;

        if (genid == -1)
            log_warnu_return(LOG_EXIT_ZERO,"get genid -- please make a bug report") ;

        size_t ndeps = genalloc_s(graph_vertex_t,&g->vertex)[genid].nedge ;

        if (ndeps) {

            size_t pos = 0 ;

            for (; pos < ndeps ; pos++) {

                ssize_t id = genalloc_s(unsigned int, &(genalloc_s(graph_vertex_t,&g->vertex)[genid].edge))[pos] ;

                if (id == -1)
                    log_warnu_return(LOG_EXIT_ZERO,"get id -- please make a bug report") ;

                char *dname = g->data.s + id ;
                ssize_t d = graph_hash_vertex_get_id(g, dname) ;

                if (d == -1)
                    log_warnu_return (LOG_EXIT_ZERO,"find: ", dname) ;

                g->matrix[i * g->mlen + (unsigned int)d] = 1 ;
            }
        }
    }

    return 1 ;
}

int graph_matrix_analyze_cycle(graph_t *g)
{
    unsigned int list[g->mlen], involved[g->mlen * g->mlen], visit[g->mlen], main = 0, idx = 0, pos = 0 ;
    int count ;

    graph_array_init_single(list, g->mlen) ;

    for (; main < g->mlen ; main++) {

        idx = pos = 0 ;

        graph_array_init_single(involved, g->mlen * g->mlen) ;
        graph_array_init_single(visit, g->mlen) ;

        char *smain = g->data.s + genalloc_s(graph_hash_t,&g->hash)[main].vertex ;

        count = graph_matrix_get_edge(list, g, smain, 0) ;

        if (count == -1)
            return 0 ;

        if (!count)
            continue ;

        involved[idx++] = main ;

        count = graph_array_rebuild_list(list, g->mlen) ;

        for (; pos < count ; pos++) {

            if (g->matrix[main * g->mlen + list[pos]] == 1) {

                involved[idx++] = list[pos] ;

                if (list[pos] == main)
                    graph_show_cycle(g,involved, idx) ;

                if (!graph_matrix_edge_cycle(g, involved, &idx, main, list[pos], visit)) {
                    graph_show_cycle(g,involved, idx) ;
                    return 0 ;
                }

                involved[--idx] = 0 ;
            }
        }
    }

    return 1 ;
}

int graph_matrix_sort(graph_t *g)
{
    unsigned int i = 0, count = 0, root[g->mlen], queue[g->mlen], m[g->mlen * g->mlen] ;
    int vertex = -1 ;

    graph_array_init_single(queue, g->mlen) ;

    /** initiate front and rear to correct value
     * if graph_matrix_sort is used on the same
     * whereas the graph was not fully freed */
    g->front = g->rear = -1 ;

    graph_matrix_copy(g, m) ;

    for (; i < g->mlen ; i++) {

        root[i] = graph_matrix_search_rootvertex(g, i) ;

        if(root[i] == 0)
            if (!graph_queue_addvertex(g, queue, i))
                log_warnu_return(LOG_EXIT_ZERO,"add vertex to queue") ;
    }

    while (!graph_queue_isempty(g) && count < g->mlen) {

        vertex = graph_queue_delvertex(g, queue) ;

        if (vertex == -1)
            log_warnu_return(LOG_EXIT_ZERO,"delete vertex from queue") ;

        g->sort[count++] = vertex ;

        for (i = 0 ; i < g->mlen ; i++) {

            if (m[i * g->mlen + vertex] == 1) {

                m[i * g->mlen + vertex] = 0 ;
                root[i] = root[i] - 1 ;
                if (root[i] == 0)
                    if (!graph_queue_addvertex(g, queue, i))
                        log_warnu_return(LOG_EXIT_ZERO,"add vertex to queue") ;
            }
        }
    }

    if (count < g->mlen)
        return 0 ;

    g->sort_count = count ;

    return 1 ;
}

void graph_matrix_transpose(graph_t *g)
{
    unsigned int pos = 0, deps = 0, t = 0 ;

    for (; pos < g->mlen ; pos++) {

        for (deps = 0 + pos; deps < g->mlen ; deps++) {
            t = g->matrix[deps * g->mlen + pos] ;
            g->matrix[deps * g->mlen + pos] = g->matrix[pos * g->mlen + deps] ;
            g->matrix[pos * g->mlen + deps] = t ;
        }
    }
}

void graph_matrix_sort_reverse(graph_t *g)
{
    graph_array_reverse(g->sort, g->mlen) ;
}

int graph_matrix_sort_tosa(stralloc *sa, graph_t *g)
{
    size_t pos = 0 ;

    for(; pos < g->sort_count ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[g->sort[pos]].vertex ;

        if (!sastr_add_string(sa, name)) {
            log_warnu("add string") ;
            return 0 ;
        }
    }

    return 1 ;
}

void graph_matrix_sort_tolist(unsigned int *list, graph_t *gc, graph_t *g)
{
    size_t pos = 0 ;

    graph_array_init_single(list, gc->sort_count) ;

    for(; pos < gc->sort_count ; pos++) {
        char *vertex = gc->data.s + genalloc_s(graph_hash_t, &gc->hash)[gc->sort[pos]].vertex ;
        list[pos] = graph_hash_vertex_get_id(g, vertex) ;
    }
}

int graph_matrix_get_edge(unsigned int *list, graph_t *g, char const *vertex, uint8_t recursive)
{
    if (!vertex)
        return -1 ;

    int id = graph_hash_vertex_get_id(g, vertex), c = 0 ;
    unsigned int pos = 0, count = 0 ;

    if (id == -1)
        return -1 ;

    for (; pos < g->mlen ; pos++) {

        c = 0 ;

        if (g->matrix[id * g->mlen + pos] == 1) {

            list[pos] = 1 ;

            count++ ;

            if (recursive) {

                c = graph_matrix_get_edge(list, g, g->data.s + genalloc_s(graph_hash_t, &g->hash)[pos].vertex, recursive) ;
                if (c == -1)
                    return -1 ;

                count += c ;
            }
        }
    }

    return count ;
}

int graph_matrix_get_requiredby(unsigned int *list, graph_t *g, char const *vertex, uint8_t recursive)
{
    graph_matrix_transpose(g) ;

    int count = graph_matrix_get_edge(list, g, vertex, recursive) ;

    graph_matrix_transpose(g) ;

    return count ;
}

int graph_matrix_get_edge_g(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive)
{
    graph_array_init_single(list, g->mlen) ;

    if (!requiredby) {

        if (graph_matrix_get_edge(list, g, name, recursive) < 0)
            return 0 ;

    } else {

        if (graph_matrix_get_requiredby(list, g, name, recursive) < 0)
            return 0 ;
    }

    return 1 ;
}

int graph_matrix_get_edge_g_sa(stralloc *sa, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive)
{
    size_t pos = 0 ;

    unsigned int list[g->mlen] ;
    int count = -1 ;

    if (!graph_matrix_get_edge_g(list, g, name, requiredby, recursive))
        return count ;

    count = graph_array_rebuild_list(list, g->mlen) ;

    if (count < 0)
        return count ;

    for(; pos < count ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        if (!sastr_add_string(sa, name))
            return -1 ;
    }

    return count ;
}

int graph_matrix_get_edge_g_list(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive)
{
    int count = -1 ;

    if (!graph_matrix_get_edge_g(list, g, name, requiredby, recursive))
        return count ;

    count = graph_array_rebuild_list(list, g->mlen) ;

    return count ;
}

int graph_matrix_get_edge_g_sorted(graph_t *g, graph_t *gc, char const *name, uint8_t requiredby, uint8_t recursive)
{
    size_t pos = 0 ;
    int count = -1 ;

    stralloc sa = STRALLOC_ZERO ;

    count = graph_matrix_get_edge_g_sa(&sa, g, name, requiredby, recursive) ;

    size_t len = sa.len ;
    char vertex[len + 1] ;

    if (count <= 0)
        goto freed ;

    sastr_to_char(vertex, &sa) ;

    for (; pos < len ; pos += strlen(vertex + pos) + 1) {

        sa.len = 0 ;

        char *ename = vertex + pos ;

        if (!graph_vertex_add(gc, ename)) {

            log_warnu("add vertex: ",ename) ;
            count = -1 ;
            goto freed ;
        }

        unsigned int c = graph_matrix_get_edge_g_sa(&sa, g, ename, requiredby, recursive) ;

        if (c < 0) {
            count = -1 ;
            goto freed ;
        }

        {
            size_t bpos = 0 ;

            FOREACH_SASTR(&sa, bpos) {

                char *edge = sa.s + bpos ;

                if (!graph_vertex_add_with_edge(gc, ename, edge)) {

                    log_warnu("add edge: ",ename," to vertex: ", edge) ;
                    count = -1 ;
                    goto freed ;
                }

                if (!graph_vertex_add(gc, edge)) {

                    log_warnu("add vertex: ",edge) ;
                    count = -1 ;
                    goto freed ;
                }
            }
        }
    }

    freed:
        stralloc_free(&sa) ;
        return count ;
}

int graph_matrix_get_edge_g_sorted_sa(stralloc *sa, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive)
{
    graph_t gc = GRAPH_ZERO ;

    int count = -1 ;

    count = graph_matrix_get_edge_g_sorted(g, &gc, name, requiredby, recursive) ;
    if (count <= 0)
        goto freed ;

    count = -1 ;

    if (!graph_matrix_build(&gc)) {

        log_warnu("build the matrix") ;
        goto freed ;
    }

    if (!graph_matrix_analyze_cycle(&gc)) {

        log_warnu("found cycle") ;
        goto freed ;
    }

    if (!graph_matrix_sort(&gc)) {

        log_warnu("sort the matrix") ;
        goto freed ;
    }

    sa->len = 0 ;

    if (!graph_matrix_sort_tosa(sa, &gc))
        goto freed ;

    count = gc.sort_count ;

    freed:
        graph_free_all(&gc) ;
        return count ;
}

int graph_matrix_get_edge_g_sorted_list(unsigned int *list, graph_t *g, char const *name, uint8_t requiredby, uint8_t recursive)
{
    graph_t gc = GRAPH_ZERO ;

    int count = -1 ;

    count = graph_matrix_get_edge_g_sorted(g, &gc, name, requiredby, recursive) ;
    if (count <= 0)
        goto freed ;

    count = -1 ;

    if (!graph_matrix_build(&gc)) {

        log_warnu("build the matrix") ;
        goto freed ;
    }

    if (!graph_matrix_analyze_cycle(&gc)) {

        log_warnu("found cycle") ;
        goto freed ;
    }

    if (!graph_matrix_sort(&gc)) {

        log_warnu("sort the matrix") ;
        goto freed ;
    }

    graph_matrix_sort_tolist(list, &gc, g) ;

    count = gc.sort_count ;

    freed:
        graph_free_all(&gc) ;
        return count ;
}
