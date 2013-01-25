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

#ifdef NO_CILK
#define cilk_spawn
#define cilk_sync
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
//#include <cilk/reducer_min.h>
#include <cilk/reducer_opadd.h>
#define YES_CILK
#endif

#if defined(FLOORPLAN_XVARRAY)
#include "xarray.h"
#include "xvarray.h"
#endif

#include "misc.h"
#include "tsc.h"
#include "xcnt.h"

#include "floorplan_stats.h"

DECLARE_FLOORPLAN_STATS

int cutoff_level = 5;

// lock to replace OMP critical sections
spinlock_t xlock;

#define ROWS 64
#define COLS 64
#define DMAX 64
#define max(a, b) ((a > b) ? a : b)
#define min(a, b) ((a < b) ? a : b)

#if defined(FLOORPLAN_XVARRAY)
#define SLA_P             0.5
#define SLA_MAX_LEVEL     5
#define XARR_CHUNK_SIZE   COLS // to simplify things, we keep a chunk per row
#endif

int solution = -1;

typedef int  coor[2];
typedef char (*pibrd)[COLS];

typedef char board_elem_t;
#if !defined(FLOORPLAN_XVARRAY)
typedef board_elem_t ibrd[ROWS][COLS];
#else
typedef xvarray_t *ibrd;

__thread void *xStats = NULL;
#endif


struct cell {
  int   n;    /* number of alternative orientations (INPUT) */
  coor *alt;  /* array of alternative orientations  (INPUT) */
  int   top;
  int   bot;
  int   lhs;
  int   rhs;
  int   left;  /* left dependence  (-1:no dependence) (INPUT) */
  int   above; /* above dependence (-1:no dependence) (INPUT) */
  int   next;  /* next cell */
};

struct cell * gcells;

//CILK_C_REDUCER_MIN_TYPE(int) min_area;

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

  } else {

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
static int
lay_down(int id, ibrd brd, struct cell *cells)
{

    int  i, j, top, bot, lhs, rhs, ret;

    //FLOORPLAN_TIMER_START(lay_down);

    top = cells[id].top;
    bot = cells[id].bot;
    lhs = cells[id].lhs;
    rhs = cells[id].rhs;

    #if !defined(FLOORPLAN_XVARRAY)
    for (i = top; i <= bot; i++) {
        for (j = lhs; j <= rhs; j++) {
            if (brd[i][j] == 0) {
                brd[i][j] = (char)id;
            } else {
                ret = 0;
                FLOORPLAN_XCNT_INC(lay_down_fail);
                goto end;
            }
        }
    }
    #elif XARR_CHUNK_SIZE == COLS
    #if 0
    for (i = top; i <= bot; i++) {
        size_t idx = i*COLS, nelems;
        char *row = xvarray_getchunk_rdwr(brd, idx, &nelems);
        for (j=lhs; j <= rhs; j++) {
            if (row[j] == 0) {
                row[j] = (char)id;
            } else {
                ret = 0;
                FLOORPLAN_XCNT_INC(lay_down_fail);
                goto end;
            }
        }
    }
    #else
    size_t nrows = bot - top + 1;
    xvchunk_t chunks[nrows];

    FLOORPLAN_XCNT_ADD(chunks, nrows);
    xvchunk_init(brd, chunks + 0, top*COLS);
    assert(chunks[0].off == 0);
    for (i=0; ;) {
        size_t nelems;
        FLOORPLAN_TIMER_START(lay_down_read);
        const char *row = xvchunk_getrd(chunks + i, &nelems);
        FLOORPLAN_TIMER_PAUSE(lay_down_read);
        for (j=lhs; j <= rhs; j++) {
            if (row[j] != 0) {
                ret = 0;
                FLOORPLAN_XCNT_INC(lay_down_fail);
                goto end;
            }
        }

        if (i++ == nrows)
            break;

        xvchunk_getnext(chunks +i-1, chunks +i);
    }

    /*
    size_t ch_i=0;
    FLOORPLAN_TIMER_START(lay_down_read);
    for (i = top; i <= bot; i++) {
        size_t idx = i*COLS, nelems;
        xvchunk_t *ch = chunks + ch_i++;
        xvchunk_init(brd, ch, idx);
        const char *row = xvchunk_getrd(ch, &nelems);
        for (j=lhs; j <= rhs; j++) {
            if (row[j] != 0) {
                ret = 0;
                FLOORPLAN_XCNT_INC(lay_down_fail);
                FLOORPLAN_TIMER_PAUSE(lay_down_read);
                goto end;
            }
        }
    }
    FLOORPLAN_TIMER_PAUSE(lay_down_read);
    */

    FLOORPLAN_TIMER_START(lay_down_write);
    for (unsigned r=0; r<nrows; r++) {
        size_t nelems;
        char *row = xvchunk_getrdwr(chunks + r, &nelems);
        for (j=lhs; j <= rhs; j++) {
                row[j] = (char)id;
        }
    }
    /*
    for (i = top; i <= bot; i++) {
        size_t idx = i*COLS, nelems;
        char *row = xvarray_getchunk_rdwr(brd, idx, &nelems);
        for (j=lhs; j <= rhs; j++) {
                row[j] = (char)id;
        }
    }
    */
    FLOORPLAN_TIMER_PAUSE(lay_down_write);
    #endif
    #else
    #error "NYI"
    #endif

    ret = 1;
    FLOORPLAN_XCNT_INC(lay_down_ok);
end:
    //FLOORPLAN_TIMER_PAUSE(lay_down);
    return ret;
}


#define read_integer(file,var) \
  if ( fscanf(file, "%d", &var) == EOF ) {\
    printf(" Bogus input file\n");\
    exit(-1);\
  }

/* file format:
 * <number of cells>
 *
 * <number of alternative orientations>
 * <orientation 1>
 * ...
 * <orientation N>
 * <left requirement> <above requirement>
 * <next cell>
 *
 * Note that there is a dummy cell in location 0
 */
static void read_inputs(FILE *inputFile) {
  int i, j, n;

  read_integer(inputFile,n);
  N = n;

  gcells = (struct cell *) malloc((n + 1) * sizeof(struct cell));

  /* this is the first, dummy cell */
  // it is used in restriction (.left, .above), so it is on the first corner
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
        // first number is the number of alternative configurations for the cell
        read_integer(inputFile, gcells[i].n);
        // based on that we allocate one tuple per alternative configuration
        gcells[i].alt = (coor *) malloc(gcells[i].n * sizeof(coor));
        // read alternative configuration
        for (j = 0; j < gcells[i].n; j++) {
            read_integer(inputFile, gcells[i].alt[j][0]);
            read_integer(inputFile, gcells[i].alt[j][1]);
        }
        // read placement restrictions, left and above
        read_integer(inputFile, gcells[i].left);
        read_integer(inputFile, gcells[i].above);
        // next cell
        read_integer(inputFile, gcells[i].next);
    }

    if (!feof(inputFile)) {
        read_integer(inputFile, solution);
    }
}


static void write_outputs() {
  int i, j;

    printf("Minimum area = %d\n\n", MIN_AREA);

    #if !defined(FLOORPLAN_XVARRAY)
    for (i = 0; i < MIN_FOOTPRINT[0]; i++) {
        for (j = 0; j < MIN_FOOTPRINT[1]; j++) {
            if (BEST_BOARD[i][j] == 0)
                printf(" ");
            else
                printf("%c", 'A' + BEST_BOARD[i][j] - 1);
      }
      printf("\n");
    }
    #elif XARR_CHUNK_SIZE == COLS
    for (i = 0; i <= MIN_FOOTPRINT[0]; i++) {
        size_t idx = i*COLS, nelems;
        const char *row = xvarray_getchunk_rd(BEST_BOARD, idx, &nelems);
        for (j=0; j <= MIN_FOOTPRINT[1]; j++) {
            if (row[j] == 0)
                printf(" ");
            else
                printf("%c", 'A' + row[j] - 1);
        }
        printf("\n");
    }
    #else
    #error "NYI"
    #endif
}

static bool
try_update_min(int area, coor footprint, ibrd board)
{
    bool updated;

    updated = false;
    if (area < MIN_AREA) {
        spin_lock(&xlock);
        if (area < MIN_AREA) {
            updated          = true;
            MIN_AREA         = area;
            MIN_FOOTPRINT[0] = footprint[0];
            MIN_FOOTPRINT[1] = footprint[1];
            #if defined(FLOORPLAN_XVARRAY)
            xvarray_t *old = BEST_BOARD;
            BEST_BOARD = board;
            if (old)
                xvarray_destroy(old);
            #else
            memcpy(BEST_BOARD, board, sizeof(ibrd));
            #endif
            dmsg("N  %d\n", MIN_AREA);
        }
        spin_unlock(&xlock);
    }
    return updated;
}

static unsigned __attribute__((unused))
add_cell_ser(int id, coor FOOTPRINT, ibrd BOARD, struct cell *CELLS)
{
  int  i, j; // loop variables
  int nn, nn2, area;

  ibrd brd;
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
            bool __attribute__((unused)) destroy = true;
            /* extent of shape */
            cells[id].top = NWS[j][0];
            cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
            cells[id].lhs = NWS[j][1];
            cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

            FLOORPLAN_XCNT_INC(branch);
            FLOORPLAN_TIMER_START(xvarray_branch);
            #if defined(FLOORPLAN_XVARRAY)
            brd = xvarray_branch(BOARD);
            #else
            memcpy(brd, BOARD, sizeof(ibrd));
            #endif
            FLOORPLAN_TIMER_PAUSE(xvarray_branch);

            /* if the cell cannot be layed down, prune search */
            if (!lay_down(id, brd, cells)) {
                dmsg("Chip %d, shape %d does not fit\n", id, i);
                #if defined(FLOORPLAN_XVARRAY)
                xvarray_destroy(brd);
                #endif
                continue;
            }

            /* calculate new footprint of board and area of footprint */
            footprint[0] = max(FOOTPRINT[0], cells[id].bot+1);
            footprint[1] = max(FOOTPRINT[1], cells[id].rhs+1);
            area         = footprint[0] * footprint[1];

            /* if last cell */
            if (cells[id].next == 0) {
                /* try  to update globals */
                /* if area is minimum, update global values */
                destroy = !try_update_min(area, footprint, brd);
            } else if (area < MIN_AREA) {
                /* if area is less than best area */
                FLOORPLAN_XCNT_INC(commit);
                nn2 += add_cell_ser(cells[id].next, footprint, brd, cells);
            } else {
                /* if area is greater than or equal to best area, prune search */
                //dmsg("T  %d, %d\n", area, MIN_AREA);
            }

            #if defined(FLOORPLAN_XVARRAY)
            if (destroy)
                xvarray_destroy(brd);
            #endif

        }
    }
    return nn2;
}

static unsigned
add_cell(int id,
         coor footprint_in,
         ibrd BOARD,
         struct cell *cells_in,
         int level)
{
    int  i, nnc, nnl;

    nnc = nnl = 0;
    //CILK_C_REDUCER_OPADD(nnc__, int, 0);

    /* for each possible shape */
    for (i = 0; i < cells_in[id].n; i++) {
        int nn;
        coor NWS[DMAX];
        /* compute all possible locations for nw corner */
        nn = starts(id, i, NWS, cells_in);
        nnl += nn;
        /* for all possible locations */
        cilk_for (int j = 0; j < nn; j++) { // parallel for
            int area;
            ibrd brd;
            coor footprint;
            struct cell *cells;
            bool __attribute__((unused)) destroy = true;

            int __attribute__((unused)) myid = __cilkrts_get_worker_number();
            myfloorplan_stats_set(myid);

            //xcnt_inc(branches);
            cells = (struct cell *)alloca(sizeof(struct cell)*(N+1));
            memcpy(cells,cells_in,sizeof(struct cell)*(N+1));

            /* extent of shape */
            cells[id].top = NWS[j][0];
            cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
            cells[id].lhs = NWS[j][1];
            cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

            FLOORPLAN_XCNT_INC(branch);
            FLOORPLAN_TIMER_START(xvarray_branch);
            #if defined(FLOORPLAN_XVARRAY)
            brd = xvarray_branch(BOARD);
            #else
            memcpy(brd, BOARD, sizeof(ibrd));
            #endif
            FLOORPLAN_TIMER_PAUSE(xvarray_branch);

            /* if the cell cannot be layed down, prune search */
            if (!lay_down(id, brd, cells)) {
                dmsg("Chip %d, shape %d does not fit\n", id, i);
            } else {
                /* calculate new footprint of board and area of footprint */
                footprint[0] = max(footprint_in[0], cells[id].bot+1);
                footprint[1] = max(footprint_in[1], cells[id].rhs+1);
                area         = footprint[0] * footprint[1];

                /* if last cell */
                if (cells[id].next == 0) {
                    destroy = !try_update_min(area, footprint, brd);
                    /* if area is minimum, update global values */
                } else if (area < MIN_AREA) {
                    FLOORPLAN_XCNT_INC(commit);
                    int my_nnc;
                    if (level < cutoff_level) {
                        my_nnc = add_cell(cells[id].next, footprint, brd, cells, level + 1);
                    } else {
                        my_nnc = add_cell_ser(cells[id].next, footprint, brd, cells);
                    }
                    //int my_nnc = add_cell_ser(cells[id].next, footprint, brd,cells);

                    spin_lock(&xlock);
                    nnc += my_nnc;
                    spin_unlock(&xlock);

                    //REDUCER_VIEW(nnc__) += my_nnc;
                } else {
                    /* if area is greater than or equal to best area, prune search */
                    dmsg("T  %d, %d\n", area, MIN_AREA);
                }
            }

            #if defined(FLOORPLAN_XVARRAY)
            if (destroy)
                xvarray_destroy(brd);
            #endif
        }
    }

    //printf("%d %d\n", nnc, REDUCER_VIEW(nnc__));
    return nnc+nnl;
}


void floorplan_init (const char *filename, ibrd board)
{
    FILE *inputFile;

    inputFile = fopen(filename, "r");

    if (NULL == inputFile) {
        printf("Couldn't open %s file for reading\n", filename);
        exit(1);
    }

    /* read input file and initialize global minimum area */
    read_inputs(inputFile);
    MIN_AREA = ROWS * COLS;

    #if defined(FLOORPLAN_XVARRAY)
    // do nothing, initialization has already been performed in
    // board_xva_alloc()
    #else
    int i,j;
    /* initialize board is empty */
    for (i = 0; i < ROWS; i++)
    for (j = 0; j < COLS; j++)
        board[i][j] = 0;
    #endif

}

unsigned
compute_floorplan(ibrd board)
{
    int ret;
    coor footprint;
    /* footprint of initial board is zero */
    footprint[0] = 0;
    footprint[1] = 0;
    printf("Computing floorplan \n");

    //tsc_init(&total_tsc); tsc_start(&total_tsc);
    //tsc_init(&memcpy_tsc);
    //ret  = add_cell_ser(1, footprint, board, gcells);
    ret  = add_cell(1, footprint, board, gcells,0);
    //tsc_pause(&total_tsc);
    return ret;
}

void floorplan_end (void)
{
    /* write results */
    write_outputs();
}

void floorplan_verify (void)
{
    if (solution == -1)
        return;

    if (MIN_AREA != solution) {
        fprintf(stderr, "FAIL\n");
        abort();
    } else printf("VERIFIED\n");
}

#if defined(FLOORPLAN_XVARRAY)
static ibrd
board_xva_alloc(void)
{
    
    xarray_t *xarr;
    xvarray_t *xvarr;

    xarr = xarray_create(&(struct xarray_init) {
            .elem_size = sizeof(board_elem_t),
            .da = {
                    .elems_alloc_grain = XARR_CHUNK_SIZE,
                    .elems_init = (ROWS*COLS)
            },
            .sla = {
                    .p                =  SLA_P,
                    .max_level        =  SLA_MAX_LEVEL,
                    .elems_chunk_size =  XARR_CHUNK_SIZE,
            }
    });

    xarray_append_set(xarr, 0, ROWS*COLS);
    xvarr = xvarray_create(xarr);
    return xvarr;
}
#endif


int main(int argc, const char *argv[])
{

    unsigned nworkers, steps;
    spinlock_init(&xlock);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input>\n", argv[0]);
        exit(1);
    }

    const char *input = argv[1];
    if (argc > 2) {
        cutoff_level = atoi(argv[2]);
    }

    nworkers = __cilkrts_get_nworkers();

    floorplan_stats_create(nworkers);

    ibrd board;
    #if defined(FLOORPLAN_XVARRAY)
    // initialize board
    board = board_xva_alloc();
    BEST_BOARD = NULL;
    #endif

    floorplan_init(argv[1], board);

    TSC_MEASURE_TICKS(total_ticks, {
        steps = compute_floorplan(board);
    })

    floorplan_end();
    floorplan_verify();

    printf("nthreads:%u steps:%u input:%s cutoff:%d\n",
            nworkers, steps, input, cutoff_level);
    tsc_report_ticks(" TOTAL", total_ticks);
    floorplan_stats_report(nworkers, total_ticks);

    return 0;
}

// vim:expandtab:tabstop=8:shiftwidth=4:softtabstop=4
