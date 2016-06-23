#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <mpi.h>
#define MP_BARRIER()      MPI_Barrier(MPI_COMM_WORLD)
#define MP_FINALIZE()     MPI_Finalize()
#define MP_INIT(arc,argv) MPI_Init(&(argc),&(argv))
#define MP_MYID(pid)      MPI_Comm_rank(MPI_COMM_WORLD, (pid))
#define MP_PROCS(pproc)   MPI_Comm_size(MPI_COMM_WORLD, (pproc))
#define MP_FLUSH(proc)    MPI_Win_flush_local(proc, win)

#define ELEMS 500

#define DIM1 5
#define DIM2 3
#ifdef __sun
/* Solaris has shared memory shortages in the default system configuration */
# define DIM3 6
# define DIM4 5
# define DIM5 4
#elif defined(__alpha__)
# define DIM3 8
# define DIM4 5
# define DIM5 6
#else
# define DIM3 8
# define DIM4 9
# define DIM5 7
#endif
#define DIM6 3
#define DIM7 2


#define OFF 1
#define EDIM1 (DIM1+OFF)
#define EDIM2 (DIM2+OFF)
#define EDIM3 (DIM3+OFF)
#define EDIM4 (DIM4+OFF)
#define EDIM5 (DIM5+OFF)
#define EDIM6 (DIM6+OFF)
#define EDIM7 (DIM7+OFF)

#define DIMS 4
#define MAXDIMS 7
#define MAX_DIM_VAL 50 
#define LOOP 200

#define BASE 100.
#define MAXPROC 128
#define TIMES 100

/***************************** macros ************************/
#define COPY(src, dst, bytes) memcpy((dst),(src),(bytes))
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#define ABS(a) (((a) <0) ? -(a) : (a))

/***************************** global data *******************/
MPI_Win win;
int me, nproc;
void* work[MAXPROC]; /* work array for propagating addresses */

/*\ generate random range for a section of multidimensional array 
\*/
void get_range(int ndim, int dims[], int lo[], int hi[]) 
{
	int dim;
	for(dim=0; dim <ndim;dim++){
		int toss1, toss2;
		toss1 = rand()%dims[dim];
		toss2 = rand()%dims[dim];
		if(toss1<toss2){
			lo[dim]=toss1;
			hi[dim]=toss2;
		}else {
  			hi[dim]=toss1;
			lo[dim]=toss2;
		}
    }
}



/*\ generates a new random range similar to the input range for an array with specified dimensions
\*/
void new_range(int ndim, int dims[], int lo[], int hi[],int new_lo[], int new_hi[])
{
	int dim;
	for(dim=0; dim <ndim;dim++){
		int toss, range;
		int diff = hi[dim] -lo[dim]+1;
		assert(diff <= dims[dim]);
                range = dims[dim]-diff;
                toss = (range > 0)? rand()%range : lo[dim];
		new_lo[dim] = toss;
		new_hi[dim] = toss + diff -1;
		assert(new_hi[dim] < dims[dim]);
		assert(diff == (new_hi[dim] -new_lo[dim]+1));
	}
}





/*\ print range of ndim dimensional array with two strings before and after
\*/
void print_range(char *pre,int ndim, int lo[], int hi[], char* post)
{
	int i;

	printf("%s[",pre);
	for(i=0;i<ndim;i++){
		printf("%d:%d",lo[i],hi[i]);
		if(i==ndim-1)printf("] %s",post);
		else printf(",");
	}
}

/*\ print subscript of ndim dimensional array with two strings before and after
\*/
void print_subscript(char *pre,int ndim, int subscript[], char* post)
{
	int i;

	printf("%s [",pre);
	for(i=0;i<ndim;i++){
		printf("%d",subscript[i]);
		if(i==ndim-1)printf("] %s",post);
		else printf(",");
	}
}


/*\ print a section of a 2-D array of doubles
\*/
void print_2D_double(double *a, int ld, int *lo, int *hi)
{
int i,j;
     for(i=lo[0];i<=hi[0];i++){
       for(j=lo[1];j<=hi[1];j++) printf("%13f ",a[ld*j+i]);
       printf("\n");
     }
}
          

/*\ initialize array: a[i,j,k,..]=i+100*j+10000*k+ ... 
\*/
void init(double *a, int ndim, int elems, int dims[], int neg, int offset)
{
  int idx[MAXDIMS];
  int i,dim;

  double o = 900.;
  for (i=0; i<ndim; i++) o *= BASE;

 	for(i=0; i<elems; i++){
		int Index = i;
		double field, val, v;
        
		for(dim = 0; dim < ndim; dim++){
			idx[dim] = Index%dims[dim];
			Index /= dims[dim];
		}
		
        field=1.; val=0.;
		for(dim=0; dim< ndim;dim++){
			val += field*idx[dim];
			field *= BASE;
		}
                v = val;
                if (offset) v += o;
                if (neg) v = -v;
		a[i] = v;
		/* printf("(%d,%d,%d)=%6.0f",idx[0],idx[1],idx[2],val); */
	}
}


/*\ compute Index from subscript
 *  assume that first subscript component changes first
\*/
int Index(int ndim, int subscript[], int dims[])
{
	int idx = 0, i, factor=1;
	for(i=0;i<ndim;i++){
		idx += subscript[i]*factor;
		factor *= dims[i];
	}
	return idx;
}


void update_subscript(int ndim, int subscript[], int lo[], int hi[], int dims[])
{
	int i;
	for(i=0;i<ndim;i++){
		if(subscript[i] < hi[i]) { subscript[i]++; return; }
		subscript[i] = lo[i];
	}
}

	

void compare_patches(double eps, int ndim, double *patch1, int lo1[], int hi1[],
                     int dims1[],double *patch2, int lo2[], int hi2[], 
                     int dims2[])
					           
{
	int i,j, elems=1;	
	int subscr1[MAXDIMS], subscr2[MAXDIMS];
        double diff,max;
    int offset1 = 0, offset2 = 0;

	for(i=0;i<ndim;i++){   /* count # of elements & verify consistency of both patches */
		int diff = hi1[i]-lo1[i];
		assert(diff == (hi2[i]-lo2[i]));
		assert(diff < dims1[i]);
		assert(diff < dims2[i]);
		elems *= diff+1;
		subscr1[i]= lo1[i];
		subscr2[i]=lo2[i];
	}

	
	/* compare element values in both patches */ 
	for(j=0; j< elems; j++){ 
		int idx1, idx2;
		
		idx1 = Index(ndim, subscr1, dims1);	 /* calculate element Index from a subscript */
		idx2 = Index(ndim, subscr2, dims2);

		if(j==0){
			offset1 =idx1;
			offset2 =idx2;
		}
		idx1 -= offset1;
		idx2 -= offset2;
		

                diff = patch1[idx1] - patch2[idx2];
                max  = MAX(ABS(patch1[idx1]),ABS(patch2[idx2]));
                if(max == 0. || max <eps) max = 1.; 

		if(eps < ABS(diff)/max){
			char msg[48];
			sprintf(msg,"(proc=%d):%f",me,patch1[idx1]);
			print_subscript("ERROR: a",ndim,subscr1,msg);
			sprintf(msg,"%f\n",patch2[idx2]);
			print_subscript(" b",ndim,subscr2,msg);
                        printf("\nA = %f B = %f\n", patch1[idx1], patch2[idx2]);
                        fflush(stdout);
                        sleep(1);
                        fprintf(stderr, "Bailing out\n");
                        MPI_Abort(MPI_COMM_WORLD, 1);
		}

		{ /* update subscript for the patches */
		   update_subscript(ndim, subscr1, lo1,hi1, dims1);
		   update_subscript(ndim, subscr2, lo2,hi2, dims2);
		}
	}
				 
	

	/* make sure we reached upper limit */
	/*for(i=0;i<ndim;i++){ 
		assert(subscr1[i]==hi1[i]);
		assert(subscr2[i]==hi2[i]);
	}*/ 
}


void scale_patch(double alpha, int ndim, double *patch1, int lo1[], int hi1[], int dims1[])
{
	int i,j, elems=1;	
	int subscr1[MAXDIMS];

	for(i=0;i<ndim;i++){   /* count # of elements in patch */
		int diff = hi1[i]-lo1[i];
		assert(diff < dims1[i]);
		elems *= diff+1;
		subscr1[i]= lo1[i];
	}

	/* scale element values in both patches */ 
	for(j=0; j< elems; j++){ 
		int idx1, offset1;
		
		idx1 = Index(ndim, subscr1, dims1);	 /* calculate element Index from a subscript */

		if(j==0){
			offset1 =idx1;
		}
		idx1 -= offset1;

		patch1[idx1] *= alpha;
		update_subscript(ndim, subscr1, lo1,hi1, dims1);
	}	
}

#define MMAX 100
/* #define NEWMALLOC */
#ifdef NEWMALLOC
armci_meminfo_t meminfo[MMAX][MAXPROC];
int g_idx=0;
#endif

void create_safe_array(void **a, int elem_size, int ndim, int dims[])
{
     int bytes=elem_size, i;
     void * base;

     assert(ndim<=MAXDIMS);
     for(i=0;i<ndim;i++)bytes*=dims[i];
     // a[me] = malloc(bytes);
     MPI_Win_allocate(bytes, 1, MPI_INFO_NULL, MPI_COMM_WORLD, (void *)&base, &win);
     MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
     assert(base);
     *a = base;
     MP_BARRIER();
}

void destroy_safe_array()
{
    int rc;
    MP_BARRIER();
    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
    MP_BARRIER();
}

void Strided_to_dtype(int stride_array[/*stride_levels*/], int count[/*stride_levels+1*/],
                      int stride_levels, MPI_Datatype old_type, MPI_Datatype *new_type)
{
  int sizes   [stride_levels+1];
  int subsizes[stride_levels+1];
  int starts  [stride_levels+1];
  int i, old_type_size;

  MPI_Type_size(old_type, &old_type_size);

  /* Eliminate counts that don't count (all 1 counts at the end) */
  for (i = stride_levels+1; (i > 0) && (stride_levels > 0) && (count[i-1] == 1); i--)
    stride_levels--;

  /* A correct strided spec should me monotonic increasing and stride_array[i+1] should
     be a multiple of stride_array[i]. */
  if (stride_levels > 0) {
    for (i = 1; i < stride_levels; i++) {
      assert(stride_array[i] >= stride_array[i-1]);
      /* This assertion is violated by what seems to be valid usage resulting from
       * the new GA API call nga_strided_get during the stride test in GA 5.2.
       * assert((stride_array[i] % stride_array[i-1]) == 0); */
    }
  }

  /* Test for a contiguous transfer */
  if (stride_levels == 0) {
    int elem_count = count[0]/old_type_size;

    assert((count[0] % old_type_size) == 0);
    MPI_Type_contiguous(elem_count, old_type, new_type);
  }

  /* Transfer is non-contiguous */
  else {

    for (i = 0; i < stride_levels+1; i++)
      starts[i] = 0;

    sizes   [stride_levels] = stride_array[0]/old_type_size;
    subsizes[stride_levels] = count[0]/old_type_size;

    assert((stride_array[0] % old_type_size) == 0);
    assert((count[0] % old_type_size) == 0);

    for (i = 1; i < stride_levels; i++) {
      /* Convert strides into dimensions by dividing out contributions from lower dims */
      sizes   [stride_levels-i] = stride_array[i]/stride_array[i-1];
      subsizes[stride_levels-i] = count[i];

      /* This assertion is violated by what seems to be valid usage resulting from
       * the new GA API call nga_strided_get during the stride test in GA 5.2.  
       * assert_msg((stride_array[i] % stride_array[i-1]) == 0, "Invalid striding"); */
    }

    sizes   [0] = count[stride_levels];
    subsizes[0] = count[stride_levels];

    MPI_Type_create_subarray(stride_levels+1, sizes, subsizes, starts, MPI_ORDER_C, old_type, new_type);
  }
}

int loA[MAXDIMS], hiA[MAXDIMS];
int dimsA[MAXDIMS]={DIM1,DIM2,DIM3,DIM4,DIM5,DIM6, DIM7};
int loB[MAXDIMS], hiB[MAXDIMS];
int dimsB[MAXDIMS]={EDIM1,EDIM2,EDIM3,EDIM4,EDIM5,EDIM6,EDIM7};
int count[MAXDIMS];
int strideA[MAXDIMS], strideB[MAXDIMS];
int loC[MAXDIMS], hiC[MAXDIMS];
int idx[MAXDIMS]={0,0,0,0,0,0,0};

    
void test_dim(int ndim)
{
	int dim,elems;
	int i,j, proc;
	/* double a[DIM4][DIM3][DIM2][DIM1], b[EDIM4][EDIM3][EDIM2][EDIM1];*/
        double *b;
        double *a, *a1, *a2, *c;
        int ridx;
        MPI_Datatype typeA, typeB;
        int rstrideB[MAXDIMS];
        int rcount[MAXDIMS];
        int pidx1 = -1, pidx2 = -1, pidx3 = -1;

	elems = 1;   
        strideA[0]=sizeof(double); 
        strideB[0]=sizeof(double);
	for(i=0;i<ndim;i++){
		strideA[i] *= dimsA[i];
		strideB[i] *= dimsB[i];
                if(i<ndim-1){
                     strideA[i+1] = strideA[i];
                     strideB[i+1] = strideB[i];
                }
		elems *= dimsA[i];
	}

        /* create shared and local arrays */
        create_safe_array((void**)&b, sizeof(double),ndim,dimsB);
        a1 = (double *)malloc(sizeof(double)*elems);
        assert(a1);
        a2 = (double *)malloc(sizeof(double)*elems);
        assert(a2);
        c = (double *)malloc(sizeof(double)*elems);
        assert(c);

	init(a1, ndim, elems, dimsA, me!=0, 0);
	init(a2, ndim, elems, dimsA, me!=0, 1);
	
	if(me==0){
            printf("--------array[%d",dimsA[0]);
	    for(dim=1;dim<ndim;dim++)printf(",%d",dimsA[dim]);
	    printf("]--------\n");
        }
        sleep(1);

        MP_BARRIER();
	for(i=0;i<LOOP;i++){
	    int idx1, idx2, idx3, ridx;
            MPI_Request request;
            if (i%2) {
               a = a2;
            } else {
               a = a1;
            }
	    get_range(ndim, dimsA, loA, hiA);
	    new_range(ndim, dimsB, loA, hiA, loB, hiB);
	    new_range(ndim, dimsA, loA, hiA, loC, hiC);

            proc=nproc-1-me;

            if(me==0){
	       print_range("local",ndim,loA, hiA,"-> ");
	       print_range("remote",ndim,loB, hiB,"-> ");
	       print_range("local",ndim,loC, hiC,"\n");
            }

	    idx1 = Index(ndim, loA, dimsA);
	    idx2 = Index(ndim, loB, dimsB);
	    idx3 = Index(ndim, loC, dimsA);

            MPI_Sendrecv(&idx2, 1, MPI_INT, proc, 666, &ridx, 1, MPI_INT, proc, 666, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	    for(j=0;j<ndim;j++)count[j]=hiA[j]-loA[j]+1;

	    count[0]   *= sizeof(double); /* convert range to bytes at stride level zero */

            Strided_to_dtype(strideA, count, ndim-1, MPI_BYTE, &typeA);
            MPI_Type_commit(&typeA);
            Strided_to_dtype(strideB, count, ndim-1, MPI_BYTE, &typeB);
            MPI_Type_commit(&typeB);

            MPI_Accumulate(a + idx1, 1, typeA, proc, (MPI_Aint)(idx2*sizeof(double)), 1, typeB, MPI_REPLACE, win);
            MP_FLUSH(proc);

            /* note that we do not need Fence here since
             * consectutive operations targeting the same process are ordered */

            MPI_Get_accumulate(NULL, 0, MPI_BYTE, c + idx3, 1, typeA, proc,
                         (MPI_Aint)(idx2*sizeof(double)), 1, typeB, MPI_NO_OP, win);
            MP_FLUSH(proc);

            
            compare_patches(0., ndim, a+idx1, loA, hiA, dimsA, c+idx3, loC, hiC, dimsA);

            pidx1 = idx1;
            pidx2 = idx2;
            pidx3 = idx3; 
        }

        free(c);
        destroy_safe_array();
        free(a);
        MPI_Type_free(&typeA);
        MPI_Type_free(&typeB);
}


/* we need to rename main if linking with frt compiler */
#ifdef FUJITSU_FRT
#define main MAIN__
#endif

int main(int argc, char* argv[])
{
    int ndim;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    if(me==0){
       printf("MPI test program (%d processes)\n",nproc); 
       fflush(stdout);
       sleep(1);
    }

    if(me==0){
       printf("\nTesting strided gets and puts\n");
       printf("(Only std output for process 0 is printed)\n\n"); 
       fflush(stdout);
       sleep(1);
    }

    for(ndim=1; ndim<= MAXDIMS; ndim++) test_dim(ndim);
    MP_BARRIER();

    MPI_Finalize();
    return(0);
}
