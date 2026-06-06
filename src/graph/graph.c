/*
 * graph.c
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
#include <oblibs/graph.h>
#include <oblibs/bits.h>
#include <oblibs/sbl.h>

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

int graph_init(graph *g, uint32_t glen)
{
    g->len = glen ;
    g->vertexes = NULL ;
    memset(g->sindex, 0, sizeof(vertex_t *) * glen) ;
    g->qindex = queue_init(glen, graph_queue_compare, graph_queue_compare) ;
    if (g->qindex == NULL)
        return 0 ;

    g->sort = malloc(sizeof(uint32_t) * glen) ;
    if (!g->sort) {
        queue_free(g->qindex) ;
        return (errno = ENOMEM, 0) ;
    }
    g->nsort = 0 ;
    g->nvertexes = 0 ;

    return 1 ;
}

int graph_add_vertex(graph *g, const char *name)
{
    vertex_t *s = NULL ;
    size_t len = strlen(name) ;
    uint32_t index = 0, *iptr = NULL ;

    if (len > GRAPH_MAX_VERTEX_NAME)
        return (errno = EINVAL, 0) ;

    if ((g->nvertexes + 1) > g->len)
        return (errno = EINVAL, 0) ;

    HASH_FIND_STR(g->vertexes, name, s) ;
    if (s != NULL)
        return 1 ;

    iptr = (uint32_t *)queue_get_min(g->qindex) ;
    if (!iptr) {
        /** empty queue,
         * take it from the current count */
        index = g->nvertexes ;

    } else {

        index = *iptr ;
        if (!queue_remove(g->qindex, iptr))
            return 0 ;
    }

    s = (vertex_t *)malloc(sizeof(vertex_t)) ;
    if (s == NULL)
        return (errno = ENOMEM, 0) ;

    memcpy(s->name, name, len) ;
    s->name[len] = 0 ;
    s->depends = bitset32_init(g->len) ;
    if (!s->depends.size) {
        free(s) ;
        return (errno = ENOMEM, 0) ;
    }

    s->requiredby = bitset32_init(g->len) ;
    if (!s->requiredby.size) {
        free(s) ;
        return (errno = ENOMEM, 0) ;
    }

    s->ndepends = 0 ;
    s->nrequiredby = 0 ;
    s->index = index ;
    g->sindex[s->index] = s ;

    HASH_ADD_STR(g->vertexes, name, s) ;
    g->nvertexes++ ;

    return 1 ;
}

int graph_remove_vertex(graph *g, const char *name, bool cldepends, bool clrequired)
{
    vertex_t *s = NULL ;

    HASH_FIND_STR(g->vertexes, name, s) ;
    if (!s)
        return 1 ;

    if (s->ndepends && cldepends) {
        vertex_t *list[s->ndepends] ;
        memset(list, 0, sizeof(vertex_t *) * s->ndepends) ;
        graph_get_edge(g, s, list, false) ;

        for (uint32_t idx = 0 ; idx < s->ndepends ; idx++) {
            bitset32_clear(&list[idx]->requiredby, s->index) ;
            list[idx]->nrequiredby-- ;
        }
    }

    if (s->nrequiredby && clrequired) {
        vertex_t *list[s->nrequiredby] ;
        memset(list, 0, sizeof(vertex_t *) * s->nrequiredby) ;
        graph_get_edge(g, s, list, true) ;

        for (uint32_t idx = 0 ; idx < s->nrequiredby ; idx++) {
            bitset32_clear(&list[idx]->depends, s->index) ;
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

int graph_add_edge(graph *g, const char *vertex, const char *edge, bool addedge)
{
    vertex_t *src = NULL, *dst = NULL ;

    if (!vertex || !edge)
        return (errno = EINVAL, 0) ;

    HASH_FIND_STR(g->vertexes, vertex, src) ;
    HASH_FIND_STR(g->vertexes, edge, dst) ;
    if (!addedge && ((src == NULL) || (dst == NULL))) {
        log_warn("vertex: ", src == NULL ? vertex : edge," not found") ;
        return 0 ;
    }

    if (src == NULL) {
        if (!graph_add_vertex(g, vertex))
            log_warnusys("add edge: ", vertex, " to the graph selection") ;
        HASH_FIND_STR(g->vertexes, vertex, src) ;
    }

    if (dst == NULL) {
        if (!graph_add_vertex(g, edge))
            log_warnusys("add edge: ", edge, " to the graph selection") ;
        HASH_FIND_STR(g->vertexes, edge, dst) ;
    }

    if (!bitset32_isvalid(&src->depends, dst->index)) {
        bitset32_set(&src->depends, dst->index) ;
        src->ndepends++ ;
    }

    if (!bitset32_isvalid(&dst->requiredby, src->index)) {
        bitset32_set(&dst->requiredby, src->index) ;
        dst->nrequiredby++ ;
    }

    return 1 ;
}

int graph_add_nedge(graph *g, const char *vertex, strbuf *sb, bool requiredby, bool addedge)
{
    size_t pos = 0 ;

    if (sb->len) {

        FOREACH_SBL(sb, pos) {
            if (requiredby == false) {
                if (!graph_add_edge(g, vertex, sb->s + pos, addedge))
                    return 0 ;
            } else {
                if (!graph_add_edge(g, sb->s + pos, vertex, addedge))
                    return 0 ;
            }
        }
    }

    return 1 ;
}

int graph_remove_edge(graph *g, const char *vertex, const char *edge, bool cledge)
{
    vertex_t *src= NULL, *dst = NULL ;

    if (!vertex || !edge)
        return (errno = EINVAL, 0) ;

    HASH_FIND_STR(g->vertexes, vertex, src) ;
    HASH_FIND_STR(g->vertexes, edge, dst) ;
    if (src == NULL || dst == NULL)
        log_warn_return(LOG_EXIT_ZERO, "find service: ", vertex, " or ", edge, " -- please make a bug report.") ;

    bitset32_clear(&src->depends, dst->index) ;
    src->ndepends-- ;
    if (cledge) {
        bitset32_clear(&dst->requiredby, src->index) ;
        dst->nrequiredby-- ;
    }

    return 1 ;
}

void graph_get_edge(graph *g, const vertex_t *vertex, vertex_t **list, bool requiredby)
{
    uint32_t found = 0 ;
    const bitset32_t *type = !requiredby ? &vertex->depends : &vertex->requiredby ;
    uint32_t nedge = !requiredby ? vertex->ndepends : vertex->nrequiredby ;

    if (!nedge || type->size > BITSET32_ARRAY_SIZE)
        return ;

    // Search for each element (uint32_t) of the bitset
    for (uint32_t i = 0 ; i < type->size && found < nedge ; i++) {
        uint32_t bits = type->bits[i] ;
        if (!bits)
            continue ;

        // Search for each bit in that element
        for (uint32_t j = 0 ; j < UINT32_BITS && found < nedge ; j++) {
            if (bits & 1U) {
                uint32_t index = i * UINT32_BITS + j ;
                if (index >= UINT32_BITS_MAX)
                    return ;
                list[found++] = g->sindex[index] ;
            }

            bits >>= 1 ;
        }
    }
}

uint32_t graph_get_id(graph *g, const char *name)
{
    if (!g || !name)
        return (errno = EINVAL, 0) ;

    vertex_t *vertex = NULL ;
    HASH_FIND_STR(g->vertexes, name, vertex) ;

    if (vertex == NULL)
        return 0 ;

    return vertex->index;
}

void graph_free(graph *g)
{
    uint32_t i = 0 ;

    HASH_CLEAR(hh, g->vertexes) ;

    for (; i < g->len ; i++) {
       if (g->sindex[i]) {
            bitset32_free(&g->sindex[i]->depends);
            bitset32_free(&g->sindex[i]->requiredby);
            free(g->sindex[i]);
            g->sindex[i] = NULL;
        }
    }

    free(g->sort) ;
    g->sort = NULL ;
    queue_free(g->qindex) ;
}

int check_cycle_recursive(graph *g, vertex_t *src, vertex_t *target, uint32_t *visited)
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

int graph_check_cycle(graph *g)
{

    uint32_t visited[g->len], pos = 0 ;
    memset(visited, 0, sizeof(uint32_t) * g->len) ;

    for (; pos < g->nvertexes ; pos++) {
        if (g->sindex[pos] != NULL && g->sindex[pos]->ndepends) {
            vertex_t *edge[g->sindex[pos]->ndepends] ;
            memset(edge, 0, sizeof(vertex_t *) * g->sindex[pos]->ndepends) ;
            graph_get_edge(g, g->sindex[pos], edge, false) ;

            for (uint32_t idx = 0 ; idx < g->sindex[pos]->ndepends ; idx++) {
                if (!check_cycle_recursive(g, edge[idx], g->sindex[pos], visited)) {
                    log_warn("Adding the dependency: ", g->sindex[pos]->name, " depending on service: ", edge[idx]->name, " would create a cycle.\n") ;
                    return 0 ;
                }
            }
        }
    }

    return 1 ;
}

static void graph_dfs_edges(graph *g, uint32_t idx, uint32_t *visited, bool reverse)
{
    vertex_t *vertex = g->sindex[idx] ;
    uint32_t nedge = reverse ? vertex->nrequiredby : vertex->ndepends ;

    if (nedge > 0) {
        vertex_t *edges[nedge] ;
        graph_get_edge(g, vertex, edges, reverse) ;

        for (uint32_t i = 0; i < nedge; i++) {
            if (edges[i] && !visited[edges[i]->index]) {
                graph_dfs(g, edges[i]->index, visited, reverse) ;
            }
        }
    }
}

void graph_dfs(graph *g, uint32_t idx, uint32_t *visited, bool reverse)
{
    vertex_t *vertex = g->sindex[idx] ;
    if (vertex == NULL || visited[vertex->index])
        return ;

    graph_dfs_edges(g, idx, visited, reverse) ;

    // Add the current vertex to the store
    g->sort[g->nsort++] = vertex->index ;
    visited[vertex->index] = 1 ;
}

int graph_sort(graph *g, bool reverse)
{
    vertex_t *c, *t ;
    uint32_t visited[g->len] ;
    memset(g->sort, 0, sizeof(uint32_t) * g->len) ;
    memset(visited, 0, sizeof(uint32_t) * g->len) ;
    g->nsort = 0 ;

    if (!graph_check_cycle(g))
        return 0 ;

    // Process root vertices first
    HASH_ITER(hh, g->vertexes, c, t) {

        if (c == NULL)
            continue ;

        uint32_t rev = reverse ? c->nrequiredby : c->ndepends ;
        if (!rev)
            graph_dfs(g, c->index, visited, reverse);
    }

    HASH_ITER(hh, g->vertexes, c, t) {

        if (c && !visited[c->index])
            graph_dfs(g, c->index, visited, reverse) ;

    }

    return 1 ;
}

void graph_show(graph *g)
{
    vertex_t *s = NULL, *tmp = NULL ;
    log_1_warn("[DEBUG]: ") ;
    HASH_ITER(hh, g->vertexes, s, tmp) {
        printf("%s:%i -> ", s->name, s->index) ;
        if (s->ndepends) {
            vertex_t *list[s->ndepends] ;
            graph_get_edge(g, s, list, false) ;
            for (uint32_t pos = 0 ; pos < s->ndepends ; pos++)
                printf("%s:%i ", list[pos]->name, list[pos]->index) ;
        }

        printf("\n%s:%i <- ", s->name, s->index) ;
        if (s->nrequiredby) {
            vertex_t *list[s->nrequiredby] ;
            graph_get_edge(g, s, list, true) ;
            for (uint32_t pos = 0 ; pos < s->nrequiredby ; pos++)
                printf("%s:%i ", list[pos]->name, list[pos]->index) ;
        }
        printf("\n") ;
    }
}