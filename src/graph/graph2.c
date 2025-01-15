/*
 * graph2.h
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/graph2.h>

bitset_t bitset_create_empty(uint32_t len)
{
    bitset_t bs ;
    uint32_t rsize = (len + (UINT_NBITS - 1)) / UINT_NBITS ;

    if (rsize > (GRAPH_MAX_VERTEX / UINT_NBITS)) {
        bs.size = 0 ;
        memset(bs.bits, 0, sizeof(bs.bits)) ;
        return bs ;
    }

    bs.size = rsize ;
    memset(bs.bits, 0, sizeof(bs.bits)) ;
    return bs ;
}

void bitset_set(bitset_t *bitset, uint32_t index)
{
    bitset->bits[index / (8 * sizeof(uint32_t))] |= (1U << (index % (8 * sizeof(uint32_t)))) ;
}

void bitset_clear(bitset_t *bitset, uint32_t index)
{
    bitset->bits[index / (8 * sizeof(uint32_t))] &= ~(1U << (index % (8 * sizeof(uint32_t)))) ;
}

int bitset_isvalid(const bitset_t *bitset, uint32_t index)
{
    return bitset->bits[index / (8 * sizeof(uint32_t))] & (1U << (index % (8 * sizeof(uint32_t)))) ;
}

int graph_queue_compare(const void *a, const void *b)
{
    const uint32_t ua = (*(uint32_t *)a) ;
    const uint32_t ub = (*(uint32_t *)b) ;

    if (ua < ub)
        return -1 ;
    else if (ua > ub)
        return 1 ;

    return 0 ;
}

int graph_init(graph_t *g, uint32_t glen)
{
    g->len = glen ;
    g->vertexes = NULL ;
    g->qindex = queue_init(glen, graph_queue_compare, graph_queue_compare) ;
    if (g->qindex == NULL)
        return 0 ;
    memset(g->sindex, 0, sizeof(vertex_t *) * glen) ;
    g->nvertexes = 0 ;

    return 1 ;
}

int graph_add_vertex(graph_t *g, const char *name)
{
    vertex_t *s = NULL ;
    size_t len = strlen(name) ;
    uint32_t *index, i ;

    if (len > GRAPH_MAX_VERTEX_NAME)
        return (errno = EINVAL, 0) ;

    if ((g->nvertexes + 1) > g->len)
        return (errno = ENOMEM, 0) ;

    HASH_FIND_STR(g->vertexes, name, s) ;
    if (s != NULL)
        return 1 ;

    index = (uint32_t *)queue_get_min(g->qindex) ;
    if (!index) {
        /** empty queue,
         * take it from the current count */
        i = g->nvertexes ;
        index = &i ;

    } else {

        if (!queue_remove(g->qindex, index))
            return 0 ;
    }

    s = (vertex_t *)malloc(sizeof(vertex_t)) ;
    if (s == NULL)
        return (errno = ENOMEM, 0) ;

    memcpy(s->name, name, len) ;
    s->name[len] = 0 ;
    s->depends = bitset_create_empty(g->len) ;
    if (!s->depends.size) {
        free(s) ;
        return (errno = ENOMEM, 0) ;
    }

    s->requiredby = bitset_create_empty(g->len) ;
    if (!s->requiredby.size) {
        free(s) ;
        return (errno = ENOMEM, 0) ;
    }

    s->ndepends = 0 ;
    s->nrequiredby = 0 ;
    s->index = *index ;
    g->sindex[s->index] = s ;

    HASH_ADD_STR(g->vertexes, name, s) ;
    g->nvertexes++ ;

    return 1 ;
}

int graph_remove_vertex(graph_t *g, const char *name, bool cldepends, bool clrequired)
{
    vertex_t *s ;

    HASH_FIND_STR(g->vertexes, name, s) ;
    if (!s)
        return 1 ;

    if (s->ndepends && cldepends) {
        vertex_t *list[s->ndepends] ;
        memset(list, 0, sizeof(vertex_t *) * s->ndepends) ;
        graph_get_edge(g, s, list, false) ;

        for (uint32_t idx = 0 ; idx < s->ndepends ; idx++) {
            bitset_clear(&list[idx]->requiredby, s->index) ;
            list[idx]->nrequiredby-- ;
        }
    }

    if (s->nrequiredby && clrequired) {
        vertex_t *list[s->nrequiredby] ;
        memset(list, 0, sizeof(vertex_t *) * s->nrequiredby) ;
        graph_get_edge(g, s, list, true) ;

        for (uint32_t idx = 0 ; idx < s->nrequiredby ; idx++) {
            bitset_clear(&list[idx]->depends, s->index) ;
            list[idx]->ndepends-- ;
        }
    }

    // Mark the index available
    if (!queue_insert(g->qindex, (void *)&s->index, sizeof(s->index)))
        return 0 ;

    g->sindex[s->index] = NULL ;

    HASH_DEL(g->vertexes, s) ;
    free(s) ;
    g->nvertexes-- ;

    return 1 ; // Success
}

int graph_add_edge(graph_t *g, const char *vertex, const char *edge)
{
    vertex_t *src, *dst ;

    if (!vertex || !edge)
        return (errno = EINVAL, 0) ;

    HASH_FIND_STR(g->vertexes, vertex, src) ;
    HASH_FIND_STR(g->vertexes, edge, dst) ;
    if (src == NULL || dst == NULL) {
        log_warn("One or both services not found: '%s', '%s'.\n", vertex, edge) ;
        return 0 ;
    }

    if (!bitset_isvalid(&src->depends, dst->index)) {
        bitset_set(&src->depends, dst->index) ;
        src->ndepends++ ;
    }

    if (!bitset_isvalid(&dst->requiredby, src->index)) {
        bitset_set(&dst->requiredby, src->index) ;
        dst->nrequiredby++ ;
    }

    return 1 ;
}

int graph_remove_edge(graph_t *g, const char *vertex, const char *edge, bool cledge)
{
    vertex_t *src, *dst ;

    if (!vertex || !edge)
        return (errno = EINVAL, 0) ;

    HASH_FIND_STR(g->vertexes, vertex, src) ;
    HASH_FIND_STR(g->vertexes, edge, dst) ;
    if (src == NULL || dst == NULL)
        log_warn_return(LOG_EXIT_ZERO, "find service: ", vertex, " or ", edge, " -- please make a bug report.") ;

    bitset_clear(&src->depends, dst->index) ;
    src->ndepends-- ;
    if (cledge) {
        bitset_clear(&dst->requiredby, src->index) ;
        dst->nrequiredby-- ;
    }

    return 1 ;
}

void graph_get_edge(graph_t *g, const vertex_t *vertex, vertex_t **list, bool requiredby)
{
    uint32_t found = 0 ;
    const bitset_t *type = !requiredby ? &vertex->depends : &vertex->requiredby ;
    uint32_t nedge = !requiredby ? vertex->ndepends : vertex->nrequiredby ;

    if (!nedge)
        return ;

    // Search for each element (uint32_t) of the bitset
    for (uint32_t i = 0 ; i < type->size ; i++) {
        uint32_t bits = type->bits[i] ;

        // Search for each bit in that element
        for (uint32_t j = 0 ; j < UINT_NBITS ; j++) {
            if (bits & (1U << j)) {  // if bit == 1
                uint32_t index = i * UINT_NBITS + j ;
                list[found++] = g->sindex[index] ;
                if (found == nedge)
                    return ;
            }
        }
    }
}

void graph_cleanup(graph_t *g)
{
    vertex_t *s, *tmp ;
    uint32_t i = 0 ;

    HASH_ITER(hh, g->vertexes, s, tmp) {
        HASH_DEL(g->vertexes, s) ;
        free(s) ;
    }

    for (; i < g->len ; i++)
        g->sindex[i] = NULL ;

    queue_free(g->qindex) ;
}

static void graph_dfs_edges(graph_t *g, vertex_t *vertex, uint32_t *visited, uint32_t *store, uint32_t *len, bool reverse)
{
    uint32_t nedge = reverse ? vertex->nrequiredby : vertex->ndepends;

    if (nedge > 0) {
        vertex_t *edges[nedge];
        graph_get_edge(g, vertex, edges, reverse);

        for (uint32_t i = 0; i < nedge; i++) {
            if (edges[i] && !visited[edges[i]->index]) {
                graph_dfs(g, edges[i], visited, store, len, reverse);
            }
        }
    }
}

void graph_dfs(graph_t *g, vertex_t *vertex, uint32_t *visited, uint32_t *store, uint32_t *len, bool requiredby)
{
    if (vertex == NULL || visited[vertex->index])
        return;

    graph_dfs_edges(g, vertex, visited, store, len, requiredby);

    // Add the current vertex to the store
    store[(*len)++] = vertex->index;
    visited[vertex->index] = 1;
}

int check_cycle_recursive(graph_t *g, vertex_t *src, vertex_t *target, uint32_t *visited)
{
    uint32_t pos = 0 ;
    if (src->index == target->index)
        return 0 ;

    visited[src->index] = 1 ;

    vertex_t *edge[src->ndepends] ;
    memset(edge, 0, sizeof(vertex_t *) * src->ndepends) ;
    graph_get_edge(g, src, edge, false) ;

    for (; pos < src->ndepends ; pos++) {
        if (!visited[pos]) {
            if (!check_cycle_recursive(g, edge[pos], target, visited))
                return 0 ;
        }
    }

    return 1 ;
}

int graph_check_cycle(graph_t *g)
{

    uint32_t visited[g->len] ;
    memset(visited, 0, sizeof(uint32_t) * g->len) ;

    for (uint32_t pos = 0 ; pos < g->nvertexes ; pos++) {
        if (g->sindex[pos] != NULL && g->sindex[pos]->ndepends) {
            vertex_t *edge[g->sindex[pos]->ndepends] ;
            memset(edge, 0, sizeof(vertex_t *) * g->sindex[pos]->ndepends) ;
            graph_get_edge(g, g->sindex[pos], edge, false) ;

            for (uint32_t idx = 0 ; idx < g->sindex[pos]->ndepends ; idx++) {
                if (!check_cycle_recursive(g, edge[idx], g->sindex[pos], visited)) {
                    log_warn("Adding the dependency '%s' -> '%s' would create a cycle.\n", g->sindex[pos]->name, edge[idx]->name) ;
                    return 0 ;
                }
            }

        }
    }

    return 1 ;
}

int graph_sort(uint32_t *store, uint32_t *store_pos, graph_t *g, vertex_t **list, uint32_t len, bool requiredby)
{
    bool reverse = requiredby ;
    uint32_t visited[len], pos = 0 ;
    memset(store, 0, sizeof(uint32_t) * g->len) ;
    memset(visited, 0, sizeof(uint32_t) * g->len) ;

    if (!graph_check_cycle(g))
        return 0 ;

    for (; pos < len ; pos++) {
        if (list[pos])
            graph_dfs(g, list[pos], visited, store, store_pos, reverse) ;
    }
    return 1 ;
}

/*
#include <stdio.h>
void graph_show(graph_t *g, const char *msg)
{
    vertex_t *s, *tmp ;
    log_1_warn("[DEBUG]: ", msg) ;
    HASH_ITER(hh, g->vertexes, s, tmp) {
        printf("%s -> ", s->name) ;
        printf(" %i -> ", s->index) ;
        if (s->ndepends) {

            vertex_t *list[s->ndepends] ;
            graph_get_edge(g, s, list, false) ;
            for (uint32_t pos = 0 ; pos < s->ndepends ; pos++) {
                printf("%s ", list[pos]->name) ;
                printf(" index:%i ", list[pos]->index) ;
            }

        }

        printf("\n%s <- ", s->name) ;
        if (s->nrequiredby) {

            vertex_t *list[s->nrequiredby] ;
            graph_get_edge(g, s, list, true) ;
            for (uint32_t pos = 0 ; pos < s->nrequiredby ; pos++) {
                printf("%s ", list[pos]->name) ;
                printf(" index:%i", list[pos]->index) ;
            }

        }
        printf("\n") ;
    }
}*/