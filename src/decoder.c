#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define min(a,b) ((a < b) ? a : b)

typedef char bool;

struct region {
  bool is_active;
  int l;
  unsigned char color;
  int y0;
  int x0;
};

unsigned char MASK = 0xfe;

//tries to decode a pixel's value based on the current active regions and up to two bytes in v1, v2
//if only one byte was read, v2 should contain 0.
//return value is the number of bytes to increment input. It is 0 if the current row position is covered by a region or if not enough data was read
//to decode a value
int decode_next(struct region* regions, int rowlen, int x, int y, unsigned char v1, unsigned char v2, unsigned char last_color, unsigned char* output, int* output_incr) {
  if (regions[x].is_active) { //if a region covers this location
    int ilim = min(y - regions[x].y0 + 1,rowlen-x);
    for(int i = 0; i < ilim; i++) {
      output[3*i] = regions[x].color;
    }
    *output_incr = ilim;
    if(regions[x].l == (y - regions[x].y0)) {
      regions[x].is_active = 0;
    }
    return 0;
  } else { //if a region just expired or there was no active region
    if ((v1 & 0x1) && v2) { //if we are reading a nonzero-length region
      regions[x].is_active = 1;
      regions[x].l = v2 & 0x7f;
      regions[x].color = last_color + (v1 & MASK);
      *output = last_color + (v1 & MASK);
      //fprintf(stderr,"last color was %x, offset is %u, at (%d,%d)\n",last_color,v1 & MASK,x,y);
      regions[x].y0 = y;
      regions[x].x0 = x;
      *output_incr = 1;
      return 2;
    } else if (v1 & 0x1) { //if we would be reading a nonzero-length region, but v2 is empty. Read another value and call again.
      *output_incr = 0;
      return 0;
    } else { //if we read a zero-length region
      regions[x].is_active = 0;
      *output = last_color + (v1 & MASK);
      //fprintf(stderr,"last color was %x, offset is %u, at (%d,%d)\n",last_color,v1 & MASK,x,y);
      *output_incr = 1;
      return 1;
    }
  }
}

//decode a file stream
//the first four bytes of the (deflated) file should be the rowlen, in little-endian
void decode_stream(gzFile in, FILE* out) {
  unsigned char v1 = 0;
  unsigned char v2 = 0;
  int rowlen = 0;
  v1 = gzgetc(in);
  rowlen = v1;
  v1 = gzgetc(in);
  rowlen |= ((int)v1 << 8);
  v1 = gzgetc(in);
  rowlen |= ((int)v1 << 16);
  v1 = gzgetc(in);
  rowlen |= ((int)v1 << 24);
  struct region* regions_red = calloc(rowlen,sizeof(struct region));
  struct region* regions_green = calloc(rowlen,sizeof(struct region));
  struct region* regions_blue = calloc(rowlen,sizeof(struct region));
  unsigned char* this_row = calloc(rowlen*3,1);
  int y = 0;
  int input_incr = 2;// start with 2, so we get a fresh pair of inputs for the first go
  int output_incr = 1; 
  while(gzeof(in) == 0) {
    //decode reds, then greens, then blues, then mix them all up and output them
    for(int x = 0; x < rowlen;) {
      if(input_incr == 1) {
        v1 = v2;
        v2 = gzgetc(in);
      } else if(input_incr == 2) {
        v1 = gzgetc(in);
        v2 = gzgetc(in);
      } else if(output_incr == 0) { //handle incomplete reads. Probably impossible but whatever
        v2 = gzgetc(in);
      }
      input_incr = decode_next(regions_red,rowlen,x,y,v1,v2,x == 0 ? 0 : this_row[3*x-3],this_row + 3*x,&output_incr);
      x += output_incr;
    }
    for(int x = 0; x < rowlen;) {
      if(input_incr == 1) {
        v1 = v2;
        v2 = gzgetc(in);
      } else if(input_incr == 2) {
        v1 = gzgetc(in);
        v2 = gzgetc(in);
      } else if(output_incr == 0) { //handle incomplete reads. Probably impossible but whatever
        v2 = gzgetc(in);
      }
      input_incr = decode_next(regions_green,rowlen,x,y,v1,v2,x == 0 ? 0 : this_row[3*x-2],this_row + 3*x+1,&output_incr);
      x += output_incr;
    }
    for(int x = 0; x < rowlen;) {
      if(input_incr == 1) {
        v1 = v2;
        v2 = gzgetc(in);
      } else if(input_incr == 2) {
        v1 = gzgetc(in);
        v2 = gzgetc(in);
      } else if(output_incr == 0) { //handle incomplete reads. Probably impossible but whatever
        v2 = gzgetc(in);
      }
      input_incr = decode_next(regions_blue,rowlen,x,y,v1,v2,x == 0 ? 0 : this_row[3*x-1],this_row + 3*x + 2,&output_incr);
      x += output_incr;
    }
    //output
    fwrite(this_row,1,rowlen*3,out);
    //fprintf(stderr,"finished row %d\n",y);
    y++;
  }
  free(regions_red);
  free(regions_green);
  free(regions_blue);
  free(this_row);
}

int main(int argc,char** argv) {
  gzFile in = gzdopen(fileno(stdin),"r");
  FILE* out = stdout;
  if (argc == 3) {
    gzclose(in);
    in = gzopen(argv[1],"r");
    out = fopen(argv[2],"w");
  }
  decode_stream(in,out);
  gzclose(in);
  fclose(out);
}
