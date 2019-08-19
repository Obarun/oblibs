/* 
 * mill.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * */

#include <oblibs/mill.h>

#include <string.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

static char tend[64+2] ;
//static char topen[64+2] ;

char mill_next(char const *s,size_t *pos)
{
	char c ;
	if (*pos >= strlen(s)) return -1 ;
	c = s[*pos] ;
	(*pos) += 1 ;
	return c ;
}

uint8_t mill_cclass (parse_mill_t *p)
{
	uint8_t *nopen = &p->inner.nopen ;
	uint8_t *nclose = &p->inner.nclose ;
	uint8_t opclequal = 0 ;
	uint8_t range = 0 ;// 0-> out, 1->in
	uint8_t firstopen = 0, firstclose = 0 ;	
	uint8_t r = 0 ;
	char curr = p->inner.curr ;
		
	if (p->open == p->close) 
		opclequal++ ; 
	
	/** open */
	if (curr == p->open)
	{
		if (!(*nopen)) firstopen = 1 ;
		(*nopen)++ ;
	}
	/** close */
	if (curr == p->close)
	{
		if (!(*nclose)) firstclose = 1 ;
		/** we can have a close before an open*/
		if (*nopen)
		{
			(*nclose)++ ;
			/** special exist when close and open are equal
			* in this case do not considere close if we are
			* on firstopen */
			if (opclequal && firstopen) 
				(*nclose)-- ;
		}
	}
	if (curr == '\n')
	{ 
		if (p->flush) p->inner.flushed = 1 ;
		p->inner.jumped = 0 ; 
		p->inner.nline++ ;
	}
	/** we opened already but not closed */
	if ((*nopen))
		if (!(*nclose) || (*nopen) != (*nclose)) range = 1 ;
	
	if (MILL_DBG)
		DBG ("	nopen:%i::nclose:%i::opclequal:%i::firstopen:%i::firstclose:%i",*nopen,*nclose,opclequal,firstopen,firstclose) ;

	/** jump */
	r = mill_inner_loop(curr,p->jump,p->jumplen) ;
	if (r)
	{
		p->inner.jumped = 1 ;
		return JUMP ;
	}
	/** search */
	r = mill_inner_loop(curr,p->search,p->searchlen) ;
	if (r)
	{
		p->inner.search = 1 ;
		return END ;
	}
	/** end */
	r = mill_inner_loop(curr,p->end,p->endlen) ;
	if (r) return END ;
		
	if ((*nopen) && !(*nclose))
	{
		/** skip */
		r = mill_inner_loop(curr,p->skip,p->skiplen) ;
		if (r)
		{		
			if (!p->forceskip) return KEEP ;
			else return IGN ;
		}
				
		/** key to key */
		if (firstopen && !p->forceopen && !p->keepopen) return IGN ;
		else return KEEP ;
	}
	else if ((*nclose)) 
	{
		/** skip */
		r = mill_inner_loop(curr,p->skip,p->skiplen) ;
		if (r)
			if (!p->forceskip) return KEEP ;
		if (p->forceclose) return END ;
		if (*nopen == *nclose) return END ;
		if (range) return KEEP ;
	}

	return IGN ;
}

/** @Return 1 on sucess/end of string
 * @Return -1 system error
 * @Return 0 parsing error */  
int mill_config(parse_mill_t *p, char const *string, stralloc *kp,size_t *pos)
{
	static uint8_t const table[6] = { IGN, KEEP, JUMP, END, EFILE } ;
	uint8_t what = 0, efile = 0 ;
	char const *file = p->file ;
	char j = 0 ;
	int res = -2 ; 
	if (MILL_DBG)
	{
		DBG ("[PARSE PROCESS OF %s STARTS]", p->inner.debug) ;	
		DBG ("open:%i:%c:",p->open,p->open) ;
		DBG ("close:%i:%c:",p->close,p->close) ;
		DBG ("keepopen:%i:",p->keepopen) ;
		DBG ("keeclose:%i:",p->keepclose) ;
		DBG ("forceopen:%i:",p->forceopen) ;
		DBG ("forceclose:%i:",p->forceclose) ;
		DBG ("forceskip:%i:",p->forceskip) ;
		DBG ("flush:%i:\n----\n",p->flush) ;
	}
	while (res < -1)
	{	
		if (MILL_DBG) {
			DBG ("	parse line:%s",string+(*pos)) ;
			DBG ("	len of line:%li",strlen(string)) ;
		}
		p->inner.curr = mill_next(string, pos) ;
		// end of file	
		if (p->inner.curr == -1) what = EFILE ;
		else what = table[mill_cclass(p)] ;
		
		if (p->inner.flushed)
		{
			kp->len = 0 ;
			wild_zero(p) ;
			what = IGN ;
		}
		// end of file	
		//if (p->inner.curr == -1) what = EFILE ;
		if (MILL_DBG)
		{
			DBG ("	pos:%li::current:%c",*pos,p->inner.curr) ;
			DBG ("	nopen:%i::nclose:%i::what:%li::\n	----\n",p->inner.nopen, p->inner.nclose,what) ;
		}
		switch(what)
		{
			case KEEP:
				if (p->inner.nopen)
					if (!stralloc_catb(kp,&p->inner.curr,1)) 
						return -1 ;
				if (MILL_DBG)
					DBG("	string state: %s\n",kp->s) ;
				break ;
			case JUMP:
					while (j != '\n')
					{
						j = mill_next(string,pos) ;
						if (j < 0) {
							 res = 1 ; 
							 break ; //end of string	
						}
					}
					p->inner.nline++ ;
					res = 1 ;
				break ;
			case IGN:
				break ;
			case EFILE:
				efile++ ;
				res = 1 ;
				break ;
			case END:
				if (p->keepclose)
					if (!stralloc_catb(kp,&p->inner.curr,1)) 
						return -1 ;
				res = 1 ;
				break ;
			default: break ;
		}
	}
	p->inner.lastclose = (*pos) ;
	if (p->inner.search || p->inner.jumped) kp->len = 0 ;
	else
	{
		if (!stralloc_0(kp)) return 0 ;
		kp->len-- ;
	}
	if (MILL_DBG)
	{	
		DBG("what:%s:efile:%i:res:%i", !what ? "ign" : \
								what == 1 ? "keep" : \
								what == 2 ? "jump" : \
								what == 3 ? "end" : "efile",efile,res) ;
		DBG ("string kept::%s::len:%li",kp->s,kp->len) ; 
		DBG("[PARSE PROCESS OF %s END]\n", p->inner.debug) ;
	}

	{
		if (p->search && !p->inner.search) return 0 ;
		else if (p->jump && !p->inner.jumped) return 0 ;
	
		if (p->open && !p->inner.nopen){ kp->len = 0 ; return 0 ; }
		if (p->close && !p->inner.nclose){ kp->len = 0 ; return 0 ; }
	}
		
	if (p->check && p->inner.nopen != p->inner.nclose)
	{
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, p->inner.nline-1)] = 0 ;
		char sepopen[2] = { p->open,0 } ;
		char sepclose[2] = { p->close,0 } ;
		strerr_warnw6x("umatched ",(p->inner.nopen > p->inner.nclose) ? sepopen : sepclose," in: ",file," at line: ",fmt) ;
		return -1 ;
	}
	return res ;
}

int mill_prepare(stralloc *dst, char const *string,parse_mill_t *sep,size_t *pos)
{
	int r ;
	size_t stringlen = strlen(string), newpos = (*pos), ebase = sep->endlen, i = 0 ;
	if (!stringlen) return -1 ;
	stralloc kp = STRALLOC_ZERO ;
	
	sep->inner.nclose = sep->inner.nopen = 0 ; 
	
	/** end and skip cannot be the same
	 * open,close and end cannot be the same */
	for (; i < sep->endlen ; i++)
	{
		if (mill_inner_loop(sep->end[i],sep->skip,sep->skiplen))
			return -1 ;
		if (sep->end[i] == sep->open) return -1 ;
		if (sep->end[i] == sep->close) return -1 ;
	}
	/** open,close and skip cannot be the same */
	i = 0 ;
	for (;i < sep->skiplen ; i++)
		if ((sep->skip[i] == sep->open)  || (sep->skip[i] == sep->close)) 
			return -1 ;
	/** In case of search and jump 
	 * open, close, check, skip,
	 * forceopen, forceclose, forceskip must be 0
	 * this limit avoid complex exit status.
	 * Reverse is also true */
	
	if (sep->search || sep->jump)
	{
		sep->open = sep->close = sep->check = sep->skiplen = 0 ;
		sep->skip = 0 ;
		sep->forceopen = sep->forceclose = sep->forceskip = 0 ;
	}
	else
	{
		sep->jumplen = sep->searchlen = 0 ;
		sep->search = sep->jump = 0 ;
		if (sep->forceopen)
			sep->open = string[(*pos)] ;
	
		if (sep->forceclose)
		{	
			i = 0 ;
			while (i < ebase){
				tend[i] = sep->end[i] ;
				i++ ;
			} 
			tend[i++] = string[strlen(string)] ;
			tend[i] = 0 ;
			sep->end = tend ;
			sep->endlen = ebase + 1 ;
		}
	}
	
	r = mill_config(sep,string,&kp,&newpos) ;
	tend[ebase] = 0 ;
	sep->end = tend ;
	sep->endlen = ebase ;
	if (r == -1) goto err ;
	else if (!r || !kp.len) goto freed ;
	if (!stralloc_catb(dst,kp.s,strlen(kp.s)+1)) goto err ;
	
	freed:
		(*pos) = newpos ;
		stralloc_free(&kp) ;
		return r ;
	err:
		(*pos) = newpos ;
		stralloc_free(&kp) ;
		return -1 ;
}

int mill_element(stralloc *dst, char const *string, parse_mill_t *sep,size_t *pos)
{
	int r = 1 ;
	if (!string) return 0 ;
	stralloc kp = STRALLOC_ZERO ;
	
	r = mill_prepare(&kp,string,sep,pos) ;
	if (r == -1) goto err ;
	else if (!r || !kp.len) goto finish ; 
	if (!stralloc_catb(dst,kp.s,strlen(kp.s)+1)) goto err ;

	finish:
	stralloc_free(&kp) ;
	return r ;
	err:
		stralloc_free(&kp) ;
		return -1 ;
}

int mill_string(stralloc *dst, char const *string, parse_mill_t *sep)
{
	size_t len = strlen(string), newpos = 0, pos = 0 ;
	if (!len) return 0 ;
	
	while (pos < len)
	{
		newpos = 0 ;
		if (mill_element(dst,string+pos,sep,&newpos) == -1) return -1 ;
		pos += newpos ;
		sep->inner.pos = pos ;
	}
	return 1 ;
}

int mill_nstring(stralloc *dst, char const *string, parse_mill_t *sep)
{
	size_t len = strlen(string), newpos = 0, pos = 0 ;
	if (!len) return 0 ;
	
	for (;pos < len; pos += strlen(string + pos)+1)
	{
		newpos = 0 ;
		if (mill_element(dst,string+pos,sep,&newpos) == -1) return -1 ;
		pos += newpos ;
		sep->inner.pos = pos ;
	}
	return 1 ;
}

// helper
int mill_inner_loop(char curr,char const *s,size_t len)
{
	size_t i = 0 ;
	for (i = 0 ; i < len ; i++)
		if (curr == s[i])
			return 1 ;
	return 0 ;
}

void wild_zero(parse_mill_t *m)
{
	m->inner.nopen = m->inner.nclose = m->inner.jumped = m->inner.flushed = m->inner.search = 0 ;
}

void wild_zero_nline(parse_mill_t *m)
{
	m->inner.nline = 1 ;
}
