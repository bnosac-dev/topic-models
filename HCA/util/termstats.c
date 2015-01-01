/*
 * Module for reading and initialising term stats
 * Copyright (C) 2014 Wray Buntine
 * All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 *      http://mozilla.org/MPL/2.0/.
 *
 * Author: Wray Buntine (wray.buntine@monash.edu)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "util.h"
#include "yap.h"
#include "termstats.h"

void tstats_free(T_stats_t *ptr) {
  if ( !ptr )
    return;
  free(ptr->Nkt[0]);
  free(ptr->Nkt);
  free(ptr->tokens[0]);
  free(ptr->tokens);
  free(ptr);
}

static int tokenrange(char *collsfile, int DT, int *kmin, int *kmax) {
  FILE *fp;
  int d, i;
  fp = fopen(collsfile,"r");
  *kmin = -1;
  *kmax = -1;
  if ( !fp ) 
    return 1;
  for (d=0; d<DT; d++) {
    int nk;
    int k;
    if ( fscanf(fp," %u", &nk) != 1 ) {
      if ( feof(fp) )
	break;
      yap_sysquit("Cannot read %d-th data line from '%s'\n", d, collsfile);
    }
    for (i=0; i<nk; i++) {
      if ( fscanf(fp," %*d %*d %d", &k) != 1 )
	yap_sysquit("Cannot read %d-th line %d-th token from '%s'\n", 
		    d, i, collsfile);
      if ( *kmin<0 ) {
	*kmin = k;
	*kmax = k+1;
      } else {
	if ( k<*kmin )
	  *kmin = k;
	if ( k>=*kmax )
	  *kmax = k+1;
      }
    }
  }
  fclose(fp);
  return 0;
}

static void readstats(char *collsfile, T_stats_t *ptr, uint32_t *NdTcum, uint16_t *z) {
  FILE *fp;
  int d, i;
  fp = fopen(collsfile,"r");
  if ( !fp ) 
    yap_sysquit("collsfile '%s' not opened\n", collsfile);
  for (i=0; i<ptr->K; i++) {
    int t;
    for (t=0; t<ptr->T; t++)
      ptr->Nkt[i][t] = 0;
  }
  for (d=0; d<ptr->DT; d++) {
    int nk;
    int k, p, len;
    if ( fscanf(fp," %u", &nk) != 1 ) {
      if ( feof(fp) )
	break;
      yap_sysquit("Cannot read %d-th data line from '%s'\n", d, collsfile);
    }
    for (i=0; i<nk; i++) {
      int t, pp, l;
      if ( fscanf(fp," %d %d %d", &p, &len, &k) != 3 )
	yap_sysquit("Cannot read %d-th line %d-th token from '%s'\n", 
		    d, i, collsfile);
      k -= ptr->Kmin;
      assert(k>=0 && k<ptr->K);
      pp = p+NdTcum[d];
      t = z[pp];
      for (l=1; l<len; l++) {
	if ( z[pp+l]!=t ) {
	  t = -1;
	  break;
	}
      }
      if ( t>=0 ) {
	assert(t<ptr->T);
	ptr->Nkt[k][t]++;
      }
    }
  }
  fclose(fp);
}


/*
 *      (*ptr) is preallocated memory where the data structure
 *             is built
 *
 *      all other arguments come from the standard data structures
 */
T_stats_t *tstats_init(uint16_t *z, uint32_t *NdTcum, //  cumsum(NdT)
		       int T, int DT,  // dims
		       char *stem) {
  char *collsname;
  int kmin, kmax;
  T_stats_t *ptr;
  /*
   *   get valid range for terms in data
   */
  collsname = yap_makename(stem,".colls");
  if ( tokenrange(collsname, DT, &kmin, &kmax) ) {
    /*  no .colls file, so quit */
    free(collsname);
    return NULL;
  }
  ptr = malloc(sizeof(*ptr));
  if ( !ptr )
    yap_quit("Out of memory in tstats_init()\n");
  /*
   *    save data
   */
  ptr->T = T;
  ptr->DT = DT;
  if ( kmin==0 || kmax==0 || kmax<kmin ) 
    yap_quit("Term ranges [%d,%d] weird\n", kmin, kmax);
  ptr->Kmin = kmin;
  ptr->K = kmax-kmin;
  /*
   *    load up tokens
   */
  ptr->tokens = read_vocab(stem, kmin, kmax, 50);
  /*
   *    loocate stats
   */
  ptr->Nkt = u32mat(ptr->K,T);
  if ( !ptr->Nkt )
    yap_quit("Cannot allocate Nkt[%d,%d] in tstats_init()\n",
	     ptr->K,T);
  /*
   *    read stats
   */
  readstats(collsname, ptr, NdTcum, z);
  free(collsname);
  return ptr;
}
