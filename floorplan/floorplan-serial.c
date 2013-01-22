/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

/* Original code from the Application Kernel Matrix by Cray */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tsc.h"
tsc_t memcpy_tsc, total_tsc;

#define ROWS 64
#define COLS 64
#define DMAX 64
#define max(a, b) ((a > b) ? a : b)
#define min(a, b) ((a < b) ? a : b)

int solution = -1;

typedef int  coor[2];
typedef char ibrd[ROWS][COLS];

FILE * inputFile;

struct cell {
  int   n;     /* number of different configurations (shapes) [CONF] */
  coor *alt;   /* shapes of different configurations          [CONF] */
  int   top;
  int   bot;
  int   lhs;
  int   rhs;
  int   left;  /* cell on my left  [CONF] */
  int   above; /* cell on my right [CONF] */
  int   next;  /* next cell        [CONF] */
};

struct cell * gcells;

int  MIN_AREA;
ibrd BEST_BOARD;
coor MIN_FOOTPRINT;

int N;

/* compute all possible locations for nw corner for cell */
static int starts(int id, int shape, coor *NWS, struct cell *cells) {
  int i, n, top, bot, lhs, rhs;
  int rows, cols, left, above;

/* size of cell */
  rows  = cells[id].alt[shape][0];
  cols  = cells[id].alt[shape][1];

/* the cells to the left and above */
  left  = cells[id].left;
  above = cells[id].above;

/* if there is a vertical and horizontal dependence */
  if ((left >= 0) && (above >= 0)) {

     /* note that in this case there is only one possible NW corner for the
      * cell */
     top = cells[above].bot + 1;
     lhs = cells[left].rhs + 1;
     bot = top + rows;
     rhs = lhs + cols;

/* if footprint of cell touches the cells to the left and above */
     if ((top <= cells[left].bot) && (bot >= cells[left].top) &&
         (lhs <= cells[above].rhs) && (rhs >= cells[above].lhs))
          { n = 1; NWS[0][0] = top; NWS[0][1] = lhs;  }
     else { n = 0; }

/* if there is only a horizontal dependence */
   } else if (left >= 0) {

/* highest initial row is top of cell to the left - rows */ 
     top = max(cells[left].top - rows + 1, 0);
/* lowest initial row is bottom of cell to the left */
     bot = min(cells[left].bot, ROWS);
     n   = bot - top + 1;

     for (i = 0; i < n; i++) {
         NWS[i][0] = i + top;
         NWS[i][1] = cells[left].rhs + 1;
     }

  } else { /* else, there is a vertical dependence */

/* leftmost initial col is lhs of cell above - cols */
     lhs = max(cells[above].lhs - cols + 1, 0);
/* rightmost initial col is rhs of cell above */
     rhs = min(cells[above].rhs, COLS);
     n   = rhs - lhs + 1;

     for (i = 0; i < n; i++) {
         NWS[i][0] = cells[above].bot + 1;
         NWS[i][1] = i + lhs;
  }  }

  return (n);
}



/* lay the cell down on the board in the rectangular space defined
   by the cells top, bottom, left, and right edges. If the cell can
   not be layed down, return 0; else 1.
*/
static int lay_down(int id, ibrd board, struct cell *cells) {
  int  i, j, top, bot, lhs, rhs;

  top = cells[id].top;
  bot = cells[id].bot;
  lhs = cells[id].lhs;
  rhs = cells[id].rhs;

  for (i = top; i <= bot; i++) {
  for (j = lhs; j <= rhs; j++) {
      if (board[i][j] == 0) board[i][j] = (char)id;
      else                  return(0);
  } }

  return (1);
}


#define read_integer(file,var) \
  if ( fscanf(file, "%d", &var) == EOF ) {\
	printf(" Bogus input file\n");\
	exit(-1);\
  }

static void read_inputs() {
  int i, j, n;

  read_integer(inputFile,n);
  N = n;
  
  gcells = (struct cell *) malloc((n + 1) * sizeof(struct cell));

  gcells[0].n     =  0;
  gcells[0].alt   =  0;
  gcells[0].top   =  0;
  gcells[0].bot   =  0;
  gcells[0].lhs   = -1;
  gcells[0].rhs   = -1;
  gcells[0].left  =  0;
  gcells[0].above =  0;
  gcells[0].next  =  0;

  for (i = 1; i < n + 1; i++) {

      read_integer(inputFile, gcells[i].n);
      gcells[i].alt = (coor *) malloc(gcells[i].n * sizeof(coor));

      for (j = 0; j < gcells[i].n; j++) {
          read_integer(inputFile, gcells[i].alt[j][0]);
          read_integer(inputFile, gcells[i].alt[j][1]);
      }

      read_integer(inputFile, gcells[i].left);
      read_integer(inputFile, gcells[i].above);
      read_integer(inputFile, gcells[i].next);
      }

  if (!feof(inputFile)) {
      read_integer(inputFile, solution);
  }
}

static void write_outputs() {
  int i, j;

    printf("Minimum area = %d\n\n", MIN_AREA);

    for (i = 0; i < MIN_FOOTPRINT[0]; i++) {
      for (j = 0; j < MIN_FOOTPRINT[1]; j++) {
          if (BEST_BOARD[i][j] == 0) {printf(" ");}
          else                       printf("%c", 'A' + BEST_BOARD[i][j] - 1);
      } 
      printf("\n");
    }  
}

static int add_cell (int id, coor FOOTPRINT, ibrd BOARD, struct cell *CELLS) 
{
  int  i, j, nn, nn2, area;

  /* allocate a new board in stack */
  ibrd board;
  coor footprint, NWS[DMAX];

  nn2 = 0;
/* for each possible shape */
  for (i = 0; i < CELLS[id].n; i++) {
/* compute all possible locations for nw corner */
      nn = starts(id, i, NWS, CELLS);
      nn2 += nn;
/* for all possible locations */
      for (j = 0; j < nn; j++) {
          struct cell *cells = CELLS;
/* extent of shape */
          cells[id].top = NWS[j][0];
          cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
          cells[id].lhs = NWS[j][1];
          cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

          tsc_start(&memcpy_tsc);
          memcpy(board, BOARD, sizeof(ibrd));
          tsc_pause(&memcpy_tsc);

/* if the cell cannot be layed down, prune search */
          if (! lay_down(id, board, cells)) {
             //bots_debug("Chip %d, shape %d does not fit\n", id, i);
             goto _end;
          }

/* calculate new footprint of board and area of footprint */
          footprint[0] = max(FOOTPRINT[0], cells[id].bot+1);
          footprint[1] = max(FOOTPRINT[1], cells[id].rhs+1);
          area         = footprint[0] * footprint[1];

/* if last cell */
          if (cells[id].next == 0) {

/* if area is minimum, update global values */
		  if (area < MIN_AREA) {
			  if (area < MIN_AREA) {
				  MIN_AREA         = area;
				  MIN_FOOTPRINT[0] = footprint[0];
				  MIN_FOOTPRINT[1] = footprint[1];
		                  tsc_start(&memcpy_tsc);
				  memcpy(BEST_BOARD, board, sizeof(ibrd));
		                  tsc_pause(&memcpy_tsc);
				  //bots_debug("N  %d\n", MIN_AREA);
			  }
		  }

/* if area is less than best area */
          } else if (area < MIN_AREA) {
             nn2 += add_cell(cells[id].next, footprint, board,cells);
/* if area is greater than or equal to best area, prune search */
          } else {
             //bots_debug("T  %d, %d\n", area, MIN_AREA);
	  }
_end:;  
}
}
  return nn2;
  }


ibrd board;

void floorplan_init (const char *filename)
{
    int i,j;

    inputFile = fopen(filename, "r");
    
    if(NULL == inputFile) {
        printf("Couldn't open %s for reading\n", filename);
        exit(1);
    }
    
    /* read input file and initialize global minimum area */
    read_inputs();
    MIN_AREA = ROWS * COLS;
    
    /* initialize board is empty */
    for (i = 0; i < ROWS; i++)
    for (j = 0; j < COLS; j++) board[i][j] = 0;
    
}

void compute_floorplan (void)
{
    tsc_init(&memcpy_tsc);
    tsc_init(&total_tsc);
    coor footprint;
    /* footprint of initial board is zero */
    footprint[0] = 0;
    footprint[1] = 0;
    printf("Computing floorplan ");
    tsc_start(&total_tsc);
    add_cell(1, footprint, board, gcells);
    tsc_pause(&total_tsc);
    printf(" completed!\n");
    printf("total :%20lu ticks\n", tsc_getticks(&total_tsc));
    printf("memcpy:%20lu ticks\n", tsc_getticks(&memcpy_tsc));

}

void floorplan_end (void)
{
    /* write results */
    write_outputs();
}

int floorplan_verify (void)
{
    if (solution != -1 )
      return MIN_AREA == solution;
    else
      return -1;
}

int main(int argc, const char *argv[])
{

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input>\n", argv[0]);
        exit(1);
    }

    floorplan_init(argv[1]);
    floorplan_end();
    floorplan_verify();

    return 0;
}
