/* cddlib.c: cdd library  (library version of cdd)
   written by Komei Fukuda, fukuda@ifor.math.ethz.ch
   Version 0.93a, August 11, 2003
   Standard ftp site: ftp.ifor.math.ethz.ch, Directory: pub/fukuda/cdd
*/

/* cdd : C-Implementation of the double description method for
   computing all vertices and extreme rays of the polyhedron 
   P= {x :  b - A x >= 0}.
   Please read COPYING (GNU General Public Licence) and
   the manual cddlibman.tex for detail.
*/

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* The first version C0.21 was created on November 10,1993 
   with Dave Gillespie's p2c translator 
   from the Pascal program pdd.p written by Komei Fukuda. 
*/

#include "setoper.h" 
  /* set operation library header (Ver. June 1, 2000 or later) */
#include "cdd.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

/* Global Variables */
dd_boolean dd_debug               =dd_FALSE;
/* GLOBAL CONSTANTS (to be set by dd_set_global_constants() */
mytype dd_zero;
mytype dd_one;
mytype dd_purezero;
mytype dd_minuszero;

/* #include <profile.h>    THINK C PROFILER */
/* #include <console.h>    THINK C PROFILER */

void dd_DDInit(dd_ConePtr cone)
{
  cone->Error=dd_NoError;
  cone->CompStatus=dd_InProgress;
  cone->RayCount = 0;
  cone->TotalRayCount = 0;
  cone->FeasibleRayCount = 0;
  cone->WeaklyFeasibleRayCount = 0;
  cone->EdgeCount=0; /* active edge count */
  cone->TotalEdgeCount=0; /* active edge count */
  dd_SetInequalitySets(cone);
  dd_ComputeRowOrderVector(cone);
  cone->RecomputeRowOrder=dd_FALSE;
}

void dd_DDMain(dd_ConePtr cone)
{
  dd_rowrange hh, itemp, otemp;
  dd_boolean localdebug=dd_TRUE;

  if (cone->d<=0){
    cone->Iteration=cone->m;
    cone->FeasibleRayCount=0;
    cone->CompStatus=dd_AllFound;
    goto _L99;
  }
  if (localdebug) {
     fprintf(stderr,"(Initially added rows ) = ");
     set_fwrite(stderr,cone->InitialHalfspaces);
  }
  while (cone->Iteration <= cone->m) {
    dd_SelectNextHalfspace(cone, cone->WeaklyAddedHalfspaces, &hh);
    if (set_member(hh,cone->NonequalitySet)){  /* Skip the row hh */
      if (dd_debug) {
        fprintf(stderr,"*The row # %3ld should be inactive and thus skipped.\n", hh);
      }
      set_addelem(cone->WeaklyAddedHalfspaces, hh);
    } else {
      if (cone->PreOrderedRun)
        dd_AddNewHalfspace2(cone, hh);
      else{
        dd_AddNewHalfspace1(cone, hh);
      }
      set_addelem(cone->AddedHalfspaces, hh);
      set_addelem(cone->WeaklyAddedHalfspaces, hh);
    }
    if (!cone->PreOrderedRun){
      for (itemp=1; cone->OrderVector[itemp]!=hh; itemp++);
        otemp=cone->OrderVector[cone->Iteration];
      cone->OrderVector[cone->Iteration]=hh;
        /* store the dynamic ordering in ordervec */
      cone->OrderVector[itemp]=otemp;
        /* store the dynamic ordering in ordervec */
    }
    if (localdebug){
      fprintf(stderr,"(Iter, Row, #Total, #Curr, #Feas)= %5ld %5ld %9ld %6ld %6ld\n",
        cone->Iteration, hh, cone->TotalRayCount, cone->RayCount,
        cone->FeasibleRayCount);
    }
    if (cone->CompStatus==dd_AllFound||cone->CompStatus==dd_RegionEmpty) {
      set_addelem(cone->AddedHalfspaces, hh);
      goto _L99;
    }
    (cone->Iteration)++;
  }
  _L99:;
  if (cone->d<=0 || cone->newcol[1]==0){ /* fixing the number of output */
     cone->parent->n=cone->LinearityDim + cone->FeasibleRayCount -1;
     cone->parent->ldim=cone->LinearityDim - 1;
  } else {
    cone->parent->n=cone->LinearityDim + cone->FeasibleRayCount;
    cone->parent->ldim=cone->LinearityDim;
  }
}


void dd_InitialDataSetup(dd_ConePtr cone)
{
  long j, r;
  dd_rowset ZSet;
  static dd_Arow Vector1,Vector2;
  static dd_colrange last_d=0;

  if (last_d < cone->d){
    if (last_d>0) {
    for (j=0; j<last_d; j++){
      dd_init(Vector1[j]);
      dd_init(Vector2[j]);
    }
    free(Vector1); free(Vector2);
    }
    Vector1=(mytype*)calloc(cone->d,sizeof(mytype));
    Vector2=(mytype*)calloc(cone->d,sizeof(mytype));
    for (j=0; j<cone->d; j++){
      dd_init(Vector1[j]);
      dd_init(Vector2[j]);
    }
    last_d=cone->d;
  }

  cone->RecomputeRowOrder=dd_FALSE;
  cone->ArtificialRay = NULL;
  cone->FirstRay = NULL;
  cone->LastRay = NULL;
  set_initialize(&ZSet,cone->m);
  dd_AddArtificialRay(cone);
  set_copy(cone->AddedHalfspaces, cone->InitialHalfspaces);
  set_copy(cone->WeaklyAddedHalfspaces, cone->InitialHalfspaces);
  dd_UpdateRowOrderVector(cone, cone->InitialHalfspaces);
  for (r = 1; r <= cone->d; r++) {
    for (j = 0; j < cone->d; j++){
      dd_set(Vector1[j], cone->B[j][r-1]);
      dd_neg(Vector2[j], cone->B[j][r-1]);
    }
    dd_Normalize(cone->d, Vector1);
    dd_Normalize(cone->d, Vector2);
    dd_ZeroIndexSet(cone->m, cone->d, cone->A, Vector1, ZSet);
    if (set_subset(cone->EqualitySet, ZSet)){
      if (dd_debug) {
        fprintf(stderr,"add an initial ray with zero set:");
        set_fwrite(stderr,ZSet);
      }
      dd_AddRay(cone, Vector1);
      if (cone->InitialRayIndex[r]==0) {
        dd_AddRay(cone, Vector2);
        if (dd_debug) {
          fprintf(stderr,"and add its negative also.\n");
        }
      }
    }
  }
  dd_CreateInitialEdges(cone);
  cone->Iteration = cone->d + 1;
  if (cone->Iteration >= cone->m) cone->CompStatus=dd_AllFound;
  set_free(ZSet);
}

dd_boolean dd_DoubleDescription(dd_PolyhedraPtr poly, dd_ErrorType *err)
{
  dd_ConePtr cone=NULL;
  dd_boolean found=dd_FALSE;

  *err=dd_NoError;
  if (poly!=NULL && (poly->child==NULL || poly->child->CompStatus!=dd_AllFound)){
    cone=dd_ConeDataLoad(poly);
    /* create a cone associated with poly by homogenization */
    time(&cone->starttime);
    dd_DDInit(cone);
    if (poly->representation==dd_Generator && poly->m<=0){
       *err=dd_EmptyVrepresentation;
       cone->Error=*err;
    } else {
      dd_FindInitialRays(cone, &found);
      if (found) {
        dd_InitialDataSetup(cone);
        dd_DDMain(cone);
      }
    }
    time(&cone->endtime);
  }
  return found;
}

dd_boolean dd_DoubleDescription2(dd_PolyhedraPtr poly, dd_RowOrderType horder, dd_ErrorType *err)
{
  dd_ConePtr cone=NULL;
  dd_boolean found=dd_FALSE;

  *err=dd_NoError;
  if (poly!=NULL && (poly->child==NULL || poly->child->CompStatus!=dd_AllFound)){
    cone=dd_ConeDataLoad(poly);
    /* create a cone associated with poly by homogenization */
    cone->HalfspaceOrder=horder;  /* set the row order */
    time(&cone->starttime);
    dd_DDInit(cone);
    if (poly->representation==dd_Generator && poly->m<=0){
       *err=dd_EmptyVrepresentation;
       cone->Error=*err;
    } else {
      dd_FindInitialRays(cone, &found);
      if (found) {
        dd_InitialDataSetup(cone);
        dd_DDMain(cone);
      }
    }
    time(&cone->endtime);
  }
  return found;
}

dd_boolean dd_DDInputAppend(dd_PolyhedraPtr *poly, dd_MatrixPtr M,
  dd_ErrorType *err)
{
  /* This is imcomplete.  It simply solves the problem from scratch.  */
  dd_boolean found;
  dd_ErrorType error;

  if ((*poly)->child!=NULL) dd_FreeDDMemory(*poly);
  dd_AppendMatrix2Poly(poly, M);
  (*poly)->representation=dd_Inequality;
  found=dd_DoubleDescription(*poly, &error);
  *err=error;
  return found;
}

dd_boolean dd_DDFile2File(char *ifile, char *ofile, dd_ErrorType *err)
{
  /* The representation conversion from an input file to an outfile.  */
  /* modified by D. Avis to allow stdin/stdout */
  dd_boolean found=dd_TRUE;
  FILE *reading=NULL,*writing=NULL;
  dd_PolyhedraPtr poly;
  dd_MatrixPtr M, A, G;

  if (strcmp(ifile,"**stdin") == 0 )
     reading = stdin;
  else if ( ( reading = fopen(ifile, "r") )!= NULL) {
    fprintf(stderr,"input file %s is open\n", ifile);
   }
  else{
    fprintf(stderr,"The input file %s not found\n",ifile);
    found=dd_FALSE;
    *err=dd_IFileNotFound;
    goto _L99;
  }

  if (found){
    if (strcmp(ofile,"**stdout") == 0 )
      writing = stdout;
    else if ( (writing = fopen(ofile, "w") ) != NULL){
      fprintf(stderr,"output file %s is open\n",ofile);
      found=dd_TRUE;
    } else {
      fprintf(stderr,"The output file %s cannot be opened\n",ofile);
      found=dd_FALSE;
      *err=dd_OFileNotOpen;
      goto _L99;
    }
  }

  M=dd_PolyFile2Matrix(reading, err);
  if (*err!=dd_NoError){
    goto _L99;
  } poly=dd_DDMatrix2Poly(M, err);  /* compute the second representation */ dd_FreeMatrix(M);

  if (*err==dd_NoError) {
    dd_WriteRunningMode(writing, poly);
    A=dd_CopyInequalities(poly);
    G=dd_CopyGenerators(poly);

    if (poly->representation==dd_Inequality) {
      dd_WriteMatrix(writing,G);
     } else {
      dd_WriteMatrix(writing,A);
    }

    dd_FreePolyhedra(poly);
    dd_FreeMatrix(A);
    dd_FreeMatrix(G);
  } 

_L99: ;
  if (*err!=dd_NoError) dd_WriteErrorMessages(stderr,*err);
  if (reading!=NULL) fclose(reading);
  if (writing!=NULL) fclose(writing);
  return found;
}

/* end of cddlib.c */
