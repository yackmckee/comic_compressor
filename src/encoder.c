#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)
#define square(a) (a*a)
#define rref(x) (row[x] & MASK)
#define diff(a,b) ((a & 1) == (b & 1) ? (a - b) : ((a & MASK) - (b & MASK)))


typedef char bool;

//a struct that contains information about an active region
struct region {
  bool is_active;
  int l;
  unsigned char* output;
  unsigned char color;
};

long avg_area_num = 0;
long avg_area_denom = 0;
long num_bad_regions = 0;

//the bitmask to apply to all values. The last bit must be unset, but it is possible to get better compression (and a more restricted color space) by making a smaller mask.
unsigned char MASK = 0xfe;

//checks that there is no active edge to the left of the given position
bool no_active_edge(int x, unsigned char this_color, struct region* regions) {
  for(int xo = 1; xo <= x; xo++) {
    if (regions[x-xo].is_active && (regions[x-xo].l == xo - 1) && (regions[x-xo].color == this_color)) {
      return 0;
    }
  }
  return 1;
}

//encodes one component of one row.
//this is implemented as a macro so it takes up less space. Use encode_row_red, encode_row_green, etc.
//diagnostics data has been commented out for speed
#define encode_row(OFFSET) for(int x = OFFSET; x < rowlen*3; x+=3) { /*first we have to get rid of the low bit of every byte */ \
    row[x] &= MASK; \
  }\
  if (nextrow != 0) {\
    for(int x = OFFSET; x < rowlen*3; x+=3) { /*first we have to get rid of the low bit of every byte*/\
      nextrow[x] &= MASK;\
    }\
  }\
  for(int x = 0; x < rowlen;) {\
    if (regions[x].is_active) { /*if this region is active, we need to see if it continues here or not*/\
      int i = x;\
      int imax = min(rowlen,x+regions[x].l+2);\
      for(;i < imax; i++) {\
        if(row[3*i+OFFSET] != regions[x].color) {\
          break;\
        }\
      }\
      /*case: the region continues through this row*/ \
      if (i == imax) {\
        regions[x].l++;\
        /*case: the region at the imax-1 position is active, so we need to deactivate it and write its contents to the output buffer*/ \
        if(regions[i - 1].is_active && (x < (i-1))) {\
          regions[i - 1].is_active = 0;\
          *(regions[i - 1].output) = regions[i - 1].l | 0x80;\
          /*avg_area_denom += square(regions[i - 1].l + 1)/2;\
          avg_area_num += 2;*/\
        }\
        /*case: the region at the x position is at maximum size, so we deactivate & write it */\
        if(regions[x].l == 127) {\
          regions[x].is_active = 0;\
          *(regions[x].output) = 0xff;\
          /*avg_area_denom += square(128)/2;\
          avg_area_num += 2; */\
        }\
        /*increment x to skip over the region we just encoded*/\
        x = i;\
      /*case: the region is no longer active, so we deactivate it*/\
      } else {\
        regions[x].is_active = 0;\
        *(regions[x].output) = regions[x].l | 0x80;\
        /*avg_area_denom += square(regions[x].l + 1)/2;\
        avg_area_num += 2;\
        if(regions[x].l == 0) {\
          num_bad_regions++;\
        }*/ \
        /*case: it is possible to begin a new region starting at this location
         * basically this is true if and only if we are not at the last row and the two pixels below this one match
         * all the trinary expressions are to handle the case when we are at the last column */\
        if((nextrow != 0) && (nextrow[3*x+OFFSET] == row[3*x+OFFSET]) && (nextrow[x < rowlen - 1 ? 3*x + (3+OFFSET) : 3*x+OFFSET] == row[3*x+OFFSET]) && no_active_edge(x,row[3*x+OFFSET],regions)) {\
          regions[x].is_active = 1;\
          regions[x].output = output+1;\
          *output = (row[3*x+OFFSET] - (x > 0 ? row[3*x-(3-OFFSET)] : 0)) | 0x1;\
          *(output + 1) = 0;\
          regions[x].l = 0;\
          regions[x].color = row[3*x+OFFSET];\
          output = output + 2;\
        /*case: it is not possible to begin a new region, so we simply encode the offset from the last pixel*/\
        } else {\
          *output = row[3*x+OFFSET] - (x > 0 ? row[3*x-(3-OFFSET)] : 0);\
          /*$////fprintf(stderr,"zero-length region at %d, last color was %x, this color = %x, offset = %d\n",x,(x > 0 ? row[x - 1] : 0),row[x],*output);*/\
          output = output + 1;\
          /*avg_area_num ++;\
          avg_area_denom ++;*/\
        }\
        /*increment x by just 1*/\
        x++;\
      }\
    /*case: there is no active region here*/\
    } else {\
      /*case: it is possible to begin a new region here*/\
      if ((nextrow != 0) && (nextrow[3*x+OFFSET] == row[3*x+OFFSET]) && (nextrow[x < rowlen - 1 ? 3*x + (3+OFFSET) : 3*x+OFFSET] == row[3*x+OFFSET]) && no_active_edge(x,row[3*x+OFFSET],regions)) {\
        regions[x].is_active = 1;\
        regions[x].output = output+1;\
        *output = (row[3*x+OFFSET] - (x > 0 ? row[3*x-(3-OFFSET)] : 0)) | 0x1;\
        *(output + 1) = 0;\
        regions[x].l = 0;\
        regions[x].color = row[3*x+OFFSET];\
        output = output + 2;\
      /*case: it is not possible to begin a new region here*/\
      } else {\
        *output = row[3*x+OFFSET] - (x > 0 ? row[3*x-(3-OFFSET)] : 0);\
        output = output + 1;\
        /*avg_area_num ++;\
        avg_area_denom ++;*/\
      }\
      x++;\
    }\
  }\
  return output;

unsigned char* encode_row_red(struct region* regions, int rowlen, unsigned char* output, unsigned char* row, unsigned char* nextrow) {
  encode_row(0)
}
unsigned char* encode_row_green(struct region* regions, int rowlen, unsigned char* output, unsigned char* row, unsigned char* nextrow) {
  encode_row(1)
}
unsigned char* encode_row_blue(struct region* regions, int rowlen, unsigned char* output, unsigned char* row, unsigned char* nextrow) {
  encode_row(2)
}

//reads from a file and outputs to a file, calling encode_row repeatedly and feeding the results into a gzip file
//reads and writes only one row at a time, so it can work on streams and can easily be adapted to situations where that is key
void encode_stream(FILE* in, gzFile out, int rowlen) {
  struct region* red_regions = calloc(rowlen,sizeof(struct region));
  struct region* green_regions = calloc(rowlen,sizeof(struct region));
  struct region* blue_regions = calloc(rowlen,sizeof(struct region));
  unsigned char* output_buffer = calloc(rowlen*256*6,1); //an adversarial example keeps the output buffer locked for an entire 128 rows, so we need quite a lot of memory to guarantee no segfaults.
  unsigned char* output_cursor = output_buffer;
  int output_begin = 0;
  int output_len = 0;
  int output_end = rowlen*256*6;
  unsigned char* this_row_buf = calloc(rowlen*3,1);
  unsigned char* next_row_buf = calloc(rowlen*3,1);
  //fprintf(stderr,"output buffer is size %d\n",rowlen*256*6);
  //read the first row into next_row to prime the whole process
  fread(next_row_buf,1,3*rowlen,in);
  int y = 0;
  int last_read = 0;
  while(feof(in) == 0) {
    unsigned char* tmp = this_row_buf; //trick: just swap the bufs each time you read a row
    this_row_buf = next_row_buf;
    next_row_buf = tmp;
    last_read = fread(next_row_buf,1,3*rowlen,in);
    if(last_read == 0) {
      break;
    }
    //encode the different components
    output_cursor = encode_row_red(red_regions,rowlen,output_cursor,this_row_buf,next_row_buf);
    output_cursor = encode_row_green(green_regions,rowlen,output_cursor,this_row_buf,next_row_buf);
    output_cursor = encode_row_blue(blue_regions,rowlen,output_cursor,this_row_buf,next_row_buf);
    //if we can, input some data.
    //fprintf(stderr,"after reading row %d, cursor is %d above buffer\n",y,output_cursor - output_buffer);
    if (output_begin < (output_cursor - output_buffer)) {
      while(output_begin + output_len < (output_cursor - output_buffer)) {
        if((output_buffer[output_begin + output_len] & 0x1) && (output_buffer[output_begin + output_len + 1] & 0x80)) {
          output_buffer[output_begin + output_len + 1] &= 0x7f;
          output_len += 2;
        } else if ((output_buffer[output_begin + output_len] & 0x1) == 0) {
          output_len ++;
        } else {
          break;
        }
      }
    } else {
      while(output_begin + output_len < output_end) {
        if((output_buffer[output_begin + output_len] & 0x1) && (output_buffer[output_begin + output_len + 1] & 0x80)) {
          output_buffer[output_begin + output_len + 1] &= 0x7f;
          output_len += 2;
        } else if ((output_buffer[output_begin + output_len] & 0x1) == 0) {
          output_len ++;
        } else {
          break;
        }
      }
    }
    //fprintf(stderr,"write of size %d\n",output_len);
    if(output_len > 0) {
      gzwrite(out,output_buffer + output_begin,output_len);
      output_begin += output_len;
      output_len = 0;
      //reset output_begin to 0 if we have just finished reading the region left over after resetting the cursor
      if(output_begin == output_end) {
        //fprintf(stderr,"resetting output_begin\n");
        output_begin = 0;
        output_end = rowlen*256*6;
      //reset the cursor to 0 so that there is always rowlen*128 space after the cursor
      //have to make sure this doesn't happen twice in a row, so we restrict to the case when cursor > output_buffer + output_begin
      } else if((output_cursor - output_buffer > output_begin) && (output_begin > rowlen*128*6)) {
        //fprintf(stderr,"resetting cursor\n");
        output_end = output_cursor - output_buffer;
        output_cursor = output_buffer;
      }
      //fprintf(stderr,"cursor at %d above buffer, output_begin at %d\n",output_cursor - output_buffer, output_begin);
    }
    y++;
  }
  //final row
  output_cursor = encode_row_red(red_regions,rowlen,output_cursor,this_row_buf,0);
  output_cursor = encode_row_green(green_regions,rowlen,output_cursor,this_row_buf,0);
  output_cursor = encode_row_blue(blue_regions,rowlen,output_cursor,this_row_buf,0);
  //finalize all regions that didn't get finalized
  for(int i = 0; i < rowlen; i++) {
    if(red_regions[i].is_active) {
      *(red_regions[i].output) = red_regions[i].l;
    }
    if(green_regions[i].is_active) {
      *(green_regions[i].output) = green_regions[i].l;
    }
    if(blue_regions[i].is_active) {
      *(blue_regions[i].output) = blue_regions[i].l;
    }
  }
  //now 100% of our output should be writeable
  if(output_begin > (output_cursor - output_buffer)) {
    //write the extra bit
    for(int i = output_begin; i < output_end;) {
      if (output_buffer[i] & 0x1) {
        output_buffer[i+1] &= 0x7f;
        i += 2;
      } else {
        i++;
      }
    }
    //fprintf(stderr,"write of size %d to reset output_begin\n",output_end - output_begin);
    gzwrite(out,output_buffer + output_begin,output_end - output_begin);
    output_begin = 0;
  }
  //clear the 0x80 markers and write the last bit
  output_len = output_cursor - (output_buffer + output_begin);
  for(int i = output_begin; i < output_len;) {
    if (output_buffer[i] & 0x1) {
      output_buffer[i+1] &= 0x7f;
      i += 2;
    } else {
      i++;
    }
  }
  //fprintf(stderr,"final write of size %d\n",output_len);
  gzwrite(out,output_buffer + output_begin,output_len);
  gzclose(out);
  free(red_regions);
  free(green_regions);
  free(blue_regions);
  free(output_buffer);
  free(this_row_buf);
  free(next_row_buf);
}

int main(int argc, char** argv) {
  FILE* in = stdin;
  gzFile out = gzdopen(fileno(stdout),"w");
  int rowlen = 0;
  if(argc < 2) {
    return 1;
  } else if (argc < 4) {
    rowlen = atoi(argv[1]);
  } else {
    rowlen = atoi(argv[1]);
    in = fopen(argv[2],"r");
    gzclose(out);
    out = gzopen(argv[3],"w");
  }
  gzputc(out,rowlen & 0xff); //output rowlen in little-endian
  gzputc(out,(rowlen >> 8) & 0xff);
  gzputc(out,(rowlen >> 16) & 0xff);
  gzputc(out,rowlen >> 24);
  encode_stream(in,out,rowlen);
  fclose(in);
  //fprintf(stderr,"average bytes per pixel before compression: %f\n",(1.0*avg_area_num)/avg_area_denom);
  //fprintf(stderr,"number of bad regions: %d\n",num_bad_regions);
}
